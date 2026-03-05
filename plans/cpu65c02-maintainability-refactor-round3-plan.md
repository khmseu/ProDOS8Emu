# Plan: CPU65C02 Maintainability Refactor Round 3

Continue maintainability improvements in `src/cpu65c02.cpp` by adding missing characterization coverage for remaining fallback opcode paths first, then extracting low-risk opcode groups in small slices. The sequence prioritizes behavior safety and decode-order stability.

**Phases 3**

1. **Phase 1: Characterize Remaining Fallback Contracts**
   - **Objective:** Lock behavior for under-tested fallback opcode groups before refactoring.
   - **Files/Functions to Modify/Create:**
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `execute_flag_transfer_stack_contracts`
     - `bit_tsb_trb_flag_and_writeback_contracts`
     - `undocumented_nop_cycles_and_pc_advance_contracts`
   - **Steps:**
     1. Add failing characterization tests for fallback opcode contracts.
     2. Run `prodos8emu_cpu65c02_tests` to validate coverage intent.
     3. Correct only fixture assumptions if needed; avoid production changes.
     4. Re-run tests to green and freeze behavior.

2. **Phase 2: Extract Low-Risk Fallback Opcode Groups**
   - **Objective:** Reduce duplication by extracting flags/transfers/stack and accumulator-opcode handlers from fallback switch.
   - **Files/Functions to Modify/Create:**
     - `src/cpu65c02.cpp`
     - `include/prodos8emu/cpu65c02.hpp`
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `low_risk_group_dispatch_cycles_preserved`
   - **Steps:**
     1. Add/extend tests that lock cycles and state for extracted groups.
     2. Extract one low-risk helper group at a time.
     3. Preserve decode ordering and public API.
     4. Run CPU and startup test targets after each extraction slice.

3. **Phase 3: Extract Addressing-Heavy Groups from Fallback**
   - **Objective:** Improve readability of remaining fallback regions (loads/stores, BIT family, NOP variants) while preserving behavior.
   - **Files/Functions to Modify/Create:**
     - `src/cpu65c02.cpp`
     - `include/prodos8emu/cpu65c02.hpp`
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `load_store_addressing_cycle_matrix_preserved`
     - `bit_family_page_cross_contracts`
     - `nop_bus_read_shape_contracts`
   - **Steps:**
     1. Add tests for decode-order and cycle/flag contracts in targeted groups.
     2. Extract helper paths incrementally with no decode-precedence changes.
     3. Keep fallback semantics identical for undocumented variants.
     4. Run targeted CPU and startup tests to confirm no regressions.

**Open Questions 2**

1. Should cycle assertions in Phase 3 be strict exact counts per opcode variant or grouped invariants?
2. Should any undocumented-opcode naming cleanup be deferred entirely to avoid log/output churn?
