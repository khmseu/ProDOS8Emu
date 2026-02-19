cmake_minimum_required(VERSION 3.20)

project(ProDOS8Emu 
    VERSION 0.1.0
    DESCRIPTION "ProDOS 8 MLI Emulation Library"
    LANGUAGES CXX
)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Library target
add_library(prodos8emu 
    src/mli.cpp
)

target_include_directories(prodos8emu
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Version information
target_compile_definitions(prodos8emu
    PUBLIC
        PRODOS8EMU_VERSION="${PROJECT_VERSION}"
)

# Compiler warnings
target_compile_options(prodos8emu PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# Test executable - smoke tests
add_executable(prodos8emu_tests
    tests/smoke_test.cpp
)

target_link_libraries(prodos8emu_tests 
    PRIVATE 
        prodos8emu
)

# Register tests with CTest
add_test(NAME prodos8emu_tests COMMAND prodos8emu_tests)

# Test executable - memory tests
add_executable(prodos8emu_memory_tests
    tests/memory_test.cpp
)

target_link_libraries(prodos8emu_memory_tests 
    PRIVATE 
        prodos8emu
)

add_test(NAME prodos8emu_memory_tests COMMAND prodos8emu_memory_tests)
