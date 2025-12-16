include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    test_httpparser.cpp
    test_router.cpp
)

target_link_libraries(unit_tests
    gtest
    gtest_main
    httpserver_lib
)

target_include_directories(unit_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME HttpParserTests COMMAND unit_tests)
add_test(NAME RouterTests COMMAND unit_tests)