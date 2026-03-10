## Plan: Pending Trace Prompt Markers

Update the mismatch-help prompt so the trace section shows three prior entries for more context and uses the same marker language as source context. Keep the change narrowly scoped to prompt rendering and add a self-check that locks in the new output format.

**Phases 3**
1. **Phase 1: Unify Prompt Legend**
    - **Objective:** Move to one shared legend placed before the pending trace section.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py, prompt_for_help
    - **Tests to Write:** Prompt formatting assertions covering legend placement
    - **Steps:**
        1. Update prompt rendering to print a single legend before the pending trace entries heading.
        2. Reuse the existing marker semantics so the prompt stays visually consistent.
        3. Keep the rest of the help text unchanged.
2. **Phase 2: Expand Previous Trace Context**
    - **Objective:** Show three previous trace entries while clearly marking the current pending entry.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py, prompt_for_help, run_alignment if history depth must grow
    - **Tests to Write:** Prompt formatting assertions covering three previous entries and current-entry marking
    - **Steps:**
        1. Increase retained/displayed prior trace context to three entries.
        2. Mark prior trace entries with >> and the first pending entry with =>.
        3. Preserve any overlap semantics only where they are already meaningful.
3. **Phase 3: Lock In With Self-Check**
    - **Objective:** Prevent regressions in the prompt format.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py, run_self_check
    - **Tests to Write:** Assertions that confirm legend order, marker usage, and removal of the legacy (prev) label
    - **Steps:**
        1. Capture prompt output under a controlled input stub.
        2. Assert the new legend and marker layout.
        3. Run the analyzer self-check and focused validation.

**Open Questions 1**
1. None; legend placement and previous-entry count were clarified by the user.
