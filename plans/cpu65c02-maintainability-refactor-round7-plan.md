## Plan: CPU65C02 Maintainability Refactor Round 7

Round 7 continues the tests-first decomposition of `CPU65C02` by hardening opcode-family characterization first, then extracting low-risk duplicated execution paths before higher-risk decode tableization. This sequence keeps behavior safety high while reducing dense switch complexity and improving change locality for future rounds.

**Phases 4**

1. **Phase 1: Expand Characterization Matrices**
    - **Objective:** Add exhaustive characterization coverage for ALU/RMW decode routes and full-bit-index RMB/SMB/BBR/BBS behavior before refactoring.
    - **Files/Functions to Modify/Create:** `tests/cpu65c02_test.cpp` (`run_alu_rmw_decode_matrix_preserved_test`, `run_bit_opcode_family_matrix_preserved_test`, `main` test registration)
    - **Tests to Write:** `alu_rmw_decode_matrix_preserved`, `bit_opcode_family_matrix_preserved`
    - **Steps:**
        1. Add new characterization tests covering all opcode members and edge contracts (cycles, PC, flags, memory side effects).
        2. Run targeted CPU test target and confirm new tests initially fail if expectations are violated.
        3. Adjust only test expectations/fixtures to align with current behavior and re-run to pass.

2. **Phase 2: Refactor Flag/Transfer/Stack and Accumulator Paths**
    - **Objective:** Decompose dense flag/transfer/stack and accumulator handling into helper-driven paths without changing behavior.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp` (`execute_flag_transfer_stack_opcode`, `execute_accumulator_misc_opcode`, new helper methods), `include/prodos8emu/cpu65c02.hpp` (helper declarations), `tests/cpu65c02_test.cpp` (equivalence characterization)
    - **Tests to Write:** `flag_transfer_stack_helper_dispatch_equivalence`, `accumulator_misc_helper_dispatch_equivalence`
    - **Steps:**
        1. Add helper-dispatch equivalence tests that assert unchanged cycles/flags/stack behavior.
        2. Run targeted tests and capture failing expectations from missing helpers.
        3. Implement minimal helper extraction and routing changes to make tests pass, then re-run tests.

3. **Phase 3: Refactor BIT and Bit-Opcode Families**
    - **Objective:** Centralize BIT/TSB/TRB mode handling and unify RMB/SMB/BBR/BBS bit-index logic with shared primitives.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp` (`execute_bit_family_opcode`, `execute_bit_opcode`, new mode/bit helpers), `include/prodos8emu/cpu65c02.hpp` (helper declarations), `tests/cpu65c02_test.cpp` (equivalence and contract coverage)
    - **Tests to Write:** `bit_family_mode_metadata_equivalence`, `bit_opcode_shared_primitive_equivalence`
    - **Steps:**
        1. Add characterization tests for immediate-vs-memory BIT semantics and all bit indices 0-7 for RMB/SMB/BBR/BBS.
        2. Run targeted tests and confirm failures against unimplemented shared helpers.
        3. Implement metadata/shared-primitive refactor and re-run tests to green.

4. **Phase 4: Table-Drive ALU/RMW Decode Dispatch**
    - **Objective:** Replace remaining ALU/RMW decode branch trees with metadata-driven routing while preserving precedence and timing.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp` (`execute_alu_opcode`, `execute_rmw_opcode`, ALU/RMW metadata/classifier helpers), `include/prodos8emu/cpu65c02.hpp` (helper declarations), `tests/cpu65c02_test.cpp` (full-range route equivalence)
    - **Tests to Write:** `alu_decode_table_equivalence`, `rmw_decode_table_equivalence`, `alu_rmw_route_precedence_nonregression`
    - **Steps:**
        1. Add full-range decode-equivalence tests comparing legacy and metadata route classifications.
        2. Run targeted tests to expose mismatches prior to implementation.
        3. Implement minimal metadata-driven routing and re-run required targets to confirm parity.

**Open Questions 2**

1. Keep the same mandatory validation gate each phase: `ctest --test-dir build -R "prodos8emu_cpu65c02_tests|prodos8emu_emulator_startup_tests" --output-on-failure`?
2. Prefer `constexpr` anonymous-namespace tables over function-local static tables for new opcode metadata in this round?
