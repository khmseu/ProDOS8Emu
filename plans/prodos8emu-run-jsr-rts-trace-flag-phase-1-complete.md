# Phase 1 Complete: Add Failing Runner CLI Tests

Added tests-first CLI coverage for the new `--jsr-rts-trace` runner contract without changing production runner code. The new test target is wired into CTest and establishes the expected red-state before Phase 2 implementation.

**Files created/changed:**

- tests/prodos8emu_run_cli_test.cpp
- CMakeLists.txt
- plans/prodos8emu-run-jsr-rts-trace-flag-plan.md

**Functions created/changed:**

- runner_help_lists_jsr_rts_trace_flag
- runner_rejects_unknown_option_contract_stable
- runner_accepts_jsr_rts_trace_flag

**Tests created/changed:**

- prodos8emu_run_cli_tests (new target)
- runner_help_lists_jsr_rts_trace_flag (expected fail in Phase 1)
- runner_rejects_unknown_option_contract_stable (pass)
- runner_accepts_jsr_rts_trace_flag (expected fail in Phase 1)

**Review Status:** APPROVED

**Git Commit Message:**

test: add prodos8emu_run CLI flag specs

- add runner CLI subprocess tests for jsr/rts trace flag contract
- wire new prodos8emu_run_cli_tests target into CTest
- capture expected pre-implementation red-state for new flag checks
