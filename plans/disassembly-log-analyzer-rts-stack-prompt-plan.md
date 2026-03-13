## Plan: RTS Stack Snapshot Prompt

Add prompt-time visibility into the analyzer's emulated JSR/RTS return stack when a recent RTS appears in the displayed previous trace entries. This helps operators understand call-depth state at the mismatch point without changing alignment behavior.

**Phases 3**

1. **Phase 1: Capture Pre-Execution Stack Snapshots**
    - **Objective:** Retain a pre-execution stack snapshot for each recent trace entry shown in help prompts.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py, run_alignment
    - **Tests to Write:** Self-check assertions for snapshot retention behavior
    - **Steps:**
        1. Add bounded recent snapshot history aligned with the recent trace deque.
        2. Capture a stack snapshot before advancing matched instructions.
        3. Append explicit unavailable snapshots on skip/resync paths to preserve alignment.
2. **Phase 2: Prompt Rendering for Last RTS**
    - **Objective:** Show stack contents from before the last shown RTS when previous entries include RTS.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py, prompt_for_help and call sites
    - **Tests to Write:** Prompt-output assertions for RTS-present and unavailable snapshot cases
    - **Steps:**
        1. Pass recent stack snapshots into prompt rendering.
        2. Locate the most recent displayed RTS entry and retrieve its pre-exec snapshot.
        3. Render top-first stack lines or clear fallback text if empty/unavailable.
3. **Phase 3: Validation and Hardening**
    - **Objective:** Prevent regressions and verify behavior.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py, run_self_check
    - **Tests to Write:** Assertions for section visibility, ordering, and frame formatting
    - **Steps:**
        1. Add deterministic prompt capture tests.
        2. Verify stack section formatting and fallback strings.
        3. Run analyzer self-check and fix any failures.

**Open Questions 1**

1. Emulated stack is interpreted as analyzer return frames `(source_index, return_pc)` tracked by source_return_stack.
