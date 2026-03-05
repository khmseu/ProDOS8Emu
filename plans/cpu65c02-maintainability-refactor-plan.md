## Plan: CPU65C02 Maintainability Refactor

Improve maintainability of CPU emulation code by adding characterization tests first, then refactoring in small behavior-preserving slices. The sequence minimizes regression risk by locking existing observable behavior before structural changes.

**Phases 5**

1. **Phase 1: Characterization Test Lock-In**
    - **Objective:** Freeze current externally observable CPU/MLI/COUT behavior to protect refactors.
    - **Files/Functions to Modify/Create:**
      - `tests/cpu65c02_test.cpp`
      - `tests/emulator_startup_test.cpp` (only if additional coverage is needed)
    - **Tests to Write:**
      - `mli_detached_jsr_abs_behaves_as_normal_jsr`
      - `mli_quit_and_non_quit_stop_contract`
      - `mli_log_contains_expected_field_order_for_open`
      - `mli_flags_contract_after_trap_return`
    - **Steps:**
        1. Add new failing characterization tests for current behavior in CPU/MLI paths.
        2. Run targeted CPU tests to verify failures are meaningful.
        3. Apply minimal adjustments only if needed to align test fixture assumptions.
        4. Re-run targeted tests until green.

2. **Phase 2: Extract Pure Logging/Path Helpers**
    - **Objective:** Reduce complexity by extracting pure/parsing helpers from large logging functions without behavior changes.
    - **Files/Functions to Modify/Create:**
      - `src/cpu65c02.cpp` (`extract_mli_pathnames`, pathname parsing/formatting helpers)
      - `include/prodos8emu/cpu65c02.hpp` (private helper declarations if needed)
    - **Tests to Write:**
      - `extract_pathname_len_zero_formats_empty`
      - `extract_pathname_invalid_length_formats_error`
      - `mli_get_prefix_logging_remains_stable`
    - **Steps:**
        1. Add failing tests covering edge-case formatting.
        2. Extract small pure helpers for pathname decoding and entry formatting.
        3. Keep output strings identical unless explicitly cleaned up by approved scope.
        4. Run CPU tests and fix only regressions introduced by extraction.

3. **Phase 3: Isolate JSR Trap Handling**
    - **Objective:** Separate normal JSR behavior from trap logic for readability and easier testing.
    - **Files/Functions to Modify/Create:**
      - `src/cpu65c02.cpp` (`jsr_abs`, extracted trap helper)
      - `include/prodos8emu/cpu65c02.hpp` (private helper declarations)
    - **Tests to Write:**
      - `mli_trap_sets_a_c_z_and_clears_d_as_expected`
      - `mli_error_sets_carry_success_clears_carry`
      - `quit_stops_cpu_non_quit_does_not_stop`
    - **Steps:**
        1. Add failing tests for register/flag contract and stop behavior.
        2. Extract MLI trap path into dedicated helper while preserving call sites.
        3. Keep normal JSR stack/PC semantics unchanged.
        4. Run targeted tests and resolve only behavior regressions from this phase.

4. **Phase 4: Decompose execute() by Opcode Families**
    - **Objective:** Improve readability by moving opcode-family logic into smaller helpers while preserving cycles and side effects.
    - **Files/Functions to Modify/Create:**
      - `src/cpu65c02.cpp` (`execute`, extracted opcode-family helpers)
      - `include/prodos8emu/cpu65c02.hpp` (private helper declarations)
    - **Tests to Write:**
      - `branch_page_cross_behavior_preserved`
      - `brk_rti_stack_and_flags_behavior_preserved`
      - `wai_stp_control_flow_preserved`
    - **Steps:**
        1. Add failing tests around sensitive instruction behavior.
        2. Extract one opcode family at a time (branches, loads/stores, ALU, shifts).
        3. Re-run CPU tests after each extraction slice.
        4. Keep instruction semantics and cycle accounting unchanged.

5. **Phase 5: Separate step() Trace Instrumentation**
    - **Objective:** Improve `step()` maintainability by moving EdAsm trace and delta logging into focused helper(s).
    - **Files/Functions to Modify/Create:**
      - `src/cpu65c02.cpp` (`step`, trace helper extraction)
      - `include/prodos8emu/cpu65c02.hpp` (private helper declarations)
      - `tests/cpu65c02_test.cpp` (trace behavior tests)
    - **Tests to Write:**
      - `trace_markers_emit_for_known_entry_points`
      - `passnbr_genf_listing_deltas_logged_consistently`
      - `trace_disabled_emits_no_trace_lines`
    - **Steps:**
        1. Add failing tests for trace output behavior.
        2. Extract trace marker and flag-delta logic into helper(s).
        3. Keep instruction execution path untouched.
        4. Run targeted and integration tests to ensure no behavioral drift.

**Open Questions 2**

1. Scope approved as “all phases” by user confirmation.
2. Log string cleanup is allowed provided behavior-critical semantics remain unchanged and tests stay green.
