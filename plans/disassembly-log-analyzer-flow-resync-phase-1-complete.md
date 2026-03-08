## Phase 1 Complete: Add Call-Stack Source Stepping

Implemented flow-aware stepping for `JSR`/`RTS` in synced alignment so the analyzer can follow subroutine paths and return points more reliably. The logic now avoids stale stack frames and preserves safe fallback behavior when return PCs cannot be matched.

**Files created/changed:**

- tools/disassembly_log_analyzer.py

**Functions created/changed:**

- call_label_from_source
- advance_source_position
- run_alignment
- run_self_check

**Tests created/changed:**

- run_self_check: added synthetic JSR step-in + RTS return-match assertions
- run_self_check: added JSR no-step/no-push regression assertion
- run_self_check: added RTS unmatched-post-pc no-pop regression assertion

**Review Status:** APPROVED

**Git Commit Message:**
feat: add JSR/RTS-aware analyzer stepping

- add call-stack-based source stepping for JSR and RTS transitions
- push return frames only when call stepping enters callee
- preserve safe fallback when RTS post-PC has no matching frame
