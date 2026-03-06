# Plan Complete: JSR/RTS Trace Monitor Option

Implemented a dedicated, opt-in JSR/RTS transition monitor that logs old/new PC transitions in trace output without changing default trace behavior. The feature integrates at normal JSR/RTS control-flow points, excludes MLI trap transitions by policy, and preserves compatibility baselines validated by existing CPU/startup suites. The work was delivered in tests-first phases with review gates and targeted regression validation.

**Phases Completed:** 3 of 3

1. ✅ Phase 1: Add Failing Characterization Tests
2. ✅ Phase 2: Implement Toggle and JSR/RTS Trace Emission
3. ✅ Phase 3: Regression Gate and Compatibility Confirmation

**All Files Created/Modified:**

- tests/cpu65c02_test.cpp
- include/prodos8emu/cpu65c02.hpp
- src/cpu65c02.cpp
- plans/jsr-rts-trace-monitor-option-plan.md
- plans/jsr-rts-trace-monitor-option-phase-1-complete.md
- plans/jsr-rts-trace-monitor-option-phase-2-complete.md
- plans/jsr-rts-trace-monitor-option-phase-3-complete.md
- plans/jsr-rts-trace-monitor-option-complete.md

**Key Functions/Classes Added:**

- CPU65C02::setJsrRtsTraceMonitorEnabled
- CPU65C02::log_jsr_rts_transition
- run_jsr_rts_trace_monitor_default_off_compatibility_test
- run_jsr_rts_trace_monitor_enabled_normal_jsr_rts_logs_old_new_pc_test
- run_jsr_rts_trace_monitor_enabled_dcb8_redirect_behavior_test
- run_jsr_rts_trace_monitor_excludes_mli_trap_test

**Test Coverage:**

- Total tests written: 4
- All tests passing: ✅

**Recommendations for Next Steps:**

- Optionally expose the new monitor toggle through the emulator runner CLI when trace output is enabled.
- Keep Test 83-86 in the CPU gate for future control-flow refactors.
