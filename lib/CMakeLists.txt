add_library(httpserver_lib
    src/server.cpp
    src/port.cpp
    src/logger.cpp
    src/http_parser.cpp
    src/utils.cpp
    src/http_object.cpp
    src/http_response_builder.cpp
    src/router.cpp
    src/thread_pool.cpp
    src/periodic_idle_ip_cleanup.cpp
    src/config.cpp
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
)