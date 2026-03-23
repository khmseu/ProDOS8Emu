## Phase 2 Complete: Emit Fixed TSV Report

A fixed-path runtime-label report is now written as TSV at the end of alignment runs, containing runtime-label recovery events with normalized label/address fields for auditability.

**Files created/changed:**

- tools/disassembly_log_analyzer.py

**Functions created/changed:**

- runtime_label_to_addr
- coerce_event_int
- dedupe_runtime_label_events
- write_runtime_label_report
- run_alignment
- parse_args

**Tests created/changed:**

- run_self_check TSV report formatting assertions
- run_self_check dedupe assertions for duplicate runtime events

**Review Status:** APPROVED with minor recommendations

**Git Commit Message:**
feat: add runtime-label TSV report output

- write deduped runtime-label-required events to fixed TSV path
- include normalized label token and parsed runtime address columns
- print report path and counts in alignment summary
