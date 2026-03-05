# Plan: CPU65C02 Maintainability Refactor Round 4

Improve maintainability of the remaining `execute()` tail and control-flow complexity in `src/cpu65c02.cpp` using tests-first, small-slice extractions. The plan prioritizes behavior safety by locking uncovered contracts before refactoring.

**Phases 3**

1. **Phase 1: Characterize Execute-Tail and Bit-Branch Edges**
   - **Objective:** Lock under-tested fallback and branch edge behavior before code movement.
   - **Files/Functions to Modify/Create:**
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `register_incdec_nz_cycle_contracts`
     - `cpx_cpy_zp_abs_cycle_flag_contracts`
     - `bbr_bbs_not_taken_and_page_cross_contracts`
   - **Steps:**
     1. Add tests that assert cycles, PC advance, and flag side effects.
     2. Run targeted CPU tests and correct only fixture assumptions if needed.
     3. Keep this phase tests-only (no production code edits).
     4. Re-run tests to green.

2. **Phase 2: Extract Execute Tail Low-Risk Groups**
   - **Objective:** Reduce fallback switch complexity by extracting register inc/dec and CPX/CPY groups.
   - **Files/Functions to Modify/Create:**
     - `src/cpu65c02.cpp`
     - `include/prodos8emu/cpu65c02.hpp`
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `misc_tail_dispatch_cycles_preserved`
     - `compare_xy_dispatch_contracts_preserved`
   - **Steps:**
     1. Add tests for extracted-group behavior and cycles.
     2. Extract one helper at a time for low-risk groups.
     3. Preserve decode precedence and fallback semantics.
     4. Validate against CPU and startup test targets.

3. **Phase 3: Split Control-Flow by Concern**
   - **Objective:** Improve readability of `execute_control_flow_opcode` by extracting branch/jump-return/COUT subhelpers.
   - **Files/Functions to Modify/Create:**
     - `src/cpu65c02.cpp`
     - `include/prodos8emu/cpu65c02.hpp`
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `branch_opcode_matrix_preserved`
     - `cout_escape_mapping_contracts_preserved`
   - **Steps:**
     1. Add tests covering branch matrix and COUT escape behavior.
     2. Extract focused control-flow helpers without semantic change.
     3. Keep cycle and PC behavior unchanged.
     4. Re-run targeted tests to green.

**Open Questions 2**

1. Should branch matrix tests include all 8 branch opcodes in both taken and not-taken forms?
2. Should COUT tests lock exact full output strings or only stable token/escape semantics?
