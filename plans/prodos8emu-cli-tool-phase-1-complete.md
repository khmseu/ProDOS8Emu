# Phase 1 Complete: Add ROM Loading API to Apple2Memory

Extended Apple2Memory with a public method to load ROM image files (12KB) into the ROM backing area ($D000-$FFFF), enabling proper ROM/LC RAM switching behavior for emulator initialization.

**Files created/changed:**

- include/prodos8emu/apple2mem.hpp
- src/apple2mem.cpp
- tests/apple2mem_test.cpp
- CMakeLists.txt

**Functions created/changed:**

- `Apple2Memory::loadROM(const std::filesystem::path& path)` - Loads 12KB ROM image from file
- `createTestROM()` - Test helper to generate deterministic ROM files
- `updateBanks()` - (existing) Maps ROM content when LC read is disabled

**Tests created/changed:**

- `ROMLoadingTest` - Verifies ROM data is loaded into memory
- `ROMReadbackTest` - Validates ROM content at $D000/$E000/$F000 and reset vector
- `ROMLCBehaviorTest` - Confirms ROM vs LC RAM switching works correctly
- `ROMSizeValidationSmallFile` - Rejects files < 12KB
- `ROMSizeValidationLargeFile` - Rejects files > 12KB
- `ROMLoadingNonexistentFile` - Handles missing files gracefully

**Review Status:** APPROVED

**Git Commit Message:**

```text
feat: Add ROM loading API to Apple2Memory

- Add loadROM() method to load 12KB ROM images into $D000-$FFFF
- Validate exact ROM size (0x3000 bytes) and throw on errors
- ROM content accessible when LC read is disabled
- Add 6 comprehensive tests with hermetic temp file generation
- Update documentation for ROM loading and reset() semantics
```
