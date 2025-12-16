add_executable(test_http_server
   main.cpp 
)

target_link_libraries(test_http_server
    httpserver_lib
)

target_include_directories(test_http_server
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)