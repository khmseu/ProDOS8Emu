## Plan: ProDOS8 CLI Emulator Tool

Create a C++ command-line tool that instantiates a complete Apple II emulator using the existing library, loads a ROM image and ProDOS system file, initializes the warm restart vector, and executes CPU instructions from reset.

**Phases: 6**

### 1. **Phase 1: Add ROM Loading API to Apple2Memory**

- **Objective:** Extend Apple2Memory with a public method to load ROM image files into the ROM backing area ($D000-$FFFF).
- **Files/Functions to Modify/Create:**
  - [include/prodos8emu/apple2mem.hpp](../include/prodos8emu/apple2mem.hpp): Add `loadROM(const std::filesystem::path& path)` method declaration
  - [src/apple2mem.cpp](../src/apple2mem.cpp): Implement ROM loading (read 12KB from file, populate m_romArea)
- **Tests to Write:**
  - `ROMLoadingTest`: Verify loadROM reads file and populates ROM area correctly
  - `ROMReadbackTest`: Verify ROM content is readable at $D000-$FFFF when LC read is disabled
  - `ROMSizeValidation`: Verify error handling for incorrect file sizes
- **Steps:**
  1.  Write test that creates a temporary ROM file and calls `loadROM()`
  2.  Run test to see it fail (method doesn't exist)
  3.  Add method declaration to apple2mem.hpp
  4.  Implement minimal loadROM in apple2mem.cpp (read file, validate size, copy to m_romArea)
  5.  Run test to see it pass
  6.  Write readback test verifying ROM data is accessible
  7.  Run test to see it pass
  8.  Build and run all tests to confirm no regressions

### 2. **Phase 2: System File Loader Helper**

- **Objective:** Create a utility function to load ProDOS system files ($FF type) into memory at $2000.
- **Files/Functions to Modify/Create:**
  - [include/prodos8emu/system_loader.hpp](../include/prodos8emu/system_loader.hpp): New header declaring `loadSystemFile()`
  - [src/system_loader.cpp](../src/system_loader.cpp): Implement system file loading logic
- **Tests to Write:**
  - `SystemFileLoadTest`: Verify file bytes are written starting at $2000
  - `SystemFileValidationTest`: Verify JMP instruction check at $2000
  - `PathBufferInitTest`: Verify $0280 pathname initialization
- **Steps:**
  1.  Write test that creates a minimal system file (starts with JMP) and loads it
  2.  Run test to see it fail (function doesn't exist)
  3.  Create system_loader.hpp with function signature
  4.  Implement loadSystemFile in system_loader.cpp (read file, write to $2000+)
  5.  Run test to see it pass
  6.  Write validation test for JMP opcode check
  7.  Implement validation in loadSystemFile
  8.  Run test to see it pass
  9.  Build and run all tests

### 3. **Phase 3: Warm Restart Vector Initialization**

- **Objective:** Implement helper to initialize ProDOS warm restart vector at $03F2.
- **Files/Functions to Modify/Create:**
  - [include/prodos8emu/system_loader.hpp](../include/prodos8emu/system_loader.hpp): Add `initWarmStartVector()` declaration
  - [src/system_loader.cpp](../src/system_loader.cpp): Implement warm start vector setup
- **Tests to Write:**
  - `WarmStartVectorTest`: Verify $03F2/$03F3 are set to $2000
  - `WarmStartCheckByteTest`: Verify $03F4 is set correctly
- **Steps:**
  1.  Write test that initializes warm vector and verifies memory contents
  2.  Run test to see it fail
  3.  Add initWarmStartVector declaration to header
  4.  Implement function (write $2000 to $03F2/$03F3, set check byte at $03F4)
  5.  Run test to see it pass
  6.  Build and run all tests

### 4. **Phase 4: CLI Tool Scaffolding**

- **Objective:** Create the main CLI executable with argument parsing for ROM and system file paths.
- **Files/Functions to Modify/Create:**
  - [tools/prodos8emu_run.cpp](../tools/prodos8emu_run.cpp): New CLI tool main()
  - [CMakeLists.txt](../CMakeLists.txt): Add executable target for prodos8emu_run
- **Tests to Write:**
  - Manual CLI testing with --help flag
  - Manual CLI testing with invalid arguments
- **Steps:**
  1.  Create prodos8emu_run.cpp with basic main() and argument parsing
  2.  Add executable target to CMakeLists.txt
  3.  Build to see it compile
  4.  Run with --help to verify usage message
  5.  Test error handling for missing arguments

### 5. **Phase 5: Emulator Initialization**

- **Objective:** Instantiate Apple2Memory, CPU65C02, and MLIContext in the CLI tool.
- **Files/Functions to Modify/Create:**
  - [tools/prodos8emu_run.cpp](../tools/prodos8emu_run.cpp): Add emulator setup code
- **Tests to Write:**
  - Integration test verifying emulator components connect properly
- **Steps:**
  1.  Write integration test that creates all components and verifies linkage
  2.  Run test to see it fail
  3.  Add code to main() to instantiate Apple2Memory
  4.  Add code to instantiate MLIContext with volumes root
  5.  Add code to instantiate CPU65C02 with memory reference
  6.  Add code to attach MLI to CPU
  7.  Run integration test to see it pass
  8.  Build and run all tests

### 6. **Phase 6: Complete Execution Pipeline**

- **Objective:** Load ROM and system file, initialize vectors, reset CPU, and execute instructions.
- **Files/Functions to Modify/Create:**
  - [tools/prodos8emu_run.cpp](../tools/prodos8emu_run.cpp): Complete main() with full execution flow
- **Tests to Write:**
  - End-to-end integration test with minimal ROM and system file
  - Test verifying CPU starts at correct address after reset
- **Steps:**
  1.  Write E2E test that simulates full startup sequence
  2.  Run test to see it fail
  3.  Add ROM loading call in main()
  4.  Add system file loading call in main()
  5.  Add warm vector initialization call in main()
  6.  Enable LC read/write for reset vector access
  7.  Set reset vector ($FFFC) to $2000
  8.  Call cpu.reset()
  9.  Add cpu.run() with configurable instruction limit
  10. Add basic status output (PC, registers, cycle count)
  11. Run test to see it pass
  12. Build and run manual test with real ROM/system files
  13. Run full test suite

**Open Questions:**

1. Should the CLI tool support a --max-instructions limit or run indefinitely? _Suggestion: default to 1 million with --max-instructions flag_
2. Should we add debug output (register dumps, memory inspection)? _Suggestion: add --verbose flag for Phase 6_
3. Where should the volumes root directory be configured? _Suggestion: add --volume-root flag or default to ./volumes_
4. Should the ROM path be optional (allowing ROM-less operation with LC RAM only)? _Suggestion: make ROM optional, document LC-only mode_
