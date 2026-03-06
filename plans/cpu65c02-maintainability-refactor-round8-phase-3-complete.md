## Phase 3 Complete: Table-Drive Control-Flow Branch Decode

Phase 3 replaced manual control-flow branch opcode switching with metadata-driven branch routing while preserving runtime branch behavior and decode precedence. Added characterization tests lock route equivalence and nonregression for branch dispatch ordering.

**Files created/changed:**

- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `find_control_flow_branch_metadata(uint8_t op)` (anonymous namespace)
- `CPU65C02::execute_control_flow_branch_opcode(uint8_t op, uint32_t& cycles)`
- `run_control_flow_branch_table_dispatch_equivalence_test(int& failures)`
- `run_control_flow_branch_precedence_nonregression_test(int& failures)`

**Tests created/changed:**

- `control_flow_branch_table_dispatch_equivalence`
- `control_flow_branch_precedence_nonregression`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: table-drive control-flow branch decode

- replace branch opcode switch with metadata-driven dispatch
- preserve BRA and conditional branch cycle/PC semantics
- add branch dispatch equivalence and precedence nonregression tests
- keep control-flow decode routing order unchanged
- validate cpu65c02 and emulator-startup ctest targets
