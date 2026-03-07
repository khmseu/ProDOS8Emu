# Plan Complete: Disassembler Trace With MLI Symbols

Implemented a full disassembler tracing workflow for `prodos8emu_run`, including CLI/wrapper option plumbing, a dedicated disassembly trace sink, per-instruction disassembly output, static symbol resolution, ProDOS MLI pseudo-op rendering, and PRE/POST register comments on every disassembly line. The solution keeps CPU execution semantics unchanged while making trace output significantly more informative and structured for debugging and reverse engineering.

**Phases Completed:** 5 of 5

1. ✅ Phase 1: CLI and Wrapper Flag Plumbing
2. ✅ Phase 2: Dedicated Disassembly Trace Sink
3. ✅ Phase 3: Per-Instruction Disassembly Core
4. ✅ Phase 4: Symbol Resolution and MLI One-Line Pseudo-Op
5. ✅ Phase 5: Register Comments and Documentation

**All Files Created/Modified:**

- include/prodos8emu/cpu65c02.hpp
- src/cpu65c02.cpp
- tools/prodos8emu_run.cpp
- tools/edasm_setup.py
- tests/prodos8emu_run_cli_test.cpp
- tests/python_edasm_setup_test.py
- tests/cpu65c02_disassembly_core_test.cpp
- CMakeLists.txt
- README.md
- plans/disassembler-trace-mli-symbols-plan.md
- plans/disassembler-trace-mli-symbols-phase-1-complete.md
- plans/disassembler-trace-mli-symbols-phase-2-complete.md
- plans/disassembler-trace-mli-symbols-phase-3-complete.md
- plans/disassembler-trace-mli-symbols-phase-4-complete.md
- plans/disassembler-trace-mli-symbols-phase-5-complete.md

**Key Functions/Classes Added:**

- `CPU65C02::setDisassemblyTraceLog`
- `disassembly_text_for_opcode`
- `emit_disassembly_trace_line`
- `append_disassembly_register_snapshot`
- `find_any_monitor_symbol`
- `append_symbol_suffix_for_address`

**Test Coverage:**

- Total tests written/extended: 13
- Targeted disassembly/CLI/Python tests passing: ✅

**Recommendations for Next Steps:**

- Add one CLI-level assertion that disassembly trace lines include PRE/POST register comment suffixes.
- Optionally normalize minor formatting consistency for indirect `JMP` operand `$` prefix in disassembly text.
