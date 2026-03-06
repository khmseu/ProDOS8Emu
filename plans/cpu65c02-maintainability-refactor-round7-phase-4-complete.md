## Phase 4 Complete: Table-Drive ALU/RMW Decode Dispatch

Phase 4 replaced remaining ALU/RMW decode branch trees with metadata-driven classifiers and dispatch tables while preserving behavioral parity. Added equivalence and precedence characterization tests to lock decode routing, cycle behavior, page-cross handling, flags, and writeback effects.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::read_alu_operand_for_mode(uint8_t mode, uint8_t& operand, uint32_t& cycles)`
- `CPU65C02::execute_alu_family_opcode(uint8_t op)`
- `CPU65C02::read_rmw_target_for_mode(uint8_t mode, uint16_t& addr, uint32_t& cycles)`
- `CPU65C02::apply_rmw_family_op(uint8_t group, uint8_t value)`
- `CPU65C02::execute_rmw_family_opcode(uint8_t op)`
- ALU/RMW metadata/classifier helpers in `src/cpu65c02.cpp`

**Tests created/changed:**

- `alu_decode_table_equivalence`
- `rmw_decode_table_equivalence`
- `alu_rmw_route_precedence_nonregression`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: table-drive alu rmw decode paths

- replace ALU/RMW decode branch trees with metadata-driven routing
- preserve cycle, page-cross, flag, and writeback behavior contracts
- add ALU/RMW equivalence and precedence characterization tests
- keep fallback decode precedence and route membership stable
- validate cpu65c02 and emulator-startup ctest targets
