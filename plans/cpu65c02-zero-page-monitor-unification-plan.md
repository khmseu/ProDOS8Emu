## Plan: CPU65C02 Zero-Page Monitor Unification

Unify current zero-page monitoring constructs into a single mechanism triggered by all instruction-driven zero-page writes, while preserving existing trace behavior contracts and output formatting expectations where applicable. The implementation will be tests-first and incremental to minimize regression risk.

**Phases 4**

1. **Phase 1: Add Characterization Guardrails for Unified Triggers**
    - **Objective:** Lock baseline expectations for zero-page monitor triggering across write families before refactor.
    - **Files/Functions to Modify/Create:** `tests/cpu65c02_test.cpp` (new tests + `main` registration)
    - **Tests to Write:** `zp_monitor_trigger_matrix_contracts`, `zp_monitor_all_writes_uniform_policy_contracts`, `zp_monitor_trace_output_compatibility_baseline`
    - **Steps:**
        1. Add tests covering zero-page writes from store/STZ/RMW/RMB/SMB/TSB/TRB instruction paths.
        2. Add contract tests asserting uniform monitor policy across all zero-page write opcodes.
        3. Run targeted CTest gate and ensure tests pass against current behavior baseline.

2. **Phase 2: Implement Single Zero-Page Write Monitor Hook**
    - **Objective:** Introduce one unified monitor mechanism at the CPU write funnel (`write8`) for all zero-page writes.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp`, `include/prodos8emu/cpu65c02.hpp`, `tests/cpu65c02_test.cpp`
    - **Tests to Write:** `zp_monitor_write8_hook_equivalence`, `zp_monitor_uniform_emission_nonregression`
    - **Steps:**
        1. Add tests-first assertions for write8-triggered monitor behavior.
        2. Implement monitored address spec + write8 hook + per-step context handoff.
        3. Re-run targeted CTest gate and align outputs to preserve contracts.

3. **Phase 3: Remove Legacy Snapshot/Whitelist Paths**
    - **Objective:** Delete duplicate legacy monitoring constructs and route all monitored events through the unified mechanism.
    - **Files/Functions to Modify/Create:** `src/cpu65c02.cpp`, `include/prodos8emu/cpu65c02.hpp`, `tests/cpu65c02_test.cpp`
    - **Tests to Write:** `zp_monitor_legacy_path_removal_nonregression`, `zp_monitor_trace_delta_format_stability`
    - **Steps:**
        1. Add parity tests proving no regressions from legacy-path removal.
        2. Remove old snapshot/whitelist logic and keep formatting compatibility.
        3. Re-run targeted CTest gate and fix any output drift.

4. **Phase 4: Finalize Round and Validate**
    - **Objective:** Consolidate docs and confirm full targeted validation on unified monitor architecture.
    - **Files/Functions to Modify/Create:** `plans/cpu65c02-zero-page-monitor-unification-phase-4-complete.md`, `plans/cpu65c02-zero-page-monitor-unification-complete.md`, optionally `tests/cpu65c02_test.cpp`
    - **Tests to Write:** No new mandatory tests unless review identifies coverage gaps.
    - **Steps:**
        1. Run required CTest gate for cpu65c02 + emulator startup targets.
        2. Address any review findings from prior phases.
        3. Publish completion documents and hand off final commit message.

**Open Questions 2**

1. Should MLI-side-effect writes to monitored zero-page addresses emit monitor lines? **Decision:** Don't care.
2. Should PassNbr (and other monitored ZP fields) use uniform all-write triggering? **Decision:** All.
