# Phase 2 Complete: Unified Symbol Table and Lookup Migration

Implemented a general monitor symbol table in `cpu65c02.cpp` with address, name, and monitor flags (`read`, `write`, `pc`). Existing ZP write monitoring, marker lookup, and JSR/RTS monitor naming now resolve through this shared symbol table, with fallback formatting preserved when no symbol is found.

**Files created/changed:**

- src/cpu65c02.cpp
- plans/cpu65c02-general-monitor-symbol-table-phase-2-complete.md

**Functions created/changed:**

- MonitorSymbol / MonitorSymbolFlag model and lookup helpers
- find_monitor_symbol
- CPU65C02::write8
- CPU65C02::log_step_trace_marker
- CPU65C02::log_step_zp_monitor_events
- CPU65C02::log_jsr_rts_transition

**Tests created/changed:**

- Reused Phase 1 characterization tests and existing monitoring baselines
- Validation run: `ctest --test-dir build -R "prodos8emu_cpu65c02_tests|prodos8emu_emulator_startup_tests" --output-on-failure`
- Result: 2/2 passed

**Review Status:** APPROVED

**Git Commit Message:**

feat: add unified monitor symbol table

- introduce shared monitor symbols with read/write/pc flags
- route zp and pc monitor lookups through symbol table helpers
- resolve jsr/rts monitor names via symbol lookup with fallback
