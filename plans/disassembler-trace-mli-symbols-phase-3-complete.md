# Phase 3 Complete: Per-Instruction Disassembly Core

Implemented the core per-instruction disassembly emission path using the dedicated disassembly sink from Phase 2. When enabled, CPU now emits one stable line per executed instruction with instruction index, PC, and opcode byte; execution behavior remains unchanged.

**Files created/changed:**

- src/cpu65c02.cpp
- tests/cpu65c02_disassembly_core_test.cpp
- CMakeLists.txt

**Functions created/changed:**

- `emit_disassembly_trace_line` in `src/cpu65c02.cpp`
- `CPU65C02::step` in `src/cpu65c02.cpp`
- `disassembly_emits_one_line_per_instruction_with_stable_order` in `tests/cpu65c02_disassembly_core_test.cpp`
- `disassembly_only_emits_while_sink_is_non_null` in `tests/cpu65c02_disassembly_core_test.cpp`

**Tests created/changed:**

- New target `prodos8emu_cpu65c02_disassembly_core_tests` in `CMakeLists.txt`
- New test file `tests/cpu65c02_disassembly_core_test.cpp`

**Review Status:** APPROVED

**Test outcomes:**

- `ctest --test-dir build -R prodos8emu_cpu65c02_disassembly_core_tests --output-on-failure`: PASS
- `ctest --test-dir build -R prodos8emu_run_cli_tests --output-on-failure`: PASS

**Git Commit Message:**
feat: add per-instruction disassembly core output

- emit one disassembly line per executed instruction via dedicated sink
- add focused CPU tests for output ordering and sink enable/disable
- register dedicated disassembly core CTest target
