## Phase 6 Complete: Complete Execution Pipeline

Completed the `prodos8emu_run` tool so it can load a ROM image, load a ProDOS system file into $2000, initialize the warm restart vector, override the CPU reset vector to start at $2000, and execute the CPU for a bounded number of instructions.

**Files created/changed:**

- tools/prodos8emu_run.cpp
- tests/emulator_startup_test.cpp
- CMakeLists.txt

**Functions created/changed:**

- Full ROM+system load + reset/run pipeline in `prodos8emu_run`

**Tests created/changed:**

- Emulator startup end-to-end test (hermetic temp ROM + system file)

**Review Status:** APPROVED

**Git Commit Message:**
feat: Run system program from reset

- Load ROM and system file, initialize warm restart vector
- Override reset vector to $2000 and call cpu.reset()
- Run CPU with default bounded instruction limit (override via flag)
- Print execution summary and registers
- Add hermetic emulator startup test target
