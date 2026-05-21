#include <socket/SocketClient.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

void printUsage() {
    std::cout << "usage: redis_socket_client_example [host] [port] [module] [connectionId] [page] [durationSeconds]\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        printUsage();
        return 0;
    }

    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto portValue = argc > 2 ? std::strtoul(argv[2], nullptr, 10) : 9000UL;
    const std::string module = argc > 3 ? argv[3] : "planning";
    const std::string connectionId = argc > 4 ? argv[4] : "conn_main";
    const std::string page = argc > 5 ? argv[5] : module;
    const auto durationSeconds = argc > 6 ? std::strtoul(argv[6], nullptr, 10) : 5UL;

    if (portValue > 65535UL) {
        std::cerr << "invalid port: " << portValue << '\n';
        return 1;
    }

    const auto port = static_cast<std::uint16_t>(portValue);

    redis_dc::SocketClient client;
    std::atomic<std::uint64_t> sequence{1};

    client.setReconnectOptions({true, std::chrono::milliseconds(1000), 0});
    client.setConnectedCallback([&]() {
        std::cout << "connected to " << host << ':' << port << '\n';

        const auto helloSequence = sequence.fetch_add(1);
        const auto helloBody = redis_dc::lite_json_data::JsonValue::makeObject({
            {"clientName", redis_dc::lite_json_data::JsonValue::makeString("redis-socket-client-example")},
            {"module", redis_dc::lite_json_data::JsonValue::makeString(module)},
            {"supportedPages", redis_dc::lite_json_data::JsonValue::makeArray({
                redis_dc::lite_json_data::JsonValue::makeString(page),
                redis_dc::lite_json_data::JsonValue::makeString("Status"),
                redis_dc::lite_json_data::JsonValue::makeString("Command")
            })}
        });
        client.sendClientHello(module, connectionId, page, helloBody);

        const auto stateBody = redis_dc::lite_json_data::JsonValue::makeObject({
            {"page", redis_dc::lite_json_data::JsonValue::makeString(page)},
            {"status", redis_dc::lite_json_data::JsonValue::makeString("connected")},
            {"helloSequence", redis_dc::lite_json_data::JsonValue::makeNumber(static_cast<double>(helloSequence))}
        });
        client.sendClientState(module, connectionId, sequence.fetch_add(1), page, stateBody);
    });
    client.setDisconnectCallback([&]() {
        std::cout << "disconnected from server\n";
    });
    client.setErrorCallback([&](const std::string& message) {
        std::cerr << "socket error: " << message << '\n';
    });
    client.setJsonMessageCallback([&](const redis_dc::lite_json_data::JsonValue& payload) {
        std::cout << "recv json: " << redis_dc::lite_json_data::dumpJsonValue(payload) << '\n';
    });
    client.setRawMessageCallback([&](const std::string& payload) {
        std::cout << "recv raw: " << payload << '\n';
    });

    if (!client.connect(host, port)) {
        std::cerr << "initial connect failed, waiting for auto reconnect\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));

    if (client.isConnected()) {
        const auto commandBody = redis_dc::lite_json_data::JsonValue::makeObject({
            {"command", redis_dc::lite_json_data::JsonValue::makeString("example_ping")},
            {"source", redis_dc::lite_json_data::JsonValue::makeString("redis_socket_client_example")},
            {"page", redis_dc::lite_json_data::JsonValue::makeString(page)}
        });
        if (!client.sendModuleCommand(module, connectionId, sequence.fetch_add(1), page, commandBody)) {
            std::cerr << "failed to send module command\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    client.disconnect();
    return 0;
}