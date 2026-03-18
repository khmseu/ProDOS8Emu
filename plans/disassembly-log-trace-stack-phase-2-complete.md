## Phase 2 Complete: Make RTS Resolution Trace-First

Implemented trace-first RTS return resolution by deriving effective return PC from pre-RTS stack dump bytes. Missing or insufficient RTS stack dumps are now mismatch-significant via explicit diagnostics, per user direction.

**Files created/changed:**

- tools/disassembly_log_analyzer.py

**Functions created/changed:**

- detect_rts_alignment_issue
- advance_source_position
- run_self_check

**Tests created/changed:**

- run_self_check assertions for:
- rts-insufficient-trace-stack diagnostics when stack dump evidence is missing
- rts-return-mismatch diagnostics when trace stack-derived return differs from post PC
- trace-first RTS source advancement when observed post PC conflicts

**Review Status:** APPROVED

**Git Commit Message:**
feat: use trace stack dumps for RTS resolution

- Derive RTS effective return PC from pre-RTS trace stack bytes
- Emit explicit issue when RTS trace stack evidence is insufficient
- Add self-check coverage for trace-first and mismatch paths
