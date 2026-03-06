## Plan: CPU65C02 Maintainability Refactor Round 8

Round 8 continues tests-first maintainability by reducing the remaining dense decode/log/trace complexity in `CPU65C02` while preserving exact runtime semantics. This round proceeds from low-risk characterization and deduplication to medium-risk decomposition/tableization with strict targeted CTest gating.

**Phases 5**

1. **Phase 1: Expand Guardrail Characterization**
    - **Objective:** Add tests-first safety coverage for branch decode parity, MLI error-path log ordering, and trace delta stability before behavior-preserving refactors.
    - **Files/Functions to Modify/Create:** `tests/cpu65c02_test.cpp` (`run_control_flow_branch_decode_table_equivalence_test`, `run_mli_error_path_logging_order_stable_test`, `run_trace_dsklistf_delta_logged_consistently_test`, `main` registration)
    - **Tests to Write:** `control_flow_branch_decode_table_equivalence`, `mli_error_path_logging_order_stable`, `trace_dsklistf_delta_logged_consistently`
    - **Steps:**
        1. Add characterization tests for branch decode mapping and log output field order.
        2. Run targeted test gate to validate baseline behavior contracts.
        3. Adjust fixtures only as needed to match current behavior and re-run to pass.

2. **Phase 2: Unify Relative Branch Application**
    - **Objective:** Extract a shared relative-branch apply helper used by `branch()` and BBR/BBS execution path to remove duplicated page-cross and PC-change logic.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp` (`branch`, `execute_bbr_bbs_opcode`, new helper), `include/prodos8emu/cpu65c02.hpp` (helper declaration), `tests/cpu65c02_test.cpp` (equivalence contracts)
    - **Tests to Write:** `relative_branch_apply_helper_equivalence`
    - **Steps:**
        1. Add parity tests for taken/not-taken and page-cross contracts across branch + BBR/BBS routes.
        2. Run targeted tests and capture any behavior mismatches before code changes.
        3. Implement minimal shared helper extraction and re-run tests to green.

3. **Phase 3: Table-Drive Control-Flow Branch Decode**
    - **Objective:** Replace manual branch opcode switch logic with metadata/classifier routing while preserving decode precedence and cycle behavior.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp` (`execute_control_flow_branch_opcode`, branch metadata/classifier helpers), `include/prodos8emu/cpu65c02.hpp` (helper declarations if needed), `tests/cpu65c02_test.cpp` (route-equivalence characterization)
    - **Tests to Write:** `control_flow_branch_table_dispatch_equivalence`, `control_flow_branch_precedence_nonregression`
    - **Steps:**
        1. Add decode-table equivalence tests first.
        2. Implement metadata-driven branch dispatch with minimal routing change.
        3. Re-run targeted validation to confirm unchanged contracts.

4. **Phase 4: Decompose MLI Trap and Log Assembly**
    - **Objective:** Split MLI trap dispatch/result handling from log string assembly for cleaner responsibilities while preserving byte-for-byte compatible output.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp` (`handle_mli_jsr_trap`, `extract_mli_*` helpers, message assembly helpers), `include/prodos8emu/cpu65c02.hpp` (helper declarations), optionally new module for pure log helpers if unchanged behavior is maintained, `tests/cpu65c02_test.cpp`
    - **Tests to Write:** `mli_trap_log_builder_equivalence`, `mli_trap_result_flag_contract_nonregression`
    - **Steps:**
        1. Add tests locking output order/content and status flag contracts.
        2. Extract focused helpers (or optional separate module) while keeping trap semantics unchanged.
        3. Re-run targeted tests to verify output and behavior parity.

5. **Phase 5: Tableize Trace Marker and Delta Emission**
    - **Objective:** Convert hard-coded trace marker/delta switch paths to table-driven emission while preserving textual output compatibility.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp` (`log_step_trace_marker`, `log_step_trace_flag_deltas`, marker metadata tables), `tests/cpu65c02_test.cpp`
    - **Tests to Write:** `trace_marker_table_equivalence`, `trace_flag_delta_output_equivalence`
    - **Steps:**
        1. Add exact-output characterization tests for marker and delta lines.
        2. Implement table-driven marker/delta mapping.
        3. Re-run targeted validation and confirm stable output.

**Open Questions 2**

1. Log compatibility policy: byte-for-byte output compatibility? **Allowed: Yes**
2. Scope policy for MLI helper extraction: allow moving pure logging helpers into a new module if behavior remains identical? **Allowed: Yes**
