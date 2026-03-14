add_library(httpserver_lib
    src/httpserver/core/server.cpp
    src/httpserver/core/connection_handler.cpp
    src/httpserver/utils/port.cpp
    src/httpserver/monitoring/logger.cpp
    src/httpserver/http/http_parser.cpp
    src/httpserver/utils/utils.cpp
    src/httpserver/http/http_object.cpp
    src/httpserver/http/http_response_builder.cpp
    src/httpserver/routing/router.cpp
    src/httpserver/routing/route_matchers.cpp
    src/httpserver/threading/thread_pool.cpp
    src/httpserver/utils/periodic_idle_ip_cleanup.cpp
    src/httpserver/utils/config.cpp
    src/httpserver/monitoring/metrics.cpp
    src/httpserver/events/event_dispatcher.cpp
)

include(FetchContent)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0   # pin to a known good version
)

FetchContent_MakeAvailable(tomlplusplus)

target_link_libraries(httpserver_lib PUBLIC
    tomlplusplus::tomlplusplus
)

find_package(OpenSSL REQUIRED)
target_link_libraries(httpserver_lib PRIVATE
    OpenSSL::SSL OpenSSL::Crypto
)

target_include_directories(httpserver_lib PUBLIC
    ${OPENSSL_INCLUDE_DIR}
)

target_include_directories(httpserver_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)