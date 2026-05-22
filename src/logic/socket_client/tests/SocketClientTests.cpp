#include <catch2/catch_test_macros.hpp>
#include <socket/SocketClient.h>

#include "support/SocketClientTestSupport.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

template <typename Predicate>
bool waitFor(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

std::string buildEnvelope(std::uint64_t sequence, const socket_dc::lite_json_data::JsonValue& body) {
    return socket_dc::lite_json_data::dumpJsonValue(
        socket_dc::lite_json_data::JsonValue::makeObject({
            {"module", socket_dc::lite_json_data::JsonValue::makeString("planning")},
            {"type", socket_dc::lite_json_data::JsonValue::makeString("module_event")},
            {"value", socket_dc::lite_json_data::JsonValue::makeObject({
                {"connectionId", socket_dc::lite_json_data::JsonValue::makeString("conn_main")},
                {"sequence", socket_dc::lite_json_data::JsonValue::makeNumber(static_cast<double>(sequence))},
                {"values", body}
            })}
        }));
}

std::string buildLargeModelPayload() {
    constexpr std::size_t kMeshCount = 10000;
    constexpr std::size_t kPointsPerMesh = 15;

    std::string payload;
    payload.reserve(12 * 1024 * 1024);
    payload += "{\"module\":\"planning\",\"type\":\"module_event\",\"value\":{\"connectionId\":\"conn_main\",\"sequence\":9001,\"values\":{\"model\":{\"meshes\":[";
    for (std::size_t meshIndex = 0; meshIndex < kMeshCount; ++meshIndex) {
        if (meshIndex != 0) {
            payload.push_back(',');
        }
        payload += "{\"id\":\"mesh-";
        payload += std::to_string(meshIndex);
        payload += "\",\"points\":[";
        for (std::size_t pointIndex = 0; pointIndex < kPointsPerMesh; ++pointIndex) {
            if (pointIndex != 0) {
                payload.push_back(',');
            }
            const auto valueIndex = meshIndex * kPointsPerMesh + pointIndex;
            payload.push_back('[');
            payload += std::to_string(valueIndex);
            payload.push_back(',');
            payload += std::to_string(valueIndex + 1);
            payload.push_back(',');
            payload += std::to_string(valueIndex + 2);
            payload.push_back(']');
        }
        payload += "]}";
    }
    payload += "]}}}}";
    return payload;
}

std::unique_ptr<socket_dc::socket_client_tests::SocketTestServer> makeAckServer(std::uint16_t port) {
    auto server = std::make_unique<socket_dc::socket_client_tests::SocketTestServer>(port);
    auto* serverPtr = server.get();
    server->setMessageCallback([serverPtr](const socket_dc::socket_client_tests::SocketMessage& message) {
        serverPtr->sendTo(message.clientId,
                          socket_dc::socket_client_tests::buildSocketAckEnvelope(
                              message, message.targetModule, true, "accepted"));
    });
    server->run(1);
    return server;
}

}  // namespace

TEST_CASE("socket client sends module command and receives ack") {
    socket_dc::socket_client_tests::SocketTestServer server(0);
    std::mutex stateMutex;
    std::optional<socket_dc::socket_client_tests::SocketMessage> receivedMessage;
    std::optional<socket_dc::lite_json_data::JsonValue> receivedAck;

    server.setMessageCallback([&](const socket_dc::socket_client_tests::SocketMessage& message) {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            receivedMessage = message;
        }
        server.sendTo(message.clientId,
                      socket_dc::socket_client_tests::buildSocketAckEnvelope(
                          message, message.targetModule, true, "accepted"));
    });
    server.run(1);

    socket_dc::SocketClient client;
    client.setJsonMessageCallback([&](const socket_dc::lite_json_data::JsonValue& payload) {
        std::lock_guard<std::mutex> lock(stateMutex);
        receivedAck = payload;
    });

    REQUIRE(client.connect("127.0.0.1", server.port()));
    REQUIRE(client.sendModuleCommand("planning",
                                     "conn_main",
                                     42,
                                     "Planning",
                                     socket_dc::lite_json_data::JsonValue::makeObject({
                                         {"command", socket_dc::lite_json_data::JsonValue::makeString("replan")},
                                         {"priority", socket_dc::lite_json_data::JsonValue::makeNumber(7)}
                                     })));

    REQUIRE(waitFor([&]() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return receivedMessage.has_value();
    }));

    REQUIRE(waitFor([&]() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return receivedAck.has_value();
    }));

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        REQUIRE(receivedMessage.has_value());
        REQUIRE(receivedMessage->type == "module_command");
        REQUIRE(receivedMessage->targetModule == "planning");
        REQUIRE(receivedMessage->connectionId == "conn_main");
        REQUIRE(receivedMessage->sequence == 42);
        REQUIRE(receivedMessage->page == "Planning");
        REQUIRE(socket_dc::lite_json_data::getString(receivedMessage->body, "command") == "replan");

        REQUIRE(receivedAck.has_value());
        REQUIRE(socket_dc::lite_json_data::getString(*receivedAck, "module") == "planning");
        REQUIRE(socket_dc::lite_json_data::getString(*receivedAck, "type") == "socket_ack");
        REQUIRE(socket_dc::lite_json_data::getString(*receivedAck, "value.requestType") == "module_command");
        REQUIRE(socket_dc::lite_json_data::getBool(*receivedAck, "value.accepted"));
    }

    client.disconnect();
    server.stop();
}

TEST_CASE("socket client exposes raw payload callback for non json frames") {
    socket_dc::socket_client_tests::SocketTestServer server(0);
    server.run(1);

    std::mutex stateMutex;
    std::optional<std::string> rawPayload;
    bool jsonCallbackCalled = false;

    socket_dc::SocketClient client;
    client.setRawMessageCallback([&](const std::string& payload) {
        std::lock_guard<std::mutex> lock(stateMutex);
        rawPayload = payload;
    });
    client.setJsonMessageCallback([&](const socket_dc::lite_json_data::JsonValue&) {
        std::lock_guard<std::mutex> lock(stateMutex);
        jsonCallbackCalled = true;
    });

    REQUIRE(client.connect("127.0.0.1", server.port()));

    REQUIRE(waitFor([&]() {
        server.broadcast("not-json");
        std::lock_guard<std::mutex> lock(stateMutex);
        return rawPayload.has_value();
    }));

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        REQUIRE(rawPayload.has_value());
        REQUIRE(*rawPayload == "not-json");
        REQUIRE_FALSE(jsonCallbackCalled);
    }

    client.disconnect();
    server.stop();
}

TEST_CASE("socket client reconnects with saved endpoint") {
    auto server = makeAckServer(0);
    const auto port = server->port();

    std::mutex stateMutex;
    std::optional<std::uint64_t> lastAckSequence;
    std::atomic<int> connectedCount{0};
    std::atomic<int> disconnectedCount{0};

    socket_dc::SocketClient client;
    client.setConnectedCallback([&]() { connectedCount.fetch_add(1); });
    client.setDisconnectCallback([&]() { disconnectedCount.fetch_add(1); });
    client.setJsonMessageCallback([&](const socket_dc::lite_json_data::JsonValue& payload) {
        if (socket_dc::lite_json_data::getString(payload, "type") != "socket_ack") {
            return;
        }
        std::lock_guard<std::mutex> lock(stateMutex);
        lastAckSequence = static_cast<std::uint64_t>(socket_dc::lite_json_data::getDouble(payload, "value.sequence"));
    });

    REQUIRE(client.connect("127.0.0.1", port));
    REQUIRE(waitFor([&]() { return connectedCount.load() >= 1; }));
    REQUIRE(client.sendModuleCommand("planning",
                                     "conn_main",
                                     42,
                                     "Planning",
                                     socket_dc::lite_json_data::JsonValue::makeObject({
                                         {"command", socket_dc::lite_json_data::JsonValue::makeString("replan")}
                                     })));
    REQUIRE(waitFor([&]() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return lastAckSequence.has_value() && *lastAckSequence == 42;
    }));

    server->stop();
    server.reset();
    REQUIRE(waitFor([&]() { return disconnectedCount.load() >= 1; }));

    std::unique_ptr<socket_dc::socket_client_tests::SocketTestServer> restartedServer;
    REQUIRE(waitFor([&]() {
        if (restartedServer) {
            return true;
        }
        try {
            restartedServer = makeAckServer(port);
            return true;
        } catch (...) {
            return false;
        }
    }));

    REQUIRE(client.reconnect());
    REQUIRE(waitFor([&]() { return connectedCount.load() >= 2 && client.isConnected(); }));
    REQUIRE(client.sendModuleCommand("planning",
                                     "conn_main",
                                     43,
                                     "Planning",
                                     socket_dc::lite_json_data::JsonValue::makeObject({
                                         {"command", socket_dc::lite_json_data::JsonValue::makeString("resume")}
                                     })));
    REQUIRE(waitFor([&]() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return lastAckSequence.has_value() && *lastAckSequence == 43;
    }));

    client.disconnect();
    restartedServer->stop();
}

TEST_CASE("socket client reconnects automatically after server restart") {
    auto server = makeAckServer(0);
    const auto port = server->port();

    std::mutex stateMutex;
    std::optional<std::uint64_t> lastAckSequence;
    std::atomic<int> connectedCount{0};
    std::atomic<int> disconnectedCount{0};

    socket_dc::SocketClient client;
    client.setReconnectOptions({true, std::chrono::milliseconds(50), 20});
    client.setConnectedCallback([&]() { connectedCount.fetch_add(1); });
    client.setDisconnectCallback([&]() { disconnectedCount.fetch_add(1); });
    client.setJsonMessageCallback([&](const socket_dc::lite_json_data::JsonValue& payload) {
        if (socket_dc::lite_json_data::getString(payload, "type") != "socket_ack") {
            return;
        }
        std::lock_guard<std::mutex> lock(stateMutex);
        lastAckSequence = static_cast<std::uint64_t>(socket_dc::lite_json_data::getDouble(payload, "value.sequence"));
    });

    REQUIRE(client.connect("127.0.0.1", port));
    REQUIRE(waitFor([&]() { return connectedCount.load() >= 1; }));

    server->stop();
    server.reset();
    REQUIRE(waitFor([&]() { return disconnectedCount.load() >= 1; }));

    std::unique_ptr<socket_dc::socket_client_tests::SocketTestServer> restartedServer;
    REQUIRE(waitFor([&]() {
        if (restartedServer) {
            return true;
        }
        try {
            restartedServer = makeAckServer(port);
            return true;
        } catch (...) {
            return false;
        }
    }));

    REQUIRE(waitFor([&]() { return connectedCount.load() >= 2 && client.isConnected(); }));
    REQUIRE(client.sendModuleCommand("planning",
                                     "conn_main",
                                     44,
                                     "Planning",
                                     socket_dc::lite_json_data::JsonValue::makeObject({
                                         {"command", socket_dc::lite_json_data::JsonValue::makeString("auto-rejoin")}
                                     })));
    REQUIRE(waitFor([&]() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return lastAckSequence.has_value() && *lastAckSequence == 44;
    }));

    client.disconnect();
    restartedServer->stop();
}

TEST_CASE("socket client transports large model payloads without truncation") {
    using namespace std::chrono_literals;

    socket_dc::socket_client_tests::SocketTestServer server(0);
    server.run(1);

    std::mutex stateMutex;
    std::optional<std::string> receivedPayload;

    socket_dc::SocketClient client;
    client.setRawMessageCallback([&](const std::string& payload) {
        std::lock_guard<std::mutex> lock(stateMutex);
        receivedPayload = payload;
    });

    REQUIRE(client.connect("127.0.0.1", server.port()));

    const auto payload = buildLargeModelPayload();
    REQUIRE(server.broadcast(payload));

    REQUIRE(waitFor([&]() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return receivedPayload.has_value();
    }, 20000ms));

    std::string payloadCopy;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        REQUIRE(receivedPayload.has_value());
        payloadCopy = *receivedPayload;
    }

    REQUIRE(payloadCopy.size() == payload.size());

    const auto document = socket_dc::lite_json_data::parseJsonValue(payloadCopy);
    REQUIRE(socket_dc::lite_json_data::getString(document, "module") == "planning");
    REQUIRE(socket_dc::lite_json_data::getDouble(document, "value.sequence") == 9001.0);

    const auto* meshes = socket_dc::lite_json_data::detail::resolvePath(document, "value.values.model.meshes");
    REQUIRE(meshes != nullptr);
    REQUIRE(meshes->isArray());
    REQUIRE(meshes->arrayValues.size() == 10000);

    std::size_t totalPoints = 0;
    for (const auto& mesh : meshes->arrayValues) {
        const auto* points = socket_dc::lite_json_data::detail::resolvePath(mesh, "points");
        REQUIRE(points != nullptr);
        REQUIRE(points->isArray());
        totalPoints += points->arrayValues.size();
    }
    REQUIRE(totalPoints == 150000);

    client.disconnect();
    server.stop();
}

TEST_CASE("socket client processes inbound messages sequentially without skipping") {
    using namespace std::chrono_literals;

    socket_dc::socket_client_tests::SocketTestServer server(0);
    server.run(1);

    std::mutex stateMutex;
    std::vector<int> processedSequence;
    int activeCallbacks = 0;
    int maxConcurrentCallbacks = 0;

    socket_dc::SocketClient client;
    client.setJsonMessageCallback([&](const socket_dc::lite_json_data::JsonValue& payload) {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            ++activeCallbacks;
            maxConcurrentCallbacks = std::max(maxConcurrentCallbacks, activeCallbacks);
            processedSequence.push_back(static_cast<int>(socket_dc::lite_json_data::getDouble(payload, "value.sequence")));
        }

        if (socket_dc::lite_json_data::getDouble(payload, "value.sequence") == 1.0) {
            std::this_thread::sleep_for(150ms);
        }

        std::lock_guard<std::mutex> lock(stateMutex);
        --activeCallbacks;
    });

    REQUIRE(client.connect("127.0.0.1", server.port()));

    std::thread sender([&]() {
        server.broadcast(buildEnvelope(1, socket_dc::lite_json_data::JsonValue::makeObject({
            {"message", socket_dc::lite_json_data::JsonValue::makeString("first")}
        })));
        server.broadcast(buildEnvelope(2, socket_dc::lite_json_data::JsonValue::makeObject({
            {"message", socket_dc::lite_json_data::JsonValue::makeString("second")}
        })));
        server.broadcast(buildEnvelope(3, socket_dc::lite_json_data::JsonValue::makeObject({
            {"message", socket_dc::lite_json_data::JsonValue::makeString("third")}
        })));
    });
    sender.join();

    REQUIRE(waitFor([&]() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return processedSequence.size() == 3;
    }, 10000ms));

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        REQUIRE(processedSequence == std::vector<int>{1, 2, 3});
        REQUIRE(maxConcurrentCallbacks == 1);
    }

    client.disconnect();
    server.stop();
}