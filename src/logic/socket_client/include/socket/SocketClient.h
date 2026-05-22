#pragma once

#include "../core/LiteJsonDataParser.h"

#include <asio.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace socket_dc {

class SocketClient {
public:
    struct ReconnectOptions {
        bool enabled = false;
        std::chrono::milliseconds delay{1000};
        std::size_t maxAttempts = 0;
    };

    using ConnectedCallback = std::function<void()>;
    using RawMessageCallback = std::function<void(const std::string&)>;
    using JsonMessageCallback = std::function<void(const lite_json_data::JsonValue&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using DisconnectCallback = std::function<void()>;

    explicit SocketClient(std::size_t maxQueuedMessages = 1024);
    ~SocketClient();

    void setConnectedCallback(ConnectedCallback callback);
    void setRawMessageCallback(RawMessageCallback callback);
    void setJsonMessageCallback(JsonMessageCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setDisconnectCallback(DisconnectCallback callback);
    void setReconnectOptions(ReconnectOptions options);

    bool connect(const std::string& host, std::uint16_t port);
    bool reconnect();
    void disconnect();
    bool isConnected() const;

    bool send(const std::string& jsonPayload);
    bool sendJson(const lite_json_data::JsonValue& payload);
    bool sendEnvelope(const std::string& module, const std::string& type, const lite_json_data::JsonValue& value);
    bool sendClientHello(const std::string& module,
                         const std::string& connectionId,
                         const std::string& page,
                         const lite_json_data::JsonValue& body = lite_json_data::JsonValue::makeObject());
    bool sendClientState(const std::string& module,
                         const std::string& connectionId,
                         std::uint64_t sequence,
                         const std::string& page,
                         const lite_json_data::JsonValue& body);
    bool sendModuleCommand(const std::string& module,
                           const std::string& connectionId,
                           std::uint64_t sequence,
                           const std::string& page,
                           const lite_json_data::JsonValue& body);

private:
    using ConnectCompletion = std::function<void(bool)>;

    void ensureWorkerRunning();
    void startConnect(const std::string& host, std::uint16_t port, ConnectCompletion completion);
    void scheduleReconnect();
    void readHeader();
    void readPayload(std::uint32_t length);
    void writeNext();
    void enqueueInboundPayload(std::string payload);
    void processInboundMessages();
    void deliverPayload(const std::string& payload);
    void notifyConnected();
    void handleSocketError(const asio::error_code& error, const char* action);
    void reportError(const std::string& message);
    void notifyDisconnected();
    void resetSocketState();
    bool isWorkerThread() const;
    bool isCallbackWorkerThread() const;
    static std::uint64_t currentTimestampMs();

    asio::io_context io_;
    asio::ip::tcp::socket socket_;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
    asio::ip::tcp::resolver resolver_;
    asio::steady_timer reconnectTimer_;
    std::size_t maxQueuedMessages_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> manualDisconnectRequested_{true};
    std::array<unsigned char, 4> header_{};
    std::string payload_;
    std::deque<std::string> outgoing_;
    std::deque<std::string> incoming_;
    mutable std::mutex stateMutex_;
    mutable std::mutex writeMutex_;
    mutable std::mutex inboundMutex_;
    mutable std::mutex callbackMutex_;
    std::condition_variable inboundCondition_;
    std::thread worker_;
    std::thread callbackWorker_;
    std::string host_;
    std::uint16_t port_ = 0;
    std::size_t reconnectAttempts_ = 0;
    bool stopInboundProcessing_ = false;
    ReconnectOptions reconnectOptions_;
    ConnectedCallback connectedCallback_;
    RawMessageCallback rawMessageCallback_;
    JsonMessageCallback jsonMessageCallback_;
    ErrorCallback errorCallback_;
    DisconnectCallback disconnectCallback_;
};

}  // namespace socket_dc