# Plan Complete: ProDOS8 CLI Emulator Tool

Implemented a new C++ CLI tool that uses the ProDOS8Emu library to load an Apple II ROM image and a ProDOS system file, initialize the Apple II warm restart vector, and execute the 65C02 CPU starting from reset. This provides a concrete end-to-end entrypoint for driving the emulator from the command line while staying compatible with the existing MLI trap-based integration.

**Phases Completed:** 6 of 6

1. ✅ Phase 1: Add ROM Loading API to Apple2Memory
2. ✅ Phase 2: System File Loader Helper
3. ✅ Phase 3: Warm Restart Vector Init
4. ✅ Phase 4: CLI Tool Scaffolding
5. ✅ Phase 5: Emulator Initialization
6. ✅ Phase 6: Complete Execution Pipeline

**All Files Created/Modified:**

- CMakeLists.txt
- include/prodos8emu/apple2mem.hpp
- include/prodos8emu/system_loader.hpp
- src/apple2mem.cpp
- src/system_loader.cpp
- tests/apple2mem_test.cpp
- tests/system_test.cpp
- tests/emulator_startup_test.cpp
- tools/prodos8emu_run.cpp
- plans/prodos8emu-cli-tool-plan.md
- plans/prodos8emu-cli-tool-phase-1-complete.md
- plans/prodos8emu-cli-tool-phase-2-complete.md
- plans/prodos8emu-cli-tool-phase-3-complete.md
- plans/prodos8emu-cli-tool-phase-4-complete.md
- plans/prodos8emu-cli-tool-phase-5-complete.md
- plans/prodos8emu-cli-tool-phase-6-complete.md

**Key Functions/Classes Added:**

- `Apple2Memory::loadROM(const std::filesystem::path&)`
- `loadSystemFile(Apple2Memory&, const std::filesystem::path&, uint16_t)`
- `initWarmStartVector(Apple2Memory&, uint16_t)`

**Test Coverage:**

- Total tests written: 1 new test executable + new cases in existing tests
- All tests passing: ✅

**Recommendations for Next Steps:**

- Expand the CLI to support starting from the ROM reset vector (true Apple II boot path) once disk/boot emulation is added.
- Add optional tracing/logging hooks for debugging system program execution.
