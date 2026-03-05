## Phase 4 Complete: Decompose execute() by Opcode Families

Extracted a coherent control-flow/system opcode family from `execute()` into a dedicated helper, reducing `execute()` complexity while preserving cycle and state behavior. Added characterization tests to lock branch/page-cross behavior, BRK/RTI contracts, and WAI/STP control flow.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::execute_control_flow_opcode`
- `CPU65C02::execute`
- `branch_page_cross_behavior_preserved`
- `brk_rti_stack_and_flags_behavior_preserved`
- `wai_stp_control_flow_preserved`

**Tests created/changed:**

- `branch_page_cross_behavior_preserved`
- `brk_rti_stack_and_flags_behavior_preserved`
- `wai_stp_control_flow_preserved`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: extract execute control-flow family

- extract BRK/NOP/WAI/STP, jump, and branch opcodes into helper
- keep execute dispatch ordering and remaining opcode handling intact
- preserve cycle counts and state side effects for extracted opcodes
- add characterization tests for branch page-cross behavior
- add BRK/RTI and WAI/STP contract tests to guard refactor
