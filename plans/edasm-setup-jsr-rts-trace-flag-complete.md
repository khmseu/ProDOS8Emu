# Plan Complete: Edasm Setup JSR/RTS Trace Flag Passthrough

Implemented end-to-end support in `tools/edasm_setup.py` for `--jsr-rts-trace` by parsing the option and forwarding it to `prodos8emu_run`. The change preserves existing defaults, follows tests-first delivery, and passed targeted regression gates spanning Python wrapper, runner CLI, CPU, and startup suites.

**Phases Completed:** 3 of 3

1. ✅ Phase 1: Add Failing Python Tests
2. ✅ Phase 2: Implement edasm_setup Flag Support
3. ✅ Phase 3: Regression Gate and Completion

**All Files Created/Modified:**

- tools/edasm_setup.py
- tests/python_edasm_setup_test.py
- plans/edasm-setup-jsr-rts-trace-flag-plan.md
- plans/edasm-setup-jsr-rts-trace-flag-phase-1-complete.md
- plans/edasm-setup-jsr-rts-trace-flag-phase-2-complete.md
- plans/edasm-setup-jsr-rts-trace-flag-phase-3-complete.md
- plans/edasm-setup-jsr-rts-trace-flag-complete.md

**Key Functions/Classes Added:**

- run_emulator (added `jsr_rts_trace` passthrough parameter)
- parse_args (added `--jsr-rts-trace` option)
- main (forwarding `args.jsr_rts_trace` to runner)
- TestRunEmulator.test_run_emulator_forwards_jsr_rts_trace_flag_when_enabled
- TestEndToEndMocking.test_parse_args_accepts_jsr_rts_trace_flag
- TestEndToEndMocking.test_parse_args_defaults_jsr_rts_trace_to_false

**Test Coverage:**

- Total tests written: 3
- All targeted tests passing: ✅

**Recommendations for Next Steps:**

- Optionally add `--jsr-rts-trace` to `edasm.sh` helper defaults when trace diagnostics are desired.
- Keep python and runner CLI tests in the targeted gate for future CLI wrapper changes.
