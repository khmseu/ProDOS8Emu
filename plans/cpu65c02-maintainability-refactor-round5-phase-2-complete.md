## Phase 2 Complete: Extract Execute Fallback Routing Helper

Extracted fallback routing from `execute()` into a dedicated helper and preserved decode precedence by delegating to the new router only after special and control-flow decode paths. Added focused characterization coverage for fallback-router dispatch and validated both required CTest targets.

**Files created/changed:**
- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**
- `CPU65C02::execute_fallback_router_opcode(uint8_t op)`
- `CPU65C02::execute(uint8_t op)`
- `run_execute_fallback_router_dispatch_preserved_test(int& failures)`

**Tests created/changed:**
- `execute_fallback_router_dispatch_preserved` (Test 48)
- `branch_opcode_matrix_preserved` (heap-backed memory fixture update)
- `cout_escape_mapping_contracts_preserved` (heap-backed memory fixture update)
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: extract execute fallback router

- move fallback dispatch out of execute into helper
- preserve decode precedence and route behavior contracts
- add fallback router dispatch characterization test
- keep NOP handling unchanged for upcoming phase 3
- validate cpu and startup ctest targets to green
