# Phase 3 Complete: Migration Cleanup and Read-Flag Wiring

Completed migration cleanup by replacing remaining hardcoded marker payload monitor assumptions with symbol-table-driven resolution. The `read` monitor flag is now used in live payload emission logic, and existing trace-format contracts remained stable under targeted regression tests.

**Files created/changed:**

- src/cpu65c02.cpp
- plans/cpu65c02-general-monitor-symbol-table-phase-3-complete.md

**Functions created/changed:**

- MonitorSymbolKey metadata additions in monitor symbol model
- find_monitor_symbol_by_key
- append_marker_payload_field
- CPU65C02::log_step_trace_marker
- CPU65C02::log_step_zp_monitor_events

**Tests created/changed:**

- No new tests in this phase (cleanup/refactor only)
- Validation run: `ctest --test-dir build -R "prodos8emu_cpu65c02_tests|prodos8emu_emulator_startup_tests" --output-on-failure`
- Result: 2/2 passed

**Review Status:** APPROVED

**Git Commit Message:**

refactor: complete symbol-table monitor cleanup

- use symbol keys to resolve marker payload fields via read flags
- remove remaining hardcoded marker payload monitor addresses
- keep trace output contracts stable across cpu/startup tests
