## Plan: Runtime Label Usage Report

Add runtime-label usage tracking to the disassembly analyzer and emit a deterministic TSV report listing label/address resolutions that required runtime-label logic to recover source location.

**Phases 3**

1. **Phase 1: Capture Runtime-Required Events**
    - **Objective:** Capture each successful runtime-label recovery event in alignment logic.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py (run_alignment, advance_source_position, resync helpers)
    - **Tests to Write:** Extend self-check assertions to verify runtime-required event capture in JSR/RTS and resync paths.
    - **Steps:**
        1. Add runtime event collector structures.
        2. Instrument runtime-required branches.
        3. Validate with self-check assertions.

2. **Phase 2: Emit Fixed TSV Report**
    - **Objective:** Write a fixed-path TSV file for runtime-label-required events.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py (CLI/defaults, writer function, output call)
    - **Tests to Write:** Add self-check checks for TSV formatting and stable field content.
    - **Steps:**
        1. Add TSV writer with explicit header and deterministic ordering.
        2. Add fixed output path default.
        3. Write report at end of analyzer run.

3. **Phase 3: Dedupe and Compatibility Guards**
    - **Objective:** Ensure deduplicated events and no regression of existing outputs.
    - **Files/Functions to Modify/Create:** tools/disassembly_log_analyzer.py
    - **Tests to Write:** Add self-check assertions for dedupe behavior and empty-file behavior when no runtime-label events are used.
    - **Steps:**
        1. Implement dedupe key policy and preserve encounter-order semantics.
        2. Confirm existing outputs remain unchanged.
        3. Run analyzer self-check and relevant validations.

**Open Questions 0**

- Resolved by user approval: dedupe enabled, TSV format, fixed output path.
