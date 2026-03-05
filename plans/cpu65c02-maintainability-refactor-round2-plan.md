## Plan: CPU65C02 Maintainability Refactor Round 2

Continue maintainability improvements in `src/cpu65c02.cpp` by first adding characterization coverage for currently under-tested ALU/RMW behavior, then performing small structural extractions guarded by those tests. This preserves behavior while reducing duplication in the highest-risk sections.

**Phases 3**

1. **Phase 1: ALU/RMW Characterization Coverage**
    - **Objective:** Lock current ALU and read-modify-write behavior before deeper refactors.
    - **Files/Functions to Modify/Create:**
      - `tests/cpu65c02_test.cpp`
    - **Tests to Write:**
      - `adc_sbc_binary_decimal_flag_contracts`
      - `logic_ora_and_eor_nz_contracts`
      - `cmp_does_not_mutate_registers_and_sets_czn`
      - `rmw_inc_dec_shift_rotate_writeback_and_flags`
    - **Steps:**
        1. Add failing characterization tests for ALU and RMW opcode behavior.
        2. Run `prodos8emu_cpu65c02_tests` and observe failures.
        3. Adjust tests minimally if fixture assumptions are incorrect.
        4. Re-run until tests pass and behavior is locked.

2. **Phase 2: Deduplicate ALU and RMW Opcode Bodies**
    - **Objective:** Reduce duplication in `execute()` by extracting shared helpers for ALU and RMW families.
    - **Files/Functions to Modify/Create:**
      - `src/cpu65c02.cpp`
      - `include/prodos8emu/cpu65c02.hpp`
      - `tests/cpu65c02_test.cpp`
    - **Tests to Write:**
      - `alu_family_dispatch_cycles_match_baseline`
      - `rmw_family_dispatch_preserves_carry_nz_and_memory`
    - **Steps:**
        1. Add tests that lock behavior and cycle expectations for representative opcodes.
        2. Extract shared helpers incrementally by opcode family.
        3. Keep opcode decode table and ordering unchanged.
        4. Run targeted tests after each extraction slice.

3. **Phase 3: Simplify Remaining Dispatch and MLI Pathname Branches**
    - **Objective:** Improve readability of remaining high-complexity paths (`execute` dispatch edges and MLI pathname parsing).
    - **Files/Functions to Modify/Create:**
      - `src/cpu65c02.cpp`
      - `include/prodos8emu/cpu65c02.hpp`
      - `tests/cpu65c02_test.cpp`
    - **Tests to Write:**
      - `execute_special_decode_order_preserved`
      - `mli_on_line_volume_list_logging_stable`
      - `mli_read_directory_entry_logging_stable`
    - **Steps:**
        1. Add tests for decode-order and log formatting contracts.
        2. Extract per-call pathname/log helpers for heavy branches.
        3. Preserve string field ordering and existing output semantics.
        4. Re-run CPU tests and startup integration tests.

**Open Questions 2**

1. Should cycle assertions in Phase 2 be strict per-opcode counts or relative invariants only?
2. Should Phase 3 permit small log wording cleanup if all log-format characterization tests are updated and passing?
