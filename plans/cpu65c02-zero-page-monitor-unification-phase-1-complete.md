## Phase 1 Complete: Add Characterization Guardrails for Unified Triggers

Phase 1 added tests-only guardrails that characterize zero-page monitor trigger behavior across write families, enforce uniform all-write policy contracts for monitored fields, and lock baseline trace output formatting. No emulator implementation code changed in this phase.

**Files created/changed:**

- `plans/cpu65c02-zero-page-monitor-unification-plan.md`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `run_zp_monitor_trigger_matrix_contracts_test(int& failures)`
- `run_zp_monitor_all_writes_uniform_policy_contracts_test(int& failures)`
- `run_zp_monitor_trace_output_compatibility_baseline_test(int& failures)`
- `main()` registration updates

**Tests created/changed:**

- `zp_monitor_trigger_matrix_contracts`
- `zp_monitor_all_writes_uniform_policy_contracts`
- `zp_monitor_trace_output_compatibility_baseline`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
test: add zp monitor unification guardrails

- add zero-page write-family trigger matrix characterization tests
- add uniform all-write policy contracts for monitored ZP fields
- add trace output compatibility baseline for monitor delta formatting
- register Phase 1 monitor characterization tests in cpu test main
- validate cpu65c02 and emulator-startup ctest targets
