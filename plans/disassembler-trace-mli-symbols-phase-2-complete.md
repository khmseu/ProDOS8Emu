# Phase 2 Complete: Dedicated Disassembly Trace Sink

Added a dedicated disassembly trace sink to CPU and wired the runner to open its own log file when `--disassembly-trace` is enabled, independent from `--debug`. This phase intentionally does not emit instruction disassembly yet; it establishes sink plumbing and verifies file-creation behavior.

**Files created/changed:**

- include/prodos8emu/cpu65c02.hpp
- src/cpu65c02.cpp
- tools/prodos8emu_run.cpp
- tests/prodos8emu_run_cli_test.cpp

**Functions created/changed:**

- `setDisassemblyTraceLog` in `include/prodos8emu/cpu65c02.hpp`
- `CPU65C02::setDisassemblyTraceLog` in `src/cpu65c02.cpp`
- `main` in `tools/prodos8emu_run.cpp`
- `run_command_capture_in_dir` in `tests/prodos8emu_run_cli_test.cpp`
- `create_test_temp_dir` in `tests/prodos8emu_run_cli_test.cpp`
- `runner_disassembly_trace_creates_dedicated_log_without_debug` in `tests/prodos8emu_run_cli_test.cpp`

**Tests created/changed:**

- `runner_disassembly_trace_creates_dedicated_log_without_debug` in `tests/prodos8emu_run_cli_test.cpp`

**Review Status:** APPROVED with minor recommendations

**Test outcomes:**

- `ctest --test-dir build -R prodos8emu_run_cli_tests --output-on-failure`: PASS
- `ctest --test-dir build -R prodos8emu_cpu65c02_tests --output-on-failure`: FAIL (4 existing failures on this branch in trace-marker/JSR-RTS/ZP-monitor expectation tests)

**Git Commit Message:**
feat: add dedicated disassembly trace sink

- add CPU disassembly trace stream setter and storage
- open prodos8emu_disassembly_trace.log independent of --debug
- add runner CLI test validating independent log file creation
