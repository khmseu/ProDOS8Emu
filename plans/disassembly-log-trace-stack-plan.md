## Plan: Replace Emulated Stack With Trace Stack Dumps

Replace byte-level stack emulation in the disassembly log analyzer with authoritative trace stack dumps for RTS return resolution. This reduces analyzer drift and aligns mismatch diagnostics with real CPU trace evidence while preserving source-frame stack annotations.

**Phases 3**

1. **Phase 1: Remove Byte Stack State**
    - **Objective:** Eliminate byte-level emulated stack storage and mutation logic.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py state fields and instruction handlers touching source_byte_stack.
    - **Tests to Write:** Self-check assertions proving no behavior depends on source_byte_stack emulation.
    - **Steps:**
        1. Add/adjust self-check expectations so byte-stack-derived behavior is no longer required.
        2. Remove source_byte_stack state and push/pop emulation paths.
        3. Run analyzer self-check and ensure green.

2. **Phase 2: Make RTS Resolution Trace-First**
    - **Objective:** Derive RTS effective return PC from parsed pre-RTS stack dump bytes.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py RTS handling and helper logic for return address derivation.
    - **Tests to Write:** RTS resolution tests covering trace stack present, missing, and too-short cases; missing/too-short should emit mismatch diagnostics.
    - **Steps:**
        1. Add failing tests for RTS effective PC precedence and mismatch signaling.
        2. Replace emulated-byte-based RTS return derivation with trace-stack-based derivation and robust fallback.
        3. Re-run analyzer self-check and dry-run checks.

3. **Phase 3: Validate End-to-End With Real Log**
    - **Objective:** Confirm analyzer output behavior remains consistent on real trace input after migration.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py (cleanup only if needed).
    - **Tests to Write:** No new test suite targets; run analyzer self-check and non-interactive log processing.
    - **Steps:**
        1. Run analyzer self-check and dry/non-interactive processing against prodos8emu_disassembly_trace.log.
        2. Verify RTS diagnostics are trace-evidence-driven and mismatches are explicit when stack dump data is insufficient.
        3. Remove dead code/comments tied to byte emulation if any remain.

**Open Questions 0**

- None. User direction captured: insufficient/missing RTS stack dump should be treated as mismatch-significant.
