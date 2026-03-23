## Plan Complete: Runtime Label Usage Report

The analyzer now records runtime-label-required source-recovery events and writes them to a fixed TSV report file with deterministic deduplication. This makes runtime-assisted source alignment decisions inspectable while keeping existing alignment and annotation behavior intact. Self-check coverage now includes positive and negative capture cases, TSV formatting, dedupe, empty-report behavior, and default report-path assertions.

**Phases Completed:** 3 of 3

1. ✅ Phase 1: Capture Runtime-Required Events
2. ✅ Phase 2: Emit Fixed TSV Report
3. ✅ Phase 3: Dedupe and Compatibility Guards

**All Files Created/Modified:**

- tools/disassembly_log_analyzer.py
- plans/runtime-label-usage-report-plan.md
- plans/runtime-label-usage-report-phase-1-complete.md
- plans/runtime-label-usage-report-phase-2-complete.md
- plans/runtime-label-usage-report-phase-3-complete.md
- plans/runtime-label-usage-report-complete.md

**Key Functions/Classes Added:**

- record_runtime_label_event
- runtime_label_to_addr
- coerce_event_int
- dedupe_runtime_label_events
- write_runtime_label_report

**Test Coverage:**

- Total tests written: self-check assertions added/updated in run_self_check
- All tests passing: ✅

**Recommendations for Next Steps:**

- Optionally add a standalone unittest module for report writer behavior outside run_self_check.
- Optionally add report import tooling for quick diffing across multiple trace runs.
