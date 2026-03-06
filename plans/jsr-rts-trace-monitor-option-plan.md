## Plan: JSR/RTS Trace Monitor Option

Add an opt-in trace monitor that logs old/new PC transitions for JSR and RTS while preserving existing trace behavior by default. Implementation will hook into existing JSR/RTS control-flow points and keep MLI trap tracing unchanged unless explicitly expanded later.

**Phases 3**
1. **Phase 1: Add Failing Characterization Tests**
    - **Objective:** Define expected JSR/RTS monitor behavior and output format before implementation.
    - **Files/Functions to Modify/Create:** `tests/cpu65c02_test.cpp` (new `run_jsr_rts_trace_monitor_*` tests, `main` registration)
    - **Tests to Write:** `run_jsr_rts_trace_monitor_default_off_compatibility_test`, `run_jsr_rts_trace_monitor_enabled_normal_jsr_rts_logs_old_new_pc_test`, `run_jsr_rts_trace_monitor_enabled_dcb8_redirect_behavior_test`, `run_jsr_rts_trace_monitor_excludes_mli_trap_test`
    - **Steps:**
        1. Add new trace-monitor characterization tests with exact output assertions.
        2. Register new tests in `main` near current trace test registrations.
        3. Run targeted CPU tests to confirm new tests fail before implementation.

2. **Phase 2: Implement Toggle and JSR/RTS Trace Emission**
    - **Objective:** Add dedicated API toggle and emit monitor lines for JSR/RTS transitions when enabled.
    - **Files/Functions to Modify/Create:** `include/prodos8emu/cpu65c02.hpp`, `src/cpu65c02.cpp`
    - **Tests to Write:** None new; satisfy Phase 1 tests.
    - **Steps:**
        1. Add public setter and private enabled flag (default off).
        2. Add helper to emit JSR/RTS old/new PC trace line in existing style.
        3. Wire helper into normal JSR and RTS paths; keep MLI trap excluded.
        4. Run targeted CPU tests until all pass.

3. **Phase 3: Regression Gate and Compatibility Confirmation**
    - **Objective:** Confirm no regressions and preserve legacy output when monitor is disabled.
    - **Files/Functions to Modify/Create:** `tests/cpu65c02_test.cpp` and/or `src/cpu65c02.cpp` only if fixes are needed.
    - **Tests to Write:** None unless gap is discovered.
    - **Steps:**
        1. Run `prodos8emu_cpu65c02_tests` and `prodos8emu_emulator_startup_tests`.
        2. Verify existing trace marker and zero-page monitor baselines remain stable.
        3. Prepare completion artifacts and commit guidance.

**Open Questions 1**
1. None. Resolved choices: option default `off`, MLI trap transitions `excluded`, pre-existing change committed separately (`be1b090`).
