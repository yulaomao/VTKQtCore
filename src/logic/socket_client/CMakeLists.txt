cmake_minimum_required(VERSION 3.24)

set(SOCKET_CLIENT_IS_TOP_LEVEL OFF)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(SOCKET_CLIENT_IS_TOP_LEVEL ON)
  project(socket_client LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

option(SOCKET_CLIENT_BUILD_TESTS "Build standalone socket client tests" OFF)
option(SOCKET_CLIENT_BUILD_EXAMPLE "Build standalone socket client example" ${SOCKET_CLIENT_IS_TOP_LEVEL})

if(NOT COMMAND socket_dc_apply_warnings)
  function(socket_dc_apply_warnings target_name)
    if(MSVC)
      target_compile_options(${target_name} PRIVATE /W4 /permissive- /utf-8)
    else()
      target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endfunction()
endif()

find_package(Threads REQUIRED)

if(NOT TARGET asio::asio)
  set(SOCKET_CLIENT_ASIO_INCLUDE_DIR "" CACHE PATH "Path to the Asio include directory containing asio.hpp")

  if(NOT SOCKET_CLIENT_ASIO_INCLUDE_DIR)
    foreach(asio_candidate
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/asio/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/asio/asio/include")
      if(EXISTS "${asio_candidate}/asio.hpp")
        set(SOCKET_CLIENT_ASIO_INCLUDE_DIR "${asio_candidate}")
        break()
      endif()
    endforeach()
  endif()

  if(NOT SOCKET_CLIENT_ASIO_INCLUDE_DIR)
    find_path(SOCKET_CLIENT_ASIO_INCLUDE_DIR asio.hpp)
  endif()

  if(NOT SOCKET_CLIENT_ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "Asio headers not found. Set SOCKET_CLIENT_ASIO_INCLUDE_DIR to the directory containing asio.hpp.")
  endif()

  add_library(asio::asio INTERFACE IMPORTED)
  set_target_properties(asio::asio PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SOCKET_CLIENT_ASIO_INCLUDE_DIR}"
  )
  target_compile_definitions(asio::asio INTERFACE ASIO_STANDALONE)
endif()

add_library(socket_client_core STATIC
  src/SocketClient.cpp
)
add_library(socket_dc::socket_client ALIAS socket_client_core)
target_compile_features(socket_client_core PUBLIC cxx_std_17)
target_include_directories(socket_client_core
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(socket_client_core PUBLIC asio::asio Threads::Threads)
if(WIN32)
  target_compile_definitions(socket_client_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
  target_link_libraries(socket_client_core PUBLIC ws2_32 mswsock)
endif()
socket_dc_apply_warnings(socket_client_core)

if(SOCKET_CLIENT_BUILD_EXAMPLE)
  add_executable(socket_client_example examples/socket_client_example.cpp)
  target_link_libraries(socket_client_example PRIVATE socket_client_core)
  socket_dc_apply_warnings(socket_client_example)
endif()

if(SOCKET_CLIENT_BUILD_TESTS)
  enable_testing()

  if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 QUIET CONFIG)
    if(NOT Catch2_FOUND)
      message(FATAL_ERROR "Catch2 not found. Install Catch2 or use a parent project that provides Catch2::Catch2WithMain.")
    endif()
  endif()

  add_executable(socket_client_tests tests/SocketClientTests.cpp)
  target_include_directories(socket_client_tests PRIVATE tests)
  target_link_libraries(socket_client_tests PRIVATE socket_client_core Catch2::Catch2WithMain)
  socket_dc_apply_warnings(socket_client_tests)

  include(Catch)
  catch_discover_tests(socket_client_tests)
endif()