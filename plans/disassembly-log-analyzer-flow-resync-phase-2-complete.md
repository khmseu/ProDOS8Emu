## Phase 2 Complete: Add Operand-Aware Auto-Resync Heuristics

Implemented operand-aware sync scoring and local auto-resync so the analyzer can recover from drift automatically with confidence gating before falling back to interactive help. Weak unique matches are now rejected to prevent silent misalignment.

**Files created/changed:**

- tools/disassembly_log_analyzer.py

**Functions created/changed:**

- normalize_operand_text
- operand_shape_hint
- operand_addresses_16
- score_trace_source_pair
- score_sync_candidate
- is_confident_sync_metric
- acquire_sync_window
- attempt_local_auto_resync
- run_alignment
- run_self_check

**Tests created/changed:**

- run_self_check: ambiguous mnemonic-window sync resolved by operand-aware scoring
- run_self_check: local auto-resync success path assertion
- run_self_check: low-confidence unique initial-sync candidate rejection
- run_self_check: low-confidence unique local-resync candidate rejection

**Review Status:** APPROVED

**Git Commit Message:**
feat: add operand-aware analyzer auto-resync

- score sync candidates using operand shapes and address overlap hints
- add confidence gating to reject weak unique auto-sync candidates
- attempt local auto-resync on mismatch before interactive fallback
