## Phase 2 Complete: Unify Relative Branch Application

Phase 2 extracted shared relative-branch application logic and routed both `branch()` and BBR/BBS branch execution through the same helper path. This removes duplicated PC/page-cross logic while preserving cycle contracts, branch outcomes, and PC-change recording behavior.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::apply_relative_branch_offset(int8_t rel)`
- `CPU65C02::branch(bool cond)`
- `CPU65C02::execute_bbr_bbs_opcode(uint8_t op)`
- `run_relative_branch_apply_helper_equivalence_test(int& failures)`

**Tests created/changed:**

- `relative_branch_apply_helper_equivalence`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: unify relative branch application

- add shared helper for relative branch PC/page-cross application
- route branch and BBR/BBS paths through shared branch helper
- add characterization coverage for helper equivalence contracts
- preserve cycle counts, branch outcomes, and decode precedence
- validate cpu65c02 and emulator-startup ctest targets
