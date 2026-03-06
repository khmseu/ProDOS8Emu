# Plan: Edasm Setup JSR/RTS Trace Flag Passthrough

Teach `tools/edasm_setup.py` to accept `--jsr-rts-trace` and pass it through to `prodos8emu_run` without changing existing defaults. Work will follow tests-first, then implementation, then targeted regression validation.

**Phases 3**

1. **Phase 1: Add Failing Python Tests**
    - **Objective:** Define argparse and command-passthrough behavior before code changes.
    - **Files/Functions to Modify/Create:** `tests/python_edasm_setup_test.py`
    - **Tests to Write:**
        - parse args accepts `--jsr-rts-trace` and defaults false
        - `run_emulator` forwards `--jsr-rts-trace` when enabled
    - **Steps:**
        1. Add tests that assert the new flag contract.
        2. Run targeted python test suite and confirm red-state before implementation.

2. **Phase 2: Implement edasm_setup Flag Support**
    - **Objective:** Parse and forward `--jsr-rts-trace` to runner invocation.
    - **Files/Functions to Modify/Create:** `tools/edasm_setup.py`
    - **Tests to Write:** None new; satisfy Phase 1.
    - **Steps:**
        1. Add `--jsr-rts-trace` to argparse in `parse_args`.
        2. Extend `run_emulator` signature with `jsr_rts_trace` (default false).
        3. Append `--jsr-rts-trace` to runner command when enabled.
        4. Pass `args.jsr_rts_trace` from `main()` into `run_emulator()`.
        5. Re-run targeted tests to green.

3. **Phase 3: Regression Gate and Completion Docs**
    - **Objective:** Confirm no regressions and record completion artifacts.
    - **Files/Functions to Modify/Create:** `plans/edasm-setup-jsr-rts-trace-flag-phase-3-complete.md`, `plans/edasm-setup-jsr-rts-trace-flag-complete.md`
    - **Tests to Write:** None.
    - **Steps:**
        1. Run `python_edasm_setup_test` and runner/cpu targeted gate.
        2. Confirm pass status and stable default behavior.
        3. Write phase and final completion docs.

**Open Questions 1**

1. Keep this as pure passthrough independent of `--debug` (recommended).
