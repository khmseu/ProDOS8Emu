# Plan: CPU65C02 Maintainability Refactor Round 5

Continue maintainability improvements in `src/cpu65c02.cpp` by locking remaining fallback/router behavior with tests first, then extracting routing logic and simplifying repetitive NOP handling with behavior-preserving transformations.

**Phases 3**

1. **Phase 1: Characterize Remaining Fallback and Router Edges**
   - **Objective:** Freeze under-tested fallback/default and NOP variant contracts before refactoring.
   - **Files/Functions to Modify/Create:**
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `unknown_opcode_default_fallback_contracts`
     - `nop_variant_opcode_matrix_preserved`
     - `fallback_router_precedence_contracts`
   - **Steps:**
     1. Add tests that assert cycles, PC movement, and side effects for fallback/default behavior.
     2. Validate NOP variant representative/opcode-matrix behavior and non-mutation contracts.
     3. Assert precedence around special decode and fallback routing contracts.
     4. Run targeted CPU tests to green with no production edits.

2. **Phase 2: Extract Execute Fallback Routing Helper**
   - **Objective:** Reduce `execute()` complexity by extracting fallback router logic into focused helper(s).
   - **Files/Functions to Modify/Create:**
     - `src/cpu65c02.cpp`
     - `include/prodos8emu/cpu65c02.hpp`
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `execute_fallback_router_dispatch_preserved`
   - **Steps:**
     1. Add/extend routing tests before extraction.
     2. Extract fallback routing while preserving decode order and helper call sequence.
     3. Keep cycle and side-effect semantics unchanged.
     4. Re-run CPU/startup targets.

3. **Phase 3: Data-Drive NOP Variant Handling**
   - **Objective:** Replace hard-coded NOP opcode lists with metadata-driven handling for maintainability.
   - **Files/Functions to Modify/Create:**
     - `src/cpu65c02.cpp`
     - `include/prodos8emu/cpu65c02.hpp`
     - `tests/cpu65c02_test.cpp`
   - **Tests to Write:**
     - `nop_variant_table_dispatch_equivalence`
   - **Steps:**
     1. Add a pre-refactor equivalence test for representative and edge NOP opcodes.
     2. Introduce static metadata table and minimal executor.
     3. Preserve bus-read shape, cycle counts, and PC advance behavior.
     4. Re-run CPU/startup targets to confirm parity.

**Open Questions 2**

1. Should NOP matrix tests be exhaustive for all currently handled NOP opcodes or representative-by-class?
2. Should fallback default-opcode tests assert only cycles/PC or also flag/register non-mutation explicitly?
