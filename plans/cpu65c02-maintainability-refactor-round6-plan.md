## Plan: CPU65C02 Maintainability Refactor Round 6

Continue maintainability improvements in `src/cpu65c02.cpp` by adding stronger characterization guardrails first, then performing low-risk extractions in control-flow, fallback routing classification, and load/store dispatch while preserving cycle, PC, flag, and side-effect semantics.

**Phases 4**

1. **Phase 1: Characterize Control-Flow, Router, and Load/Store Edges**
    - **Objective:** Add focused tests that lock critical decode ordering and under-covered opcode contracts before any production refactor.
    - **Files/Functions to Modify/Create:**
        - `tests/cpu65c02_test.cpp`
    - **Tests to Write:**
        - `control_flow_jump_matrix_preserved`
        - `load_store_opcode_completeness_matrix_preserved`
        - `store_indexed_page_cross_cycle_contracts`
        - `fallback_router_family_membership_contracts`
        - `execute_decode_precedence_nonregression`
    - **Steps:**
        1. Add characterization tests for jump/indirect jump and decode precedence contracts.
        2. Add load/store matrix coverage for underrepresented addressing modes and cycle behavior.
        3. Add fallback family membership routing contracts for representative opcode groups.
        4. Run targeted CPU/startup test targets to green with no production edits.

2. **Phase 2: Deduplicate Control-Flow Internals**
    - **Objective:** Reduce duplication in control-flow handling helpers without changing opcode routing or semantics.
    - **Files/Functions to Modify/Create:**
        - `src/cpu65c02.cpp`
        - `include/prodos8emu/cpu65c02.hpp`
        - `tests/cpu65c02_test.cpp`
    - **Tests to Write:**
        - `control_flow_refactor_equivalence_contracts`
    - **Steps:**
        1. Add/extend control-flow equivalence tests first.
        2. Extract small shared helpers for repeated branch/jump-return patterns.
        3. Preserve BRK/RTI/JSR/RTS/COUT contracts, cycles, and PC transition logging.
        4. Run targeted CPU/startup tests to confirm parity.

3. **Phase 3: Extract Fallback Family Classifier**
    - **Objective:** Replace long manual opcode lists in fallback routing with classifier helper(s)/metadata while preserving route selection.
    - **Files/Functions to Modify/Create:**
        - `src/cpu65c02.cpp`
        - `include/prodos8emu/cpu65c02.hpp`
        - `tests/cpu65c02_test.cpp`
    - **Tests to Write:**
        - `fallback_classifier_equivalence_matrix`
    - **Steps:**
        1. Add classifier-equivalence characterization tests for representative ALU/RMW/compare/misc opcodes.
        2. Introduce classifier helper(s) and switch routing to use them.
        3. Preserve default/unknown opcode fallback behavior and decode precedence.
        4. Run targeted CPU/startup tests to green.

4. **Phase 4: Extract Load/Store Addressing Helpers**
    - **Objective:** Decompose the large load/store switch into small addressing helpers while preserving per-opcode behavior.
    - **Files/Functions to Modify/Create:**
        - `src/cpu65c02.cpp`
        - `include/prodos8emu/cpu65c02.hpp`
        - `tests/cpu65c02_test.cpp`
    - **Tests to Write:**
        - `load_store_helper_dispatch_equivalence`
    - **Steps:**
        1. Add pre-refactor equivalence tests for loads/stores across representative addressing modes.
        2. Extract minimal helpers grouped by addressing-mode behavior.
        3. Preserve cycles, NZ updates (loads only), and exact store write targets.
        4. Run targeted CPU/startup tests to confirm no regressions.

**Open Questions 2**

1. Should Phase 4 include optional compile-time opcode metadata for load/store, or stay helper-only to keep diffs smaller?
2. For fallback classifier in Phase 3, should we prioritize constexpr table membership or predicate-based bit-pattern grouping where possible?
