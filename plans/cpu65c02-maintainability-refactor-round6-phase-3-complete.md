## Phase 3 Complete: Extract Fallback Family Classifier

Replaced large manual opcode-list routing in `execute_fallback_router_opcode` with a classifier-driven routing layer while preserving decode precedence and unknown/default fallback semantics. Added full-range classifier equivalence characterization to ensure routing parity against legacy membership rules, and validated required CPU/startup test targets.

**Files created/changed:**

- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `is_fallback_misc_tail_opcode(uint8_t op)`
- `is_fallback_alu_rmw_group(uint8_t op)`
- `is_fallback_alu_mode(uint8_t op)`
- `is_fallback_compare_mode(uint8_t op)`
- `is_fallback_rmw_mode(uint8_t op)`
- `classify_fallback_route(uint8_t op)`
- `CPU65C02::execute_fallback_router_opcode(uint8_t op)`
- `run_fallback_classifier_equivalence_matrix_test(int& failures)`

**Tests created/changed:**

- `fallback_classifier_equivalence_matrix`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: classify fallback router opcode families

- replace fallback opcode list routing with classifier-driven dispatch
- add classifier helpers for misc, ALU, compare, and RMW family detection
- add full-range fallback classifier equivalence characterization test
- preserve decode precedence and unknown/default fallback semantics
- validate cpu and startup ctest targets to green
