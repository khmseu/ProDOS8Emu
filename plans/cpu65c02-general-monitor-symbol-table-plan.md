# Plan: CPU65C02 General Monitor Symbol Table

Introduce a single monitor symbol table in CPU65C02 where each symbol has address, name, and monitor flags (read/write/pc). Migrate existing monitoring to use this table and update JSR/RTS monitoring to resolve addresses through it while preserving default runtime behavior.

**Phases 4**

1. **Phase 1: Add Characterization Tests for Symbol-Table Contracts**
    - **Objective:** Define expected behavior before refactor.
    - **Files/Functions to Modify/Create:** `tests/cpu65c02_test.cpp`
    - **Tests to Write:**
        - JSR/RTS monitor symbol-lookup hit behavior
        - JSR/RTS monitor symbol-lookup miss fallback behavior
        - compatibility check: default-off JSR/RTS monitor unchanged
    - **Steps:**
        1. Add tests that describe the new symbol-table-based JSR/RTS contract.
        2. Run targeted tests and confirm red-state before implementation.
        3. Preserve existing marker and ZP monitoring baselines.

2. **Phase 2: Introduce Unified Symbol Table and Lookups**
    - **Objective:** Add internal symbol model and lookup helpers.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp`
    - **Tests to Write:** None new; satisfy Phase 1 failures.
    - **Steps:**
        1. Define internal symbol type and monitor flags (`read`, `write`, `pc`).
        2. Build unified symbol table entries for existing monitored addresses.
        3. Add helper lookups by address + flag.
        4. Re-run targeted tests and iterate to green.

3. **Phase 3: Migrate Existing Monitoring to Symbol Table**
    - **Objective:** Route ZP and marker monitoring through unified table and remove legacy parallel mappings.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp`, `tests/cpu65c02_test.cpp` (only if baseline adjustments are required)
    - **Tests to Write:** None new; validate existing monitoring suites.
    - **Steps:**
        1. Convert ZP write monitoring to symbol-table lookups.
        2. Convert PC marker emission to symbol-table lookups.
        3. Remove obsolete legacy mapping tables/helpers.
        4. Run targeted tests and preserve output formatting contracts.

4. **Phase 4: JSR/RTS Integration and Regression Gate**
    - **Objective:** Ensure JSR/RTS monitoring resolves symbol-table addresses and complete final regression validation.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp`, `plans/cpu65c02-general-monitor-symbol-table-phase-4-complete.md`, `plans/cpu65c02-general-monitor-symbol-table-complete.md`
    - **Tests to Write:** None new.
    - **Steps:**
        1. Integrate symbol-table lookup into JSR/RTS monitor output path.
        2. Keep fallback formatting stable when no symbol exists.
        3. Run regression gates:
            - `ctest --test-dir build -R "prodos8emu_cpu65c02_tests|prodos8emu_emulator_startup_tests" --output-on-failure`
            - `ctest --test-dir build -R "prodos8emu_run_cli_tests|python_edasm_setup_test" --output-on-failure`
        4. Write phase and final completion docs.

**Open Questions 2**

1. Default assumption for this implementation: append symbol names to JSR/RTS monitor lines only when lookup hits; keep exact fallback line unchanged when no symbol exists.
2. Default assumption for canonical marker mapping: treat current runtime mapping in `src/cpu65c02.cpp` as source of truth and align tests to it if conflicts are uncovered.
