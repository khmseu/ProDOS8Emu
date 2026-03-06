## Phase 4 Complete: Decompose MLI Trap and Log Assembly

Phase 4 extracted pure MLI trap log message assembly from trap execution flow to reduce mixed responsibilities while preserving runtime behavior and output compatibility. Added regression characterization for trap log builder equivalence and trap result/stop-state contracts.

**Files created/changed:**

- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `build_mli_trap_log_message(...)` (anonymous namespace helper)
- `CPU65C02::handle_mli_jsr_trap()`
- `run_mli_trap_log_builder_equivalence_test(int& failures)`
- `run_mli_trap_result_flag_contract_nonregression_test(int& failures)`

**Tests created/changed:**

- `mli_trap_log_builder_equivalence`
- `mli_trap_result_flag_contract_nonregression`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: split mli trap log assembly

- extract pure MLI trap log message builder from trap execution flow
- keep trap return flags and quit/non-quit stop behavior unchanged
- add MLI log builder and trap-result nonregression characterization tests
- preserve byte-for-byte MLI logging compatibility contracts
- validate cpu65c02 and emulator-startup ctest targets
