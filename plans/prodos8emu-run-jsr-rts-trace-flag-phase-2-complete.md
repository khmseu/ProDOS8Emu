# Phase 2 Complete: Implement Runner Flag Parsing and Runtime Wiring

Implemented `--jsr-rts-trace` in `prodos8emu_run` with default-off behavior and runtime wiring to the CPU monitor toggle. The runner now parses and reports the option, and enables the CPU JSR/RTS monitor only when requested.

**Files created/changed:**

- tools/prodos8emu_run.cpp
- plans/prodos8emu-run-jsr-rts-trace-flag-phase-2-complete.md

**Functions created/changed:**

- CliOptions (added `jsr_rts_trace` field)
- print_usage
- parse_args
- main

**Tests created/changed:**

- Reused existing Phase 1 tests (`prodos8emu_run_cli_tests`)
- Validation run: `ctest --test-dir build -R "prodos8emu_run_cli_tests|prodos8emu_cpu65c02_tests|prodos8emu_emulator_startup_tests" --output-on-failure`
- Result: 3/3 passed

**Review Status:** APPROVED

**Git Commit Message:**

feat: add --jsr-rts-trace runner option

- parse and expose jsr/rts trace toggle in prodos8emu_run
- wire runner option to CPU jsr/rts monitor enablement
- keep defaults and unknown-option behavior unchanged
