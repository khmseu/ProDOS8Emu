## Phase 3 Complete: Validate and Harden Behavior

Completed final hardening and validation by making control-flow stepping robust when trace lines do not include `POST PC`. The analyzer now infers flow from the next trace entry in synced mode while still preferring explicit `POST PC` when available.

**Files created/changed:**

- tools/disassembly_log_analyzer.py

**Functions created/changed:**

- advance_source_position
- run_alignment
- run_self_check

**Tests created/changed:**

- run_self_check: branch taken/not-taken fallback using observed next trace PC
- run_self_check: JSR fallback decision using observed next trace PC
- run_self_check: RTS return-frame matching fallback using observed next trace PC

**Review Status:** APPROVED

**Git Commit Message:**
fix: infer control flow without post-PC metadata

- use next trace PC as fallback when post-PC is unavailable
- apply fallback to branch, JSR step-in, and RTS return matching
- add self-check coverage for missing post-PC control-flow cases
