# Phase 4 Complete: Symbol Resolution and MLI One-Line Pseudo-Op

Extended disassembly output to include mnemonic/operand text, added static symbol-table resolution for decoded addresses, and implemented one-line ProDOS MLI pseudo-instruction formatting for `JSR $BF00`. Execution behavior remains unchanged; this phase only affects disassembly text emitted to the dedicated sink.

**Files created/changed:**

- src/cpu65c02.cpp
- tests/cpu65c02_disassembly_core_test.cpp

**Functions created/changed:**

- `find_any_monitor_symbol` in `src/cpu65c02.cpp`
- `append_symbol_suffix_for_address` in `src/cpu65c02.cpp`
- `append_abs_operand` in `src/cpu65c02.cpp`
- `append_zp_operand` in `src/cpu65c02.cpp`
- `disassembly_text_for_opcode` in `src/cpu65c02.cpp`
- `emit_disassembly_trace_line` in `src/cpu65c02.cpp`

**Tests created/changed:**

- `disassembly_emits_mnemonic_and_operand_text_with_stable_order` in `tests/cpu65c02_disassembly_core_test.cpp`
- `disassembly_resolves_known_symbol_for_absolute_operand` in `tests/cpu65c02_disassembly_core_test.cpp`
- `disassembly_falls_back_to_hex_for_unmapped_absolute_operand` in `tests/cpu65c02_disassembly_core_test.cpp`
- `disassembly_formats_mli_pseudo_instruction_for_jsr_bf00` in `tests/cpu65c02_disassembly_core_test.cpp`
- `disassembly_only_emits_while_sink_is_non_null` in `tests/cpu65c02_disassembly_core_test.cpp`

**Review Status:** APPROVED with minor recommendations

**Test outcomes:**

- `ctest --test-dir build -R prodos8emu_cpu65c02_disassembly_core_tests --output-on-failure`: PASS
- `ctest --test-dir build -R prodos8emu_run_cli_tests --output-on-failure`: PASS

**Git Commit Message:**
feat: add symbolized operands and MLI pseudo-op

- render mnemonic and operand text in per-instruction disassembly lines
- resolve decoded operand addresses through static monitor symbols
- format JSR $BF00 as one-line MLI with call byte, param word, and call name
