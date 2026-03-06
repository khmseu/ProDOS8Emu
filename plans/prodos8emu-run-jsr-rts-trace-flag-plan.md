# Plan: Prodos8emu Run JSR/RTS Trace Flag

Add a dedicated CLI/runtime toggle to `prodos8emu_run` for the CPU JSR/RTS transition monitor without changing existing default behavior. The work introduces tests-first coverage for runner option parsing, then wires the flag into runner setup, and finally validates targeted regression suites.

**Phases 3**

1. **Phase 1: Add Failing Runner CLI Tests**
    - **Objective:** Establish tests-first coverage for the new runner flag contract before implementation.
    - **Files/Functions to Modify/Create:** `tests/prodos8emu_run_cli_test.cpp`, `CMakeLists.txt`
    - **Tests to Write:** `runner_help_lists_jsr_rts_trace_flag`, `runner_rejects_unknown_option_contract_stable`, `runner_accepts_jsr_rts_trace_flag`
    - **Steps:**
        1. Add a runner CLI test executable that invokes `prodos8emu_run` as a subprocess.
        2. Add assertions for help text, unknown option behavior stability, and new flag acceptance.
        3. Run targeted tests and confirm the new acceptance test fails before implementation.

2. **Phase 2: Implement Runner Flag Parsing and Runtime Wiring**
    - **Objective:** Add `--jsr-rts-trace` parsing and connect it to CPU runtime configuration.
    - **Files/Functions to Modify/Create:** `tools/prodos8emu_run.cpp`
    - **Tests to Write:** None; satisfy Phase 1 tests.
    - **Steps:**
        1. Add `jsr_rts_trace` option field with default `false`.
        2. Add `--jsr-rts-trace` help text and parse handling.
        3. Enable CPU monitor via `cpu.setJsrRtsTraceMonitorEnabled(opts.jsr_rts_trace)`.
        4. Re-run targeted tests and confirm all pass.

3. **Phase 3: Regression Gate and Documentation**
    - **Objective:** Confirm no regressions in CPU/startup suites and record completion artifacts.
    - **Files/Functions to Modify/Create:** `plans/prodos8emu-run-jsr-rts-trace-flag-phase-3-complete.md`, `plans/prodos8emu-run-jsr-rts-trace-flag-complete.md`
    - **Tests to Write:** None.
    - **Steps:**
        1. Run `prodos8emu_cpu65c02_tests`, `prodos8emu_emulator_startup_tests`, and new runner CLI tests.
        2. Confirm passing status and unchanged default runner behavior.
        3. Write phase and final completion docs.

**Open Questions 1**

1. Keep `--jsr-rts-trace` independent from `--debug` so future trace sinks remain possible; current effect is visible with trace logging enabled.
