cmake_minimum_required(VERSION 3.10)
project(my_http_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_SANITIZERS "Compile with ASan and UBSan" OFF)

# Add the library directory
add_subdirectory(lib)

# Add the executable directory
add_subdirectory(src)

# Tests
enable_testing()
add_subdirectory(tests)

if (ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
