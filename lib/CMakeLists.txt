add_library(httpserver_lib
    src/httpserver/core/server.cpp
    src/httpserver/core/connection_handler.cpp
    src/httpserver/core/connection_manager.cpp
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

target_link_libraries(httpserver_lib PRIVATE
    $<BUILD_INTERFACE:tomlplusplus::tomlplusplus>
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

include(GNUInstallDirs)

install(TARGETS httpserver_lib
    EXPORT HttpServerTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY src/httpserver
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(FILES include/httpserver
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT HttpServerTargets
    FILE HttpServerTargets.cmake
    NAMESPACE HttpServer::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HttpServer
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HttpServerConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/HttpServerConfig.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/HttpServerConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/HttpServerConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HttpServer
    )

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/HttpServerConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/HttpServerConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HttpServer
    )
endif()