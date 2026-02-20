# ProDOS8Emu - Copilot Instructions

## Project Overview

ProDOS 8 Machine Language Interface (MLI) emulation layer for bridging Apple II file operations to Unix filesystems.

## Architecture & Conventions

### MLI Call Signatures

- MLIContext methods: `uint8_t methodName(const ConstMemoryBanks& banks, uint16_t paramBlockAddr)` for read-only or `uint8_t methodName(MemoryBanks& banks, uint16_t paramBlockAddr)` for mutable access
- Return ProDOS error codes (uint8_t) defined in [include/prodos8emu/errors.hpp](../include/prodos8emu/errors.hpp)
- MLI declarations in [include/prodos8emu/mli.hpp](../include/prodos8emu/mli.hpp), implementations in [src/mli\_\*.cpp](../src/) (e.g., [src/mli_housekeeping.cpp](../src/mli_housekeeping.cpp))

### Memory Model

- Banked memory: 16 banks × 4096 bytes ([include/prodos8emu/memory.hpp](../include/prodos8emu/memory.hpp))
- Address translation: `bank = addr >> 12`, `offset = addr & 0x0FFF`
- Helpers: `read_u8`, `write_u8`, `read_u16_le`, `write_u16_le`, `read_u24_le`, `write_u24_le`, `read_counted_string`
- All ProDOS multi-byte values are little-endian

### Path Handling

- Path normalization and prefix resolution in [include/prodos8emu/path.hpp](../include/prodos8emu/path.hpp), [src/path.cpp](../src/path.cpp)
- Normalize: high-bit clear (& 0x7F) + uppercase; components 1-15 chars, start with A-Z, contain A-Z/0-9/. only
- 64-byte max for input counted strings or stored prefix; 128-byte max for resolved full path
- Prefix semantics: full path starts with `/`, partial does not; `volumesRoot` maps ProDOS volume names to Unix paths

### Extended Attributes (Xattrs)

- ProDOS metadata stored in `user.prodos8.*` namespace ([include/prodos8emu/xattr.hpp](../include/prodos8emu/xattr.hpp), [src/xattr.cpp](../src/xattr.cpp))
- Functions: `prodos8_get_xattr`, `prodos8_set_xattr`, `prodos8_remove_xattr`
- Maps errno ENOSPC → ERR_VOLUME_FULL

### Tests

- Standalone executables with `main()` in [tests/](../tests/), wired via CTest targets in [CMakeLists.txt](../CMakeLists.txt)
- Targets: `prodos8emu_tests`, `prodos8emu_memory_tests`, `prodos8emu_path_prefix_tests`, `prodos8emu_housekeeping_tests`

## Build & Test Workflow

```bash
cmake -S . -B build        # Configure
cmake --build build        # Build all targets
ctest --test-dir build     # Run all tests
```

## Documentation Cache

- `third_party_docs/` is a local cache (gitignored by default)
- Only commit redistributable content; see [third_party_docs/README.md](../third_party_docs/README.md)
- If you fetch docs from the internet and it seems plausible you’ll need them again (or you already did), keep a copy here for future work (prefer URL-derived paths like existing content)
- When you need information from the net, check `third_party_docs/` first to see if it’s already available locally
