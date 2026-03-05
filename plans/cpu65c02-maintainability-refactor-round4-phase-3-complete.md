# Phase 3 Complete: Split Control-Flow by Concern

Refactored control-flow handling by splitting `execute_control_flow_opcode` into focused helpers for branch opcodes, jump/return opcodes, and COUT output formatting. The split preserves cycle timing, PC transitions, decode order, and output semantics, with targeted characterization tests added for branch matrix and COUT escaping.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::execute_control_flow_branch_opcode`
- `CPU65C02::execute_control_flow_jump_return_opcode`
- `CPU65C02::emit_cout_char`
- `CPU65C02::execute_control_flow_opcode`
- `branch_opcode_matrix_preserved`
- `cout_escape_mapping_contracts_preserved`

**Tests created/changed:**

- `branch_opcode_matrix_preserved`
- `cout_escape_mapping_contracts_preserved`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: split control-flow opcode handling concerns

- extract branch opcode handling into dedicated control-flow helper
- extract jump/return opcode handling into dedicated helper
- centralize COUT character escaping/output formatting in helper
- preserve cycle, PC, decode-order, and output behavior contracts
- add branch matrix and COUT escape characterization tests
