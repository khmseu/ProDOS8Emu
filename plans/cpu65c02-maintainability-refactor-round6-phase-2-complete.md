## Phase 2 Complete: Deduplicate Control-Flow Internals

Refactored control-flow internals to use shared helpers for source-PC derivation and PC transition application while preserving routing and instruction semantics. Added a dedicated equivalence characterization test for control-flow contracts and validated required CPU/startup targets.

**Files created/changed:**

- `src/cpu65c02.cpp`
- `include/prodos8emu/cpu65c02.hpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::control_flow_instruction_pc(uint8_t consumedOperandBytes)`
- `CPU65C02::apply_control_flow_pc_change(uint16_t fromPC, uint16_t toPC)`
- `CPU65C02::handle_mli_jsr_trap()`
- `CPU65C02::jsr_abs(uint16_t target)`
- `CPU65C02::execute_control_flow_jump_return_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_control_flow_opcode(uint8_t op, uint32_t& cycles)`
- `run_control_flow_refactor_equivalence_contracts_test(int& failures)`

**Tests created/changed:**

- `control_flow_refactor_equivalence_contracts`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: deduplicate control-flow internals

- extract shared control-flow helpers for source PC and PC transition updates
- route BRK/RTI/JSR/RTS/JMP and MLI trap internals through common helpers
- add control-flow refactor equivalence characterization coverage
- preserve control-flow routing, cycles, and recordPCChange semantics
- validate cpu and startup ctest targets to green
