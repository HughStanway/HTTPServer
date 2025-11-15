include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(test_httpparser
    test_httpparser.cpp
)

target_link_libraries(test_httpparser
    gtest
    gtest_main
    httpserver_lib
)

target_include_directories(test_httpparser
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME HttpParserTests COMMAND test_httpparser)
