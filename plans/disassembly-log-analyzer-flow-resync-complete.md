## Plan Complete: Analyzer Flow-Aware Resync

Implemented a resilient disassembly-log alignment workflow that now handles loops, subroutine call/return flow, and ambiguous sync windows with confidence-gated operand-aware scoring. The analyzer can auto-resync in more cases while avoiding silent drift from weak matches, and it has fallback control-flow inference when `POST PC` metadata is missing. This reduces early false mismatches and preserves interactive help only for genuinely unresolved points.

**Phases Completed:** 3 of 3

1. ✅ Phase 1: Add Call-Stack Source Stepping
2. ✅ Phase 2: Add Operand-Aware Auto-Resync Heuristics
3. ✅ Phase 3: Validate and Harden Behavior

**All Files Created/Modified:**

- tools/disassembly_log_analyzer.py
- plans/disassembly-log-analyzer-flow-resync-plan.md
- plans/disassembly-log-analyzer-flow-resync-phase-1-complete.md
- plans/disassembly-log-analyzer-flow-resync-phase-2-complete.md
- plans/disassembly-log-analyzer-flow-resync-phase-3-complete.md

**Key Functions/Classes Added:**

- `advance_source_position`
- `attempt_local_auto_resync`
- `acquire_sync_window`
- `score_trace_source_pair`
- `score_sync_candidate`
- `is_confident_sync_metric`
- `operand_shape_hint`
- `operand_addresses_16`
- `call_label_from_source`

**Test Coverage:**

- Total tests written: 12 self-check scenarios added/expanded in `run_self_check`
- All tests passing: ✅

**Recommendations for Next Steps:**

- Add one self-check asserting `post_pc` precedence when `observed_next_pc` conflicts, to lock fallback precedence behavior.
- If desired, add an optional CLI flag to tune confidence thresholds for easier adaptation to different trace/source corpora.
