#include "../include/socket/SocketClient.h"

#include "../include/socket/SocketFrame.h"

#include <chrono>
#include <future>
#include <memory>
#include <utility>

namespace socket_dc {
namespace {

std::string buildSocketEnvelope(const std::string& module,
                               const std::string& type,
                               const lite_json_data::JsonValue& value) {
    return lite_json_data::dumpJsonValue(
        lite_json_data::JsonValue::makeObject({
            {"module", lite_json_data::JsonValue::makeString(module)},
            {"type", lite_json_data::JsonValue::makeString(type)},
            {"value", value}
        }));
}

}  // namespace

SocketClient::SocketClient(std::size_t maxQueuedMessages)
    : socket_(io_),
      workGuard_(asio::make_work_guard(io_)),
      resolver_(io_),
      reconnectTimer_(io_),
      maxQueuedMessages_(maxQueuedMessages) {}

SocketClient::~SocketClient() {
    disconnect();

    {
        std::lock_guard<std::mutex> lock(inboundMutex_);
        stopInboundProcessing_ = true;
    }
    inboundCondition_.notify_all();
    if (callbackWorker_.joinable()) {
        if (isCallbackWorkerThread()) {
            callbackWorker_.detach();
        } else {
            callbackWorker_.join();
        }
    }

    workGuard_.reset();
    io_.stop();
    if (worker_.joinable()) {
        if (isWorkerThread()) {
            worker_.detach();
        } else {
            worker_.join();
        }
    }
}

void SocketClient::setConnectedCallback(ConnectedCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    connectedCallback_ = std::move(callback);
}

void SocketClient::setRawMessageCallback(RawMessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    rawMessageCallback_ = std::move(callback);
}

void SocketClient::setJsonMessageCallback(JsonMessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    jsonMessageCallback_ = std::move(callback);
}

void SocketClient::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = std::move(callback);
}

void SocketClient::setDisconnectCallback(DisconnectCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    disconnectCallback_ = std::move(callback);
}

void SocketClient::setReconnectOptions(ReconnectOptions options) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    reconnectOptions_ = std::move(options);
}

bool SocketClient::connect(const std::string& host, std::uint16_t port) {
    disconnect();

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        host_ = host;
        port_ = port;
        reconnectAttempts_ = 0;
    }
    manualDisconnectRequested_.store(false, std::memory_order_relaxed);
    ensureWorkerRunning();

    auto completion = std::make_shared<std::promise<bool>>();
    auto future = completion->get_future();
    asio::post(io_, [this, host, port, completion]() {
        startConnect(host, port, [completion](bool connected) { completion->set_value(connected); });
    });
    return future.get();
}

bool SocketClient::reconnect() {
    std::string host;
    std::uint16_t port = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        host = host_;
        port = port_;
    }
    if (host.empty() || port == 0) {
        return false;
    }
    return connect(host, port);
}

void SocketClient::disconnect() {
    manualDisconnectRequested_.store(true, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        reconnectAttempts_ = 0;
    }

    if (!worker_.joinable()) {
        const auto wasConnected = connected_.exchange(false, std::memory_order_relaxed);
        resetSocketState();
        if (wasConnected) {
            notifyDisconnected();
        }
        return;
    }

    if (isWorkerThread()) {
        const auto wasConnected = connected_.exchange(false, std::memory_order_relaxed);
        resetSocketState();
        if (wasConnected) {
            notifyDisconnected();
        }
        return;
    }

    auto completion = std::make_shared<std::promise<void>>();
    auto future = completion->get_future();
    asio::post(io_, [this, completion]() {
        const auto wasConnected = connected_.exchange(false, std::memory_order_relaxed);
        resetSocketState();
        if (wasConnected) {
            notifyDisconnected();
        }
        completion->set_value();
    });
    future.get();
}

bool SocketClient::isConnected() const {
    return connected_.load(std::memory_order_relaxed);
}

bool SocketClient::send(const std::string& jsonPayload) {
    if (!isConnected()) {
        return false;
    }

    const auto frame = encodeLengthPrefixedFrame(jsonPayload);

    std::lock_guard<std::mutex> lock(writeMutex_);
    if (outgoing_.size() >= maxQueuedMessages_) {
        return false;
    }

    const auto isWriting = !outgoing_.empty();
    outgoing_.push_back(frame);
    if (!isWriting) {
        asio::post(io_, [this]() { writeNext(); });
    }
    return true;
}

bool SocketClient::sendJson(const lite_json_data::JsonValue& payload) {
    return send(lite_json_data::dumpJsonValue(payload));
}

bool SocketClient::sendEnvelope(const std::string& module,
                                const std::string& type,
                                const lite_json_data::JsonValue& value) {
    return send(buildSocketEnvelope(module, type, value));
}

bool SocketClient::sendClientHello(const std::string& module,
                                   const std::string& connectionId,
                                   const std::string& page,
                                   const lite_json_data::JsonValue& body) {
    const auto value = lite_json_data::JsonValue::makeObject({
        {"connectionId", lite_json_data::JsonValue::makeString(connectionId)},
        {"page", lite_json_data::JsonValue::makeString(page)},
        {"clientTimestampMs", lite_json_data::JsonValue::makeNumber(static_cast<double>(currentTimestampMs()))},
        {"body", body}
    });
    return sendEnvelope(module, "client_hello", value);
}

bool SocketClient::sendClientState(const std::string& module,
                                   const std::string& connectionId,
                                   std::uint64_t sequence,
                                   const std::string& page,
                                   const lite_json_data::JsonValue& body) {
    const auto value = lite_json_data::JsonValue::makeObject({
        {"connectionId", lite_json_data::JsonValue::makeString(connectionId)},
        {"sequence", lite_json_data::JsonValue::makeNumber(static_cast<double>(sequence))},
        {"page", lite_json_data::JsonValue::makeString(page)},
        {"clientTimestampMs", lite_json_data::JsonValue::makeNumber(static_cast<double>(currentTimestampMs()))},
        {"body", body}
    });
    return sendEnvelope(module, "client_state", value);
}

bool SocketClient::sendModuleCommand(const std::string& module,
                                     const std::string& connectionId,
                                     std::uint64_t sequence,
                                     const std::string& page,
                                     const lite_json_data::JsonValue& body) {
    const auto value = lite_json_data::JsonValue::makeObject({
        {"connectionId", lite_json_data::JsonValue::makeString(connectionId)},
        {"sequence", lite_json_data::JsonValue::makeNumber(static_cast<double>(sequence))},
        {"page", lite_json_data::JsonValue::makeString(page)},
        {"clientTimestampMs", lite_json_data::JsonValue::makeNumber(static_cast<double>(currentTimestampMs()))},
        {"body", body}
    });
    return sendEnvelope(module, "module_command", value);
}

void SocketClient::ensureWorkerRunning() {
    if (worker_.joinable()) {
        if (!callbackWorker_.joinable()) {
            callbackWorker_ = std::thread([this]() { processInboundMessages(); });
        }
        return;
    }
    if (!callbackWorker_.joinable()) {
        callbackWorker_ = std::thread([this]() { processInboundMessages(); });
    }
    worker_ = std::thread([this]() { io_.run(); });
}

void SocketClient::startConnect(const std::string& host, std::uint16_t port, ConnectCompletion completion) {
    resetSocketState();
    resolver_.async_resolve(host, std::to_string(port),
                            [this, host, port, completion = std::move(completion)](
                                const asio::error_code& error,
                                const asio::ip::tcp::resolver::results_type& endpoints) mutable {
                                if (error) {
                                    if (error != asio::error::operation_aborted) {
                                        reportError("resolve failed: " + error.message());
                                        scheduleReconnect();
                                    }
                                    if (completion) {
                                        completion(false);
                                    }
                                    return;
                                }

                                asio::async_connect(socket_, endpoints,
                                                    [this, completion = std::move(completion)](
                                                        const asio::error_code& connectError,
                                                        const asio::ip::tcp::endpoint&) mutable {
                                                        if (connectError) {
                                                            if (connectError != asio::error::operation_aborted) {
                                                                reportError("connect failed: " + connectError.message());
                                                                resetSocketState();
                                                                scheduleReconnect();
                                                            }
                                                            if (completion) {
                                                                completion(false);
                                                            }
                                                            return;
                                                        }

                                                        connected_.store(true, std::memory_order_relaxed);
                                                        {
                                                            std::lock_guard<std::mutex> lock(stateMutex_);
                                                            reconnectAttempts_ = 0;
                                                        }
                                                        readHeader();
                                                        notifyConnected();
                                                        if (completion) {
                                                            completion(true);
                                                        }
                                                    });
                            });
}

void SocketClient::scheduleReconnect() {
    std::string host;
    std::uint16_t port = 0;
    ReconnectOptions options;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (manualDisconnectRequested_.load(std::memory_order_relaxed) ||
            !reconnectOptions_.enabled ||
            host_.empty() ||
            port_ == 0) {
            return;
        }
        if (reconnectOptions_.maxAttempts != 0 && reconnectAttempts_ >= reconnectOptions_.maxAttempts) {
            return;
        }
        ++reconnectAttempts_;
        host = host_;
        port = port_;
        options = reconnectOptions_;
    }

    reconnectTimer_.expires_after(options.delay);
    reconnectTimer_.async_wait([this, host, port](const asio::error_code& error) {
        if (error == asio::error::operation_aborted) {
            return;
        }
        startConnect(host, port, ConnectCompletion{});
    });
}

void SocketClient::readHeader() {
    asio::async_read(socket_, asio::buffer(header_), [this](const asio::error_code& error, std::size_t) {
        if (error) {
            handleSocketError(error, "read header failed");
            return;
        }
        readPayload(decodeLengthPrefix(header_.data()));
    });
}

void SocketClient::readPayload(std::uint32_t length) {
    payload_.assign(length, '\0');
    if (length == 0) {
        enqueueInboundPayload(payload_);
        readHeader();
        return;
    }

    asio::async_read(socket_, asio::buffer(payload_), [this](const asio::error_code& error, std::size_t) {
        if (error) {
            handleSocketError(error, "read payload failed");
            return;
        }
        enqueueInboundPayload(payload_);
        readHeader();
    });
}

void SocketClient::writeNext() {
    auto frame = std::make_shared<std::string>();
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        if (outgoing_.empty()) {
            return;
        }
        *frame = outgoing_.front();
    }

    asio::async_write(socket_, asio::buffer(*frame), [this, frame](const asio::error_code& error, std::size_t) {
        if (error) {
            {
                std::lock_guard<std::mutex> lock(writeMutex_);
                outgoing_.clear();
            }
            handleSocketError(error, "write payload failed");
            return;
        }

        bool hasMore = false;
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            if (!outgoing_.empty()) {
                outgoing_.pop_front();
            }
            hasMore = !outgoing_.empty();
        }

        if (hasMore) {
            asio::post(io_, [this]() { writeNext(); });
        }
    });
}

void SocketClient::enqueueInboundPayload(std::string payload) {
    {
        std::lock_guard<std::mutex> lock(inboundMutex_);
        incoming_.push_back(std::move(payload));
    }
    inboundCondition_.notify_one();
}

void SocketClient::processInboundMessages() {
    while (true) {
        std::string payload;
        {
            std::unique_lock<std::mutex> lock(inboundMutex_);
            inboundCondition_.wait(lock, [this]() { return stopInboundProcessing_ || !incoming_.empty(); });
            if (stopInboundProcessing_ && incoming_.empty()) {
                return;
            }
            payload = std::move(incoming_.front());
            incoming_.pop_front();
        }
        deliverPayload(payload);
    }
}

void SocketClient::deliverPayload(const std::string& payload) {
    RawMessageCallback rawCallback;
    JsonMessageCallback jsonCallback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        rawCallback = rawMessageCallback_;
        jsonCallback = jsonMessageCallback_;
    }

    if (rawCallback) {
        rawCallback(payload);
    }

    if (!jsonCallback) {
        return;
    }

    const auto parsed = lite_json_data::parseJsonValue(payload);
    if (parsed.isObject()) {
        jsonCallback(parsed);
    }
}

void SocketClient::notifyConnected() {
    ConnectedCallback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = connectedCallback_;
    }
    if (callback) {
        callback();
    }
}

void SocketClient::handleSocketError(const asio::error_code& error, const char* action) {
    if (error == asio::error::operation_aborted) {
        return;
    }

    reportError(std::string(action) + ": " + error.message());

    const auto wasConnected = connected_.exchange(false, std::memory_order_relaxed);
    resetSocketState();
    if (wasConnected) {
        notifyDisconnected();
    }
    scheduleReconnect();
}

void SocketClient::reportError(const std::string& message) {
    ErrorCallback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = errorCallback_;
    }
    if (callback) {
        callback(message);
    }
}

void SocketClient::notifyDisconnected() {
    DisconnectCallback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = disconnectCallback_;
    }
    if (callback) {
        callback();
    }
}

void SocketClient::resetSocketState() {
    asio::error_code ignored;
    reconnectTimer_.cancel();
    resolver_.cancel();
    if (socket_.is_open()) {
        socket_.cancel(ignored);
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
        socket_.close(ignored);
    }
    socket_ = asio::ip::tcp::socket(io_);
    payload_.clear();
    header_.fill(0);
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        outgoing_.clear();
    }
}

bool SocketClient::isWorkerThread() const {
    return worker_.joinable() && worker_.get_id() == std::this_thread::get_id();
}

bool SocketClient::isCallbackWorkerThread() const {
    return callbackWorker_.joinable() && callbackWorker_.get_id() == std::this_thread::get_id();
}

std::uint64_t SocketClient::currentTimestampMs() {
    const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    return static_cast<std::uint64_t>(now.time_since_epoch().count());
}

}  // namespace socket_dc