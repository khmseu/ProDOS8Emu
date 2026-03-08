## Plan: Analyzer Flow-Aware Resync

Improve analyzer resilience by adding control-flow-aware stepping and stronger automatic resynchronization so alignment remains stable through loops and subroutine calls before falling back to interactive prompts.

**Phases 3**

1. **Phase 1: Add Call-Stack Source Stepping**
    - **Objective:** Keep source pointer aligned across `JSR` and `RTS` control flow transitions.
    - **Files/Functions to Modify/Create:** `tools/disassembly_log_analyzer.py` (`run_alignment`, helper functions for `JSR`/`RTS` handling, `run_self_check`)
    - **Tests to Write:** `run_self_check` assertions for synthetic `JSR`/`RTS` paths and return-index behavior
    - **Steps:**
        1. Write failing self-check assertions for `JSR` call-target jump and `RTS` return handling.
        2. Implement minimal call-stack tracking in synced mode using source label targets and trace `POST PC`.
        3. Re-run self-check and ensure new assertions pass.

2. **Phase 2: Add Operand-Aware Auto-Resync Heuristics**
    - **Objective:** Recover from sync drift automatically using operand-aware candidate scoring before prompting.
    - **Files/Functions to Modify/Create:** `tools/disassembly_log_analyzer.py` (`acquire_sync_window` path, mismatch fallback, operand scoring helpers, `run_self_check`)
    - **Tests to Write:** `run_self_check` assertions that select best candidate among mnemonic-equal windows using operand/address hints
    - **Steps:**
        1. Write failing self-check assertions for ambiguous mnemonic windows resolved by operand-aware scoring.
        2. Implement scoring logic using operand shape, `$HHHH` addresses, and branch-target hints.
        3. Integrate scorer into initial sync and mismatch recovery, then re-run self-check.

3. **Phase 3: Validate and Harden Behavior**
    - **Objective:** Verify improved stability on real trace data and keep failure messages actionable.
    - **Files/Functions to Modify/Create:** `tools/disassembly_log_analyzer.py` (status/diagnostic messaging only if needed)
    - **Tests to Write:** command-level validation with `--self-check` and bounded non-interactive runs
    - **Steps:**
        1. Run `--self-check` to validate parser and heuristic invariants.
        2. Run non-interactive bounded alignment (`--max-lines`) to confirm fewer early mismatches.
        3. Adjust diagnostics minimally if unresolved stop reasons need clearer operator guidance.

**Open Questions 1**

1. Ambiguous label matches across files use same-file first, then nearest index globally. Proceeding with this policy.
