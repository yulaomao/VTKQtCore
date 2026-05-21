# 纯 C++ Socket 客户端

clients/socket_client 是可独立移植的非 Qt Socket 客户端子模块。协议为 4 字节大端长度前缀加 JSON payload，JSON envelope 统一为 `{"module":"...","type":"...","value":{...}}`。

## 构建

作为主项目子目录构建：

```powershell
cmake --build build_modules_validate --config Debug --target redis_socket_client_core
cmake --build build_modules_validate --config Debug --target redis_socket_client_example
```

作为独立项目构建：

```powershell
cmake -S clients/socket_client -B build_socket_client_standalone -DREDIS_SOCKET_CLIENT_BUILD_EXAMPLE=ON
cmake --build build_socket_client_standalone --config Debug --target redis_socket_client_example
```

独立构建时需要可找到 Asio 头文件；如果不在常见位置，可设置 `REDIS_SOCKET_CLIENT_ASIO_INCLUDE_DIR`。

## 暴露的 CMake target

子模块会导出两个可链接的 target 名：

- 推荐：`redis_dc::socket_client`
- 兼容：`redis_socket_client_core`

这个 target 会自动传播以下依赖和编译要求：

- `include` 目录
- `Threads::Threads`
- Windows 下的 `ws2_32`、`mswsock`
- C++17 编译要求

如果父项目里已经提供了 `asio::asio` target，这个子模块会直接复用；否则它会尝试从常见路径或 `REDIS_SOCKET_CLIENT_ASIO_INCLUDE_DIR` 查找 Asio 头文件。

## 在别的项目中接入

最简单的方式是把 `clients/socket_client` 当成 vendored 子目录，通过 `add_subdirectory(...)` 引入。

### 方式一：直接作为子目录接入

假设目录结构如下：

```text
your_app/
    CMakeLists.txt
    src/
    third_party/
        logic/
            clients/
                socket_client/
```

你的 `CMakeLists.txt` 可以这样写：

```cmake
cmake_minimum_required(VERSION 3.24)
project(your_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REDIS_SOCKET_CLIENT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(REDIS_SOCKET_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/logic/clients/socket_client
                                 ${CMAKE_BINARY_DIR}/_deps/redis_socket_client)

add_executable(your_app
    src/main.cpp
)

target_link_libraries(your_app
    PRIVATE redis_dc::socket_client
)
```

业务代码里直接包含：

```cpp
#include <socket/SocketClient.h>
```

### 方式二：单独拷贝这个子模块使用

如果你只拷贝了 `clients/socket_client` 目录，没有把主项目根目录下的 Asio 一起带过去，建议在 `add_subdirectory(...)` 之前显式指定 Asio 头文件位置：

```cmake
set(REDIS_SOCKET_CLIENT_ASIO_INCLUDE_DIR "D:/deps/asio/include" CACHE PATH "")
set(REDIS_SOCKET_CLIENT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(REDIS_SOCKET_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/socket_client
                                 ${CMAKE_BINARY_DIR}/_deps/redis_socket_client)

add_executable(your_app src/main.cpp)
target_link_libraries(your_app PRIVATE redis_dc::socket_client)
```

### 方式三：父项目已经有自己的 `asio::asio`

如果你的父项目已经这样定义过：

```cmake
add_library(asio::asio INTERFACE IMPORTED)
set_target_properties(asio::asio PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "D:/deps/asio/include"
)
target_compile_definitions(asio::asio INTERFACE ASIO_STANDALONE)
```

那么 `socket_client` 子模块不会再重复查找 Asio，你只需要：

```cmake
set(REDIS_SOCKET_CLIENT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(REDIS_SOCKET_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/logic/clients/socket_client
                                 ${CMAKE_BINARY_DIR}/_deps/redis_socket_client)

target_link_libraries(your_app PRIVATE redis_dc::socket_client)
```

### 方式四：直接复制源码文件到目标项目

如果你不想保留 `socket_client` 这个子目录，也可以把最小必需源码直接拷贝到自己的项目里。

最小必需文件如下：

- `clients/socket_client/include/socket/SocketClient.h`
- `clients/socket_client/include/core/LiteJsonDataParser.h`
- `clients/socket_client/src/SocketClient.cpp`

推荐在目标项目里保留下面这种相对目录结构：

```text
your_app/
    CMakeLists.txt
    src/
    third_party/
        redis_socket_client/
            include/
                core/
                    LiteJsonDataParser.h
                socket/
                    SocketClient.h
            src/
                SocketClient.cpp
```

之所以建议保留这层结构，是因为 `SocketClient.h` 当前使用了相对包含：

```cpp
#include "../core/LiteJsonDataParser.h"
```

如果你把文件打平到别的位置，也可以用，但需要同步修改这个 include 路径。

对应的 `CMakeLists.txt` 可以这样写：

```cmake
cmake_minimum_required(VERSION 3.24)
project(your_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(asio::asio INTERFACE IMPORTED)
set_target_properties(asio::asio PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "D:/deps/asio/include"
)
target_compile_definitions(asio::asio INTERFACE ASIO_STANDALONE)

add_library(redis_socket_client_local STATIC
    third_party/redis_socket_client/src/SocketClient.cpp
)

target_include_directories(redis_socket_client_local
    PUBLIC third_party/redis_socket_client/include
)

target_link_libraries(redis_socket_client_local
    PUBLIC asio::asio Threads::Threads
)

target_compile_features(redis_socket_client_local PUBLIC cxx_std_17)

if(WIN32)
    target_compile_definitions(redis_socket_client_local PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
    target_link_libraries(redis_socket_client_local PUBLIC ws2_32 mswsock)
endif()

add_executable(your_app src/main.cpp)
target_link_libraries(your_app PRIVATE redis_socket_client_local)
```

如果你不想单独建一个库，也可以把 `SocketClient.cpp` 直接加到自己的可执行目标里：

```cmake
add_executable(your_app
    src/main.cpp
    third_party/redis_socket_client/src/SocketClient.cpp
)

target_include_directories(your_app PRIVATE third_party/redis_socket_client/include)
target_link_libraries(your_app PRIVATE asio::asio Threads::Threads)
target_compile_features(your_app PRIVATE cxx_std_17)

if(WIN32)
    target_compile_definitions(your_app PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
    target_link_libraries(your_app PRIVATE ws2_32 mswsock)
endif()
```

这种方式适合：

- 你只想摘取 `SocketClient` 的少量源码，不想保留整个子模块
- 目标项目已有自己的目录规范，不希望再嵌套一个 `add_subdirectory(...)`
- 你准备对 `SocketClient` 做本地定制

这种方式的代价是：后续如果要同步上游改动，需要你自己手动比对和拷贝文件。

## 常见建议

- 作为依赖引入时，通常把 `REDIS_SOCKET_CLIENT_BUILD_EXAMPLE` 和 `REDIS_SOCKET_CLIENT_BUILD_TESTS` 都设为 `OFF`。
- 如果你只需要库本体，构建目标直接选 `redis_socket_client_core` 即可，不必编 example。
- GUI 项目里不要直接在回调里操作 UI；应把回调数据投递回自身 UI 线程。

## 接入方式

公共接口在 `include/socket/SocketClient.h`。客户端提供连接、断开、发送 raw JSON、发送 JsonValue envelope、手动重连和可选自动重连能力。JSON 依赖使用本目录内的 `include/core/LiteJsonDataParser.h`，不依赖 nlohmann/json 或 Qt。

```cpp
redis_dc::SocketClient client;
client.setReconnectOptions({true, std::chrono::milliseconds(1000), 0});
client.setRawMessageCallback([](const std::string& payload) {
    // raw JSON payload
});
client.setJsonMessageCallback([](const redis_dc::lite_json_data::JsonValue& payload) {
    // parsed object payload
});
client.connect("127.0.0.1", 9000);
```

## 入站顺序处理

SocketClient 内部有两个工作线程：Asio IO 线程负责持续收发帧，入站 FIFO 处理线程负责逐条触发 raw/json 回调。前一条回调未返回时，后一条不会进入业务回调；后续消息会排队等待，默认不主动丢弃入站 payload。

这适合大模型或耗时业务处理场景：网络层继续接收完整帧，业务层仍按接收顺序线性处理。GUI 项目应在回调中把消息投递到自身 UI 线程或业务队列。

## 长模型传输测试

主项目测试覆盖了 10000 个 mesh、150000 个 points 的模型 JSON payload 传输，验证 payload 未截断、envelope 可解析、mesh 数量和 point 总数正确。

```powershell
cmake --build build_modules_validate --config Debug --target redis_data_center_tests
./build_modules_validate/tests/Debug/redis_data_center_tests.exe "socket client transports large model payloads without truncation"
./build_modules_validate/tests/Debug/redis_data_center_tests.exe "socket client processes inbound messages sequentially without skipping"
```
