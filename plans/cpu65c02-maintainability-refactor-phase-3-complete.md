## Phase 3 Complete: Isolate JSR Trap Handling

Extracted MLI trap behavior from `jsr_abs` into a dedicated helper to reduce mixed responsibilities while preserving normal JSR semantics and existing trap behavior contracts. Added focused characterization tests for flags, carry behavior, and QUIT stop semantics.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::handle_mli_jsr_trap`
- `CPU65C02::jsr_abs`
- `mli_trap_sets_a_c_z_and_clears_d_as_expected`
- `mli_error_sets_carry_success_clears_carry`
- `quit_stops_cpu_non_quit_does_not_stop`

**Tests created/changed:**

- `mli_trap_sets_a_c_z_and_clears_d_as_expected`
- `mli_error_sets_carry_success_clears_carry`
- `quit_stops_cpu_non_quit_does_not_stop`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED with minor recommendations

**Git Commit Message:**
refactor: isolate MLI JSR trap handling

- extract MLI trap logic from jsr_abs into dedicated helper
- preserve normal JSR stack and PC behavior for non-trap paths
- keep trap contracts for A/C/Z/N flags and decimal-clear semantics
- preserve QUIT stop behavior and existing log output ordering
- add focused characterization tests for trap and stop contracts
