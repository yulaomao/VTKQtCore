#pragma once

#include "../../include/core/LiteJsonDataParser.h"
#include "../../include/socket/SocketFrame.h"

#include <asio.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace socket_dc::socket_client_tests {

using SocketClientId = std::uint64_t;

struct SocketMessage {
    SocketClientId clientId = 0;
    std::string type = "raw";
    std::string targetModule;
    std::string connectionId = "conn_main";
    std::uint64_t sequence = 0;
    std::uint64_t clientTimestampMs = 0;
    std::string page;
    lite_json_data::JsonValue body = lite_json_data::JsonValue::makeObject();
    std::string payload;
};

inline SocketMessage parseSocketPayload(SocketClientId clientId, const std::string& payload) {
    SocketMessage message;
    message.clientId = clientId;
    message.payload = payload;

    const auto parsed = lite_json_data::parseJsonValue(payload);
    if (!parsed.isObject()) {
        return message;
    }

    const auto* value = lite_json_data::detail::resolvePath(parsed, "value");
    const auto module = lite_json_data::getString(parsed, "module");
    const auto type = lite_json_data::getString(parsed, "type");
    if (module.empty() || type.empty() || value == nullptr || !value->isObject()) {
        return message;
    }

    message.type = type;
    message.targetModule = module;
    message.connectionId = lite_json_data::getString(parsed, "value.connectionId", "conn_main");
    message.sequence = static_cast<std::uint64_t>(lite_json_data::getDouble(parsed, "value.sequence", 0.0));
    message.clientTimestampMs = static_cast<std::uint64_t>(
        lite_json_data::getDouble(parsed, "value.clientTimestampMs", 0.0));
    message.page = lite_json_data::getString(parsed, "value.page");

    const auto* body = lite_json_data::detail::resolvePath(parsed, "value.body");
    if (body != nullptr) {
        message.body = *body;
    }

    return message;
}

inline std::string buildSocketAckEnvelope(const SocketMessage& message,
                                          const std::string& module,
                                          bool accepted,
                                          const std::string& detail) {
    return lite_json_data::dumpJsonValue(
        lite_json_data::JsonValue::makeObject({
            {"module", lite_json_data::JsonValue::makeString(module)},
            {"type", lite_json_data::JsonValue::makeString("socket_ack")},
            {"value", lite_json_data::JsonValue::makeObject({
                {"clientId", lite_json_data::JsonValue::makeNumber(static_cast<double>(message.clientId))},
                {"connectionId", lite_json_data::JsonValue::makeString(message.connectionId)},
                {"requestType", lite_json_data::JsonValue::makeString(message.type)},
                {"sequence", lite_json_data::JsonValue::makeNumber(static_cast<double>(message.sequence))},
                {"accepted", lite_json_data::JsonValue::makeBoolean(accepted)},
                {"detail", lite_json_data::JsonValue::makeString(detail)}
            })}
        }));
}

class SocketTestServer {
public:
    using MessageCallback = std::function<void(const SocketMessage&)>;

    explicit SocketTestServer(std::uint16_t port, std::size_t maxQueuedMessages = 1024)
        : acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
          maxQueuedMessages_(maxQueuedMessages) {}

    ~SocketTestServer() {
        stop();
    }

    void setMessageCallback(MessageCallback callback) {
        messageCallback_ = std::move(callback);
    }

    void run(std::size_t threadCount = 1) {
        doAccept();
        const auto count = threadCount == 0 ? 1 : threadCount;
        for (std::size_t index = 0; index < count; ++index) {
            threads_.emplace_back([this]() { io_.run(); });
        }
    }

    void stop() {
        asio::error_code ignored;
        acceptor_.close(ignored);
        io_.stop();
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads_.clear();
    }

    std::uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

    bool sendTo(SocketClientId clientId, const std::string& jsonPayload) {
        std::shared_ptr<ClientSession> session;
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            const auto found = clients_.find(clientId);
            if (found == clients_.end()) {
                return false;
            }
            session = found->second;
        }
        return session->enqueue(jsonPayload);
    }

    bool broadcast(const std::string& jsonPayload) {
        std::vector<std::shared_ptr<ClientSession>> sessions;
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            sessions.reserve(clients_.size());
            for (const auto& pair : clients_) {
                sessions.push_back(pair.second);
            }
        }
        bool allQueued = true;
        for (const auto& session : sessions) {
            allQueued = session->enqueue(jsonPayload) && allQueued;
        }
        return allQueued;
    }

private:
    class ClientSession : public std::enable_shared_from_this<ClientSession> {
    public:
        ClientSession(SocketTestServer& server,
                      asio::ip::tcp::socket socket,
                      SocketClientId id,
                      std::size_t maxQueuedMessages)
            : server_(server),
              socket_(std::move(socket)),
              id_(id),
              maxQueuedMessages_(maxQueuedMessages) {}

        void start() {
            readHeader();
        }

        bool enqueue(const std::string& payload) {
            std::lock_guard<std::mutex> lock(writeMutex_);
            if (outgoing_.size() >= maxQueuedMessages_) {
                return false;
            }
            const auto writing = !outgoing_.empty();
            outgoing_.push_back(encodeLengthPrefixedFrame(payload));
            if (!writing) {
                asio::post(socket_.get_executor(), [self = this->shared_from_this()]() { self->writeNext(); });
            }
            return true;
        }

    private:
        void readHeader() {
            auto self = this->shared_from_this();
            asio::async_read(socket_, asio::buffer(header_), [this, self](const asio::error_code& error, std::size_t) {
                if (error) {
                    server_.removeClient(id_);
                    return;
                }
                const auto length = decodeLengthPrefix(header_.data());
                payload_.assign(length, '\0');
                readPayload();
            });
        }

        void readPayload() {
            auto self = this->shared_from_this();
            asio::async_read(socket_, asio::buffer(payload_), [this, self](const asio::error_code& error, std::size_t) {
                if (error) {
                    server_.removeClient(id_);
                    return;
                }
                if (server_.messageCallback_) {
                    server_.messageCallback_(parseSocketPayload(id_, payload_));
                }
                readHeader();
            });
        }

        void writeNext() {
            std::lock_guard<std::mutex> lock(writeMutex_);
            if (outgoing_.empty()) {
                return;
            }
            auto self = this->shared_from_this();
            asio::async_write(socket_, asio::buffer(outgoing_.front()),
                              [this, self](const asio::error_code& error, std::size_t) {
                                  std::lock_guard<std::mutex> innerLock(writeMutex_);
                                  if (error) {
                                      outgoing_.clear();
                                      server_.removeClient(id_);
                                      return;
                                  }
                                  outgoing_.pop_front();
                                  if (!outgoing_.empty()) {
                                      asio::post(socket_.get_executor(), [self]() { self->writeNext(); });
                                  }
                              });
        }

        SocketTestServer& server_;
        asio::ip::tcp::socket socket_;
        SocketClientId id_;
        std::size_t maxQueuedMessages_;
        std::array<unsigned char, 4> header_{};
        std::string payload_;
        std::deque<std::string> outgoing_;
        std::mutex writeMutex_;
    };

    void doAccept() {
        acceptor_.async_accept([this](const asio::error_code& error, asio::ip::tcp::socket socket) {
            if (!error) {
                const auto id = nextClientId_.fetch_add(1);
                auto session = std::make_shared<ClientSession>(*this, std::move(socket), id, maxQueuedMessages_);
                {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    clients_[id] = session;
                }
                session->start();
            }
            if (acceptor_.is_open()) {
                doAccept();
            }
        });
    }

    void removeClient(SocketClientId clientId) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(clientId);
    }

    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    std::size_t maxQueuedMessages_;
    std::atomic<SocketClientId> nextClientId_{1};
    std::unordered_map<SocketClientId, std::shared_ptr<ClientSession>> clients_;
    mutable std::mutex clientsMutex_;
    std::vector<std::thread> threads_;
    MessageCallback messageCallback_;
};

}  // namespace socket_dc::socket_client_tests
