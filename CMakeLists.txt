cmake_minimum_required(VERSION 3.10)
project(my_http_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
include_directories(third_party)

add_executable(http_server
    src/main.cpp
    src/server.cpp
    src/port.cpp
    src/logger.cpp
    #src/request.cpp
    #src/response.cpp
    #src/router.cpp
    #src/utils.cpp
)

