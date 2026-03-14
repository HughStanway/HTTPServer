add_library(httpserver_lib
    src/httpserver_impl/core/server.cpp
    src/httpserver_impl/core/connection_handler.cpp
    src/httpserver_impl/core/connection_manager.cpp
    src/httpserver_impl/utils/port.cpp
    src/httpserver_impl/monitoring/logger.cpp
    src/httpserver_impl/http/http_parser.cpp
    src/httpserver_impl/utils/utils.cpp
    src/httpserver_impl/http/http_object.cpp
    src/httpserver_impl/http/http_response_builder.cpp
    src/httpserver_impl/routing/router.cpp
    src/httpserver_impl/routing/route_matchers.cpp
    src/httpserver_impl/threading/thread_pool.cpp
    src/httpserver_impl/utils/periodic_idle_ip_cleanup.cpp
    src/httpserver_impl/utils/config.cpp
    src/httpserver_impl/monitoring/metrics.cpp
    src/httpserver_impl/events/event_dispatcher.cpp
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

install(DIRECTORY src/httpserver_impl
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