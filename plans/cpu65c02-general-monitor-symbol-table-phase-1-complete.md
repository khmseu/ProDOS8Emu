# Phase 1 Complete: Characterization Tests for Symbol-Table Contracts

Added tests-first characterization for the planned general monitor symbol table behavior on JSR/RTS trace output. The new tests define lookup-hit and lookup-miss contracts while preserving existing default-off compatibility coverage, and establish expected red-state before implementation.

**Files created/changed:**

- tests/cpu65c02_test.cpp
- plans/cpu65c02-general-monitor-symbol-table-plan.md
- plans/cpu65c02-general-monitor-symbol-table-phase-1-complete.md

**Functions created/changed:**

- run_jsr_rts_trace_monitor_symbol_lookup_hit_contract_test
- run_jsr_rts_trace_monitor_symbol_lookup_miss_fallback_contract_test

**Tests created/changed:**

- Test 87: jsr_rts_trace_monitor_symbol_lookup_hit_contract
- Test 88: jsr_rts_trace_monitor_symbol_lookup_miss_fallback_contract
- Existing compatibility retained: run_jsr_rts_trace_monitor_default_off_compatibility_test
- Validation run: `ctest --test-dir build -R "prodos8emu_cpu65c02_tests|prodos8emu_emulator_startup_tests" --output-on-failure`
- Result: expected red-state for missing implementation path (Test 87), with baseline-existing marker-table mismatch still present

**Review Status:** APPROVED (baseline-aware)

**Git Commit Message:**

test: add symbol-table monitor contracts

- add JSR/RTS symbol lookup hit characterization test
- add JSR/RTS symbol lookup miss fallback characterization test
- preserve default-off monitor compatibility coverage
