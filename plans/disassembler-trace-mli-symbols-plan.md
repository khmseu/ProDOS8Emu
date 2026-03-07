# Plan: Disassembler Trace With MLI Symbols

Implement an opt-in CPU disassembler trace with a dedicated CLI option and Python wrapper support. The disassembler will emit one line per executed instruction, resolve operands through the existing static monitor symbol table, render ProDOS MLI calls as a one-line pseudo-instruction, and append register comments with both pre/post state snapshots. The new disassembly stream will be written to its own trace file, independent of existing trace and MLI outputs.

## Phases 5

1. **Phase 1: CLI and Wrapper Flag Plumbing**
    - **Objective:** Add a new `--disassembly-trace` option through runner and Python wrapper parsing/wiring.
    - **Files/Functions to Modify/Create:** `tools/prodos8emu_run.cpp` (`CliOptions`, `print_usage`, `parse_args`, `main`), `tools/edasm_setup.py` (`parse_args`, `run_emulator`, `main`), `tests/prodos8emu_run_cli_test.cpp`, `tests/python_edasm_setup_test.py`.
    - **Tests to Write:** CLI help and acceptance tests for `--disassembly-trace`; Python parse and forwarding tests for `--disassembly-trace`.
    - **Steps:**
        1. Write failing CLI and Python tests for the new option.
        2. Implement minimal parser and forwarding changes to satisfy tests.
        3. Run targeted tests to confirm pass.

2. **Phase 2: Dedicated Disassembly Trace Sink**
    - **Objective:** Add a separate trace file sink for disassembly output, independent from current debug and MLI logs.
    - **Files/Functions to Modify/Create:** `include/prodos8emu/cpu65c02.hpp`, `src/cpu65c02.cpp`, `tools/prodos8emu_run.cpp`, corresponding tests in `tests/cpu65c02_test.cpp` and CLI tests if needed.
    - **Tests to Write:** CPU and/or runner tests validating disassembly trace writes to its own sink and does not require debug trace.
    - **Steps:**
        1. Add failing tests for independent disassembly sink behavior.
        2. Implement sink setter/plumbing and wire new CLI option to its own file.
        3. Run targeted tests to confirm pass.

3. **Phase 3: Per-Instruction Disassembly Core**
    - **Objective:** Always disassemble the current instruction when enabled.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp` (step hook and decode/format helpers), `include/prodos8emu/cpu65c02.hpp` (public toggle/setter surface).
    - **Tests to Write:** CPU tests asserting one disassembly line per executed instruction with stable ordering.
    - **Steps:**
        1. Write failing CPU tests for per-step disassembly presence and order.
        2. Implement minimal decode/format path without changing execution semantics.
        3. Run targeted CPU tests to confirm pass.

4. **Phase 4: Symbol Resolution and MLI One-Line Pseudo-Op**
    - **Objective:** Resolve address operands using static symbol table and render `JSR $BF00` as one-line `MLI` pseudo-instruction with call-byte/param-word and call-name.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp`, `tests/cpu65c02_test.cpp`.
    - **Tests to Write:** CPU tests for symbol hit/miss formatting and MLI line containing opcode `MLI`, `.byte` value, `.word` value, and resolved MLI call name.
    - **Steps:**
        1. Add failing tests for symbol and MLI formatting contracts.
        2. Implement formatter updates using existing static symbol and MLI name mappings.
        3. Run targeted CPU tests to confirm pass.

5. **Phase 5: Register Comments and Documentation**
    - **Objective:** Append register comment for every disassembled instruction with both pre/post register snapshots and update docs.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp`, `tests/cpu65c02_test.cpp`, `README.md`.
    - **Tests to Write:** CPU tests asserting pre/post register values appear in disassembly comments; documentation consistency checks via existing CLI tests.
    - **Steps:**
        1. Add failing CPU tests for pre/post register comment format.
        2. Implement comment emission in disassembly formatter.
        3. Run targeted tests, then broader regression tests.

## Open Questions 0

1. None. User decisions applied: independent sink, both pre/post register comments, one-line MLI format, static symbol table.
