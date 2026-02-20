# ProDOS8Emu - ProDOS 8 MLI Emulation Library

A modern C++ library for emulating ProDOS 8 MLI (Machine Language Interface) calls on Linux.

## Overview

This library is being developed to provide 1:1 emulation of ProDOS 8 MLI calls. The planned architecture includes parameter blocks stored in emulated 6502 memory (16 banks × 4KB), with Linux filesystem operations used where possible and ProDOS-specific metadata stored in Linux extended attributes (`xattr`) under the `user.prodos8.*` namespace.

For detailed implementation plans and roadmap, see [plans/prodos8-mli-emulation-plan.md](plans/prodos8-mli-emulation-plan.md).

## Building

### Requirements

- CMake 3.20 or later
- C++20 compatible compiler (GCC 11+, Clang 12+)
- Linux with xattr support

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

### Running Tests

```bash
# Run tests directly
./prodos8emu_tests

# Or via CTest
ctest --output-on-failure
```

## Project Structure

```text
ProDOS8Emu/
├── include/prodos8emu/   # Public headers
│   └── mli.hpp           # Main MLI interface
├── src/                  # Implementation files
│   └── mli.cpp
├── tests/                # Unit tests
│   └── smoke_test.cpp
├── plans/                # Implementation roadmap
└── CMakeLists.txt        # Build configuration
```

## Development Status

**Phase 1: Project + Test Scaffolding** ✅ Complete

- CMake build system
- Basic library structure
- Smoke tests

**Phase 2: Emulated Memory + Error Codes** 🔜 Next

- Banked memory model (16 banks × 4KB)
- Parameter block helpers
- ProDOS error code constants

See [plans/prodos8-mli-emulation-plan.md](plans/prodos8-mli-emulation-plan.md) for the complete roadmap.

## License

TBD
