## Phase 4 Complete: Extract Load/Store Addressing Helpers

Refactored `execute_load_store_opcode` into helper-focused load/store dispatch paths grouped by addressing behavior while preserving existing routing and semantics. Added a full-range dispatch-equivalence characterization test plus representative runtime contracts to lock cycle counts, load-only NZ updates, and store target/non-mutation behavior.

**Files created/changed:**
- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**
- `CPU65C02::execute_load_store_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_load_store_load_immediate_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_load_store_load_read_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_load_store_load_page_cross_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_load_store_store_direct_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_load_store_store_indexed_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_load_store_store_zero_opcode(uint8_t op, uint32_t& cycles)`
- `run_load_store_helper_dispatch_equivalence_test(int& failures)`

**Tests created/changed:**
- `load_store_helper_dispatch_equivalence`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: extract load/store dispatch helpers

- split load/store opcode dispatch into addressing-focused helper methods
- keep load NZ updates and store non-mutation behavior contracts unchanged
- add load/store helper dispatch equivalence characterization coverage
- preserve decode routing and default fallback behavior
- validate cpu and startup ctest targets to green
