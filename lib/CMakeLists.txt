add_library(httpserver_lib
    src/server.cpp
    src/port.cpp
    src/logger.cpp
    src/http_parser.cpp
    src/utils.cpp
    src/http_response.cpp
    src/http_response_builder.cpp
    src/router.cpp
)

target_include_directories(httpserver_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)