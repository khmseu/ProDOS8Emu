## Phase 1 Complete: Monitor Symbol Simplification

Removed marker payload/key indirection from CPU trace monitoring and standardized zero-page monitor output to include mutator mnemonics when detected. Updated CPU trace tests to match the new, simplified output contracts. Targeted CPU and startup test suites pass.

**Files created/changed:**

- src/cpu65c02.cpp
- tests/cpu65c02_test.cpp
- plans/monitor-symbol-simplification-phase-1-complete.md

**Functions created/changed:**

- `zp_monitor_mutator_name`
- `CPU65C02::log_step_trace_marker`
- `CPU65C02::log_step_zp_monitor_events`
- `find_monitor_symbol`

**Tests created/changed:**

- `run_trace_marker_table_equivalence_test`
- `run_trace_flag_delta_output_equivalence_test`
- `run_zp_monitor_trace_output_compatibility_baseline_test`
- `run_zp_monitor_legacy_path_removal_nonregression_test`
- `run_zp_monitor_trace_delta_format_stability_test`

**Review Status:** APPROVED

**Git Commit Message:**
refactor: simplify monitor symbol tracing

- remove marker payload and symbol-key mechanics from monitor table
- apply opcode mutator labels to monitored zero-page delta output
- update cpu trace baselines for simplified marker and delta format
