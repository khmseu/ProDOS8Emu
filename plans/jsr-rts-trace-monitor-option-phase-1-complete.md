## Phase 1 Complete: Add Failing Characterization Tests

Added a tests-first characterization suite for the new JSR/RTS transition monitor contract without changing production code. The new tests codify approved policy defaults (option off by default, include $DCB8 redirect behavior, exclude $BF00 MLI trap monitor emission) and establish the expected red-state before implementation.

**Files created/changed:**
- plans/jsr-rts-trace-monitor-option-plan.md
- tests/cpu65c02_test.cpp

**Functions created/changed:**
- run_jsr_rts_trace_monitor_default_off_compatibility_test
- run_jsr_rts_trace_monitor_enabled_normal_jsr_rts_logs_old_new_pc_test
- run_jsr_rts_trace_monitor_enabled_dcb8_redirect_behavior_test
- run_jsr_rts_trace_monitor_excludes_mli_trap_test

**Tests created/changed:**
- Test 83: jsr_rts_trace_monitor_default_off_compatibility
- Test 84: jsr_rts_trace_monitor_enabled_normal_jsr_rts_logs_old_new_pc
- Test 85: jsr_rts_trace_monitor_enabled_dcb8_redirect_behavior
- Test 86: jsr_rts_trace_monitor_excludes_mli_trap

**Review Status:** APPROVED

**Git Commit Message:**
test: add JSR/RTS trace monitor specs

- add four characterization tests for JSR/RTS monitor behavior
- codify default-off policy and exclude MLI trap transitions
- register new tests and verify expected pre-implementation red-state
