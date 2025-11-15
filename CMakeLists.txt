cmake_minimum_required(VERSION 3.10)
project(my_http_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
include_directories(third_party)

add_library(httpserver_lib
    src/server.cpp
    src/port.cpp
    src/logger.cpp
    src/httpparser.cpp
)

add_executable(http_server
    src/main.cpp
)

target_link_libraries(http_server
    httpserver_lib
)
enable_testing()
add_subdirectory(tests)

