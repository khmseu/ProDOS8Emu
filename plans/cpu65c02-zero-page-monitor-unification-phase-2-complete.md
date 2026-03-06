## Phase 2 Complete: Implement Single Zero-Page Write Monitor Hook

Phase 2 introduced a unified zero-page monitor mechanism triggered at the CPU write funnel (`write8`) and routed per-instruction monitor emission through one consolidated step-time event path. This enforces the approved all-write policy for monitored fields while preserving trace output formatting contracts under updated baselines.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::write8(uint16_t addr, uint8_t v)`
- `begin_step_zp_monitor_capture(...)`
- `end_step_zp_monitor_capture(...)`
- `append_step_zp_monitor_event(...)`
- `log_step_zp_monitor_events(...)`
- `CPU65C02::step()` (capture start/flush integration)
- `run_zp_monitor_write8_hook_equivalence_test(int& failures)`
- `run_zp_monitor_uniform_emission_nonregression_test(int& failures)`

**Tests created/changed:**

- `zp_monitor_write8_hook_equivalence`
- `zp_monitor_uniform_emission_nonregression`
- Updated: `trace_flag_delta_output_equivalence` baseline for all-write policy
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: unify zp monitor write trigger

- hook monitored zero-page delta capture directly in write8
- centralize per-step zp monitor event buffering and emission
- enforce uniform all-write monitor policy for tracked zp fields
- add write8-hook equivalence and uniform emission nonregression tests
- validate cpu65c02 and emulator-startup ctest targets
