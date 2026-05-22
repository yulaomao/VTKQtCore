# Socket Client

Standalone C++17 socket client library used by VTKQtCore.

## Build In Parent Project

```cmake
set(SOCKET_CLIENT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SOCKET_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(src/logic/socket_client)

target_link_libraries(your_app PRIVATE socket_dc::socket_client)
```

## Standalone Build

```powershell
cmake -S src/logic/socket_client -B build_socket_client -DSOCKET_CLIENT_BUILD_EXAMPLE=ON
cmake --build build_socket_client --config Debug --target socket_client_example
```

If Asio is not discoverable, set `SOCKET_CLIENT_ASIO_INCLUDE_DIR` to the directory that contains `asio.hpp`.

## Public API

```cpp
socket_dc::SocketClient client;
client.setJsonMessageCallback([](const socket_dc::lite_json_data::JsonValue& payload) {
    // handle inbound JSON payload
});
client.connect("127.0.0.1", 9000);
```

The library exposes the CMake alias `socket_dc::socket_client` and the concrete target `socket_client_core`.

## Notes

- The parent application disables examples and tests by default.
- The library is transport-only; application-level routing lives in `CommunicationHub` and `LegacySocketAdapter`.
- JSON helpers live under `socket_dc::lite_json_data`.
