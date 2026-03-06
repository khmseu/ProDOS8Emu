# Phase 2 Complete: Implement edasm_setup Flag Support

Implemented `--jsr-rts-trace` support in `tools/edasm_setup.py` as a pure passthrough to `prodos8emu_run`. The new flag is parsed, forwarded at runtime, and defaults to disabled when omitted.

**Files created/changed:**

- tools/edasm_setup.py
- plans/edasm-setup-jsr-rts-trace-flag-phase-2-complete.md

**Functions created/changed:**

- run_emulator
- parse_args
- main

**Tests created/changed:**

- Reused Phase 1 tests in `tests/python_edasm_setup_test.py`
- Validation run: `ctest --test-dir build -R "python_edasm_setup_test" --output-on-failure`
- Result: 1/1 passed

**Review Status:** APPROVED

**Git Commit Message:**

feat: add jsr/rts passthrough in edasm_setup

- add --jsr-rts-trace argparse option in edasm_setup.py
- forward flag to prodos8emu_run in run_emulator command
- wire main to pass jsr_rts_trace setting through
