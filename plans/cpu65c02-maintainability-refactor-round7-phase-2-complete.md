## Phase 2 Complete: Refactor Flag/Transfer/Stack and Accumulator Paths

Phase 2 extracted helper-driven execution paths for flag, transfer, stack, and accumulator misc opcodes while preserving existing dispatch precedence and runtime behavior. Added characterization-equivalence tests lock parity for cycle counts, PC transitions, status-flag semantics, stack ordering, and register/memory side effects.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::execute_flag_transfer_stack_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_flag_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_transfer_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_stack_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_accumulator_misc_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_accumulator_inc_dec_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::execute_accumulator_shift_rotate_opcode(uint8_t op, uint32_t& cycles)`

**Tests created/changed:**

- `flag_transfer_stack_helper_dispatch_equivalence`
- `accumulator_misc_helper_dispatch_equivalence`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: extract flag stack and accumulator helpers

- split flag/transfer/stack opcode handling into focused helper paths
- route accumulator misc opcodes through dedicated helper methods
- add equivalence tests for helper dispatch and behavior contracts
- preserve decode precedence, cycles, flags, and stack semantics
- validate cpu65c02 and emulator-startup ctest targets
