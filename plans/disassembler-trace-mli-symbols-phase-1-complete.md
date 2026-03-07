## Phase 1 Complete: CLI and Wrapper Flag Plumbing

Added end-to-end option plumbing for `--disassembly-trace` in the C++ runner and Python wrapper, with tests covering help text, option acceptance, argparse parsing/defaulting, and subprocess forwarding. This phase intentionally limits scope to option surface and forwarding only, without implementing disassembly behavior.

**Files created/changed:**

- tools/prodos8emu_run.cpp
- tools/edasm_setup.py
- tests/prodos8emu_run_cli_test.cpp
- tests/python_edasm_setup_test.py

**Functions created/changed:**

- `print_usage` in `tools/prodos8emu_run.cpp`
- `parse_args` in `tools/prodos8emu_run.cpp`
- `main` in `tools/prodos8emu_run.cpp`
- `run_emulator` in `tools/edasm_setup.py`
- `parse_args` in `tools/edasm_setup.py`
- `main` in `tools/edasm_setup.py`

**Tests created/changed:**

- `runner_help_lists_disassembly_trace_flag` in `tests/prodos8emu_run_cli_test.cpp`
- `runner_accepts_disassembly_trace_flag` in `tests/prodos8emu_run_cli_test.cpp`
- `test_run_emulator_forwards_disassembly_trace_flag_when_enabled` in `tests/python_edasm_setup_test.py`
- `test_parse_args_accepts_disassembly_trace_flag` in `tests/python_edasm_setup_test.py`
- `test_parse_args_defaults_disassembly_trace_to_false` in `tests/python_edasm_setup_test.py`
- `test_main_forwards_disassembly_trace` in `tests/python_edasm_setup_test.py`

**Review Status:** APPROVED

**Git Commit Message:**
feat: add disassembly-trace option plumbing

- add `--disassembly-trace` parsing and help in runner CLI
- forward disassembly flag through `edasm_setup.py` argparse and launcher
- add CLI and Python tests for acceptance, defaults, and forwarding
