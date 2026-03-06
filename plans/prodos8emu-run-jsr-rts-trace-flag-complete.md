# Plan Complete: Prodos8emu Run JSR/RTS Trace Flag

Implemented a new `--jsr-rts-trace` runtime option for `prodos8emu_run` that enables the CPU JSR/RTS transition monitor when requested. The change was delivered with tests-first CLI coverage, runtime wiring, and targeted regression validation across runner CLI, CPU, and startup suites while preserving default behavior.

**Phases Completed:** 3 of 3

1. ✅ Phase 1: Add Failing Runner CLI Tests
2. ✅ Phase 2: Implement Runner Flag Parsing and Runtime Wiring
3. ✅ Phase 3: Regression Gate and Documentation

**All Files Created/Modified:**

- CMakeLists.txt
- tests/prodos8emu_run_cli_test.cpp
- tools/prodos8emu_run.cpp
- plans/prodos8emu-run-jsr-rts-trace-flag-plan.md
- plans/prodos8emu-run-jsr-rts-trace-flag-phase-1-complete.md
- plans/prodos8emu-run-jsr-rts-trace-flag-phase-2-complete.md
- plans/prodos8emu-run-jsr-rts-trace-flag-phase-3-complete.md
- plans/prodos8emu-run-jsr-rts-trace-flag-complete.md

**Key Functions/Classes Added:**

- runner_help_lists_jsr_rts_trace_flag
- runner_rejects_unknown_option_contract_stable
- runner_accepts_jsr_rts_trace_flag
- parse_args (`--jsr-rts-trace` handling)
- CPU runtime wiring in `main` (`setJsrRtsTraceMonitorEnabled`)

**Test Coverage:**

- Total tests written: 3
- All targeted tests passing: ✅

**Recommendations for Next Steps:**

- Optionally expose `--jsr-rts-trace` through `tools/edasm_setup.py` as a pass-through runner option.
- Keep `prodos8emu_run_cli_tests` in regular targeted gates for runner option changes.
