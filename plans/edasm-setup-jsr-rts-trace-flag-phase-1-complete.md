# Phase 1 Complete: Add Failing Python Tests

Added tests-first coverage for `--jsr-rts-trace` in `edasm_setup.py` without changing production code. The new tests establish expected red-state for missing argparse and runner passthrough support before Phase 2 implementation.

**Files created/changed:**

- tests/python_edasm_setup_test.py
- plans/edasm-setup-jsr-rts-trace-flag-plan.md
- plans/edasm-setup-jsr-rts-trace-flag-phase-1-complete.md

**Functions created/changed:**

- TestRunEmulator.test_run_emulator_forwards_jsr_rts_trace_flag_when_enabled
- TestEndToEndMocking.test_parse_args_accepts_jsr_rts_trace_flag
- TestEndToEndMocking.test_parse_args_defaults_jsr_rts_trace_to_false

**Tests created/changed:**

- python_edasm_setup_test
- Validation run: `ctest --test-dir build -R "python_edasm_setup_test" --output-on-failure`
- Result: expected red-state (`1 failure`, `2 errors`) prior to implementation

**Review Status:** APPROVED

**Git Commit Message:**

test: add edasm_setup jsr/rts flag specs

- add parse_args tests for jsr/rts trace flag true/default behavior
- add run_emulator passthrough test for --jsr-rts-trace
- capture expected pre-implementation red-state for python suite
