# Plan Complete: CPU65C02 General Monitor Symbol Table

Implemented a general monitor symbol table in `CPU65C02` with symbol metadata (address, name, monitor flags) and migrated monitoring lookups to this shared mechanism. Existing zero-page and PC marker monitoring now resolve through symbol-table helpers, and JSR/RTS monitoring resolves target addresses through symbol lookup with fallback formatting preserved. The rollout was delivered in tests-first phases and validated with targeted multi-suite regression gates.

**Phases Completed:** 4 of 4

1. ✅ Phase 1: Characterization Tests for Symbol-Table Contracts
2. ✅ Phase 2: Unified Symbol Table and Lookup Migration
3. ✅ Phase 3: Migration Cleanup and Read-Flag Wiring
4. ✅ Phase 4: Final Regression and Plan Closeout

**All Files Created/Modified:**

- src/cpu65c02.cpp
- tests/cpu65c02_test.cpp
- plans/cpu65c02-general-monitor-symbol-table-plan.md
- plans/cpu65c02-general-monitor-symbol-table-phase-1-complete.md
- plans/cpu65c02-general-monitor-symbol-table-phase-2-complete.md
- plans/cpu65c02-general-monitor-symbol-table-phase-3-complete.md
- plans/cpu65c02-general-monitor-symbol-table-phase-4-complete.md
- plans/cpu65c02-general-monitor-symbol-table-complete.md

**Key Functions/Classes Added:**

- MonitorSymbol / MonitorSymbolFlag / MonitorSymbolKey (internal monitor metadata model)
- find_monitor_symbol
- find_monitor_symbol_by_key
- append_marker_payload_field
- CPU65C02::log_step_trace_marker (symbol-table-driven marker resolution)
- CPU65C02::log_step_zp_monitor_events (symbol-table-driven label resolution)
- CPU65C02::log_jsr_rts_transition (symbol lookup annotation)

**Test Coverage:**

- Total tests written: 2 (new Phase 1 contracts)
- Existing monitor baselines preserved and executed: ✅
- Final targeted suites passing: ✅

**Recommendations for Next Steps:**

- If desired, extend symbol payload metadata from fixed enum payloads to generic symbol-key lists for more flexible marker payload composition.
- Keep the 4-suite targeted regression gate for future monitor-system refactors.
