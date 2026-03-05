# Phase 2 Complete: Deduplicate ALU and RMW Opcode Bodies

Refactored ALU and RMW opcode handling in `CPU65C02::execute()` into shared family helpers to reduce duplication while preserving opcode behavior, cycle timing, and decode ordering. Added representative cycle and semantics characterization tests to guard this extraction.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::read_alu_operand_for_mode`
- `CPU65C02::execute_alu_family_opcode`
- `CPU65C02::read_rmw_target_for_mode`
- `CPU65C02::apply_rmw_family_op`
- `CPU65C02::execute_rmw_family_opcode`
- `CPU65C02::execute`
- `alu_family_dispatch_cycles_match_baseline`
- `rmw_family_dispatch_preserves_carry_nz_and_memory`

**Tests created/changed:**

- `alu_family_dispatch_cycles_match_baseline`
- `rmw_family_dispatch_preserves_carry_nz_and_memory`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: deduplicate cpu65c02 alu and rmw handlers

- extract ALU-family opcode execution into shared helper paths
- extract RMW-family opcode execution and operation application helpers
- preserve decode ordering, cycle timing, and page-cross behavior
- add ALU family cycle-baseline characterization coverage
- add RMW memory and flag contract characterization coverage
