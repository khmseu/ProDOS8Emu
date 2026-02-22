cmake_minimum_required(VERSION 3.20)

project(ProDOS8Emu 
    VERSION 0.1.0
    DESCRIPTION "ProDOS 8 MLI Emulation Library"
    LANGUAGES CXX
)

# Export compile_commands.json for tooling (clangd, editors, static analysis)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Library target
add_library(prodos8emu 
    src/mli.cpp
    src/mli_housekeeping.cpp
    src/mli_filing.cpp
    src/mli_system.cpp
    src/mli_dispatch.cpp
    src/path.cpp
    src/xattr.cpp
    src/apple2mem.cpp
    src/cpu65c02.cpp
    src/system_loader.cpp
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

# Test executable - path and prefix tests
add_executable(prodos8emu_path_prefix_tests
    tests/path_prefix_test.cpp
)

target_link_libraries(prodos8emu_path_prefix_tests 
    PRIVATE 
        prodos8emu
)

add_test(NAME prodos8emu_path_prefix_tests COMMAND prodos8emu_path_prefix_tests)

# Test executable - housekeeping tests
add_executable(prodos8emu_housekeeping_tests
    tests/housekeeping_test.cpp
)

target_link_libraries(prodos8emu_housekeeping_tests 
    PRIVATE 
        prodos8emu
)

add_test(NAME prodos8emu_housekeeping_tests COMMAND prodos8emu_housekeeping_tests)

# Test executable - filing tests
add_executable(prodos8emu_filing_tests
    tests/filing_test.cpp
)

target_link_libraries(prodos8emu_filing_tests 
    PRIVATE 
        prodos8emu
)

add_test(NAME prodos8emu_filing_tests COMMAND prodos8emu_filing_tests)

# Test executable - system tests
add_executable(prodos8emu_system_tests
    tests/system_test.cpp
)

target_link_libraries(prodos8emu_system_tests 
    PRIVATE 
        prodos8emu
)

add_test(NAME prodos8emu_system_tests COMMAND prodos8emu_system_tests)

# Test executable - Apple2Memory tests
add_executable(prodos8emu_apple2mem_tests
    tests/apple2mem_test.cpp
)

target_link_libraries(prodos8emu_apple2mem_tests
    PRIVATE
        prodos8emu
)

add_test(NAME prodos8emu_apple2mem_tests COMMAND prodos8emu_apple2mem_tests)
set_tests_properties(prodos8emu_apple2mem_tests PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# Test executable - CPU65C02 tests
add_executable(prodos8emu_cpu65c02_tests
    tests/cpu65c02_test.cpp
)

target_link_libraries(prodos8emu_cpu65c02_tests
    PRIVATE
        prodos8emu
)

add_test(NAME prodos8emu_cpu65c02_tests COMMAND prodos8emu_cpu65c02_tests)
