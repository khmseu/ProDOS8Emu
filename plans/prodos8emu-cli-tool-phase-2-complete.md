## Phase 2 Complete: System File Loader Helper

Added a small loader helper that reads a ProDOS system file from the host filesystem, validates its entry opcode, and writes it into emulated memory starting at $2000 (or a caller-provided load address).

**Files created/changed:**

- include/prodos8emu/system_loader.hpp
- src/system_loader.cpp
- tests/system_test.cpp
- CMakeLists.txt

**Functions created/changed:**

- `loadSystemFile(Apple2Memory& mem, const std::filesystem::path& filePath, uint16_t loadAddr)`

**Tests created/changed:**

- System loader tests in `system_test.cpp` covering:
  - Valid load at $2000
  - Invalid first byte (must be 0x4C)
  - Oversized file rejection
  - Invalid load address rejection (>= $C000)

**Review Status:** APPROVED

**Git Commit Message:**
feat: Add ProDOS system file loader helper

- Add loadSystemFile() to load system programs into RAM at $2000
- Validate entry opcode (0x4C JMP abs) and reject invalid images
- Reject invalid load addresses (>= $C000) and oversized images
- Add hermetic system tests for load/validation/error cases
