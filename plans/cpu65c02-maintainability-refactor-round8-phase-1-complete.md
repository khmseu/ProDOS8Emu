## Phase 1 Complete: Expand Guardrail Characterization

Phase 1 added tests-only characterization guardrails for control-flow branch decode mapping, MLI error-path log ordering, and trace DskListF delta consistency. No emulator behavior code was changed, and the added tests lock baseline contracts needed for later Round 8 refactors.

**Files created/changed:**

- `plans/cpu65c02-maintainability-refactor-round8-plan.md`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `run_control_flow_branch_decode_table_equivalence_test(int& failures)`
- `run_mli_error_path_logging_order_stable_test(int& failures)`
- `run_trace_dsklistf_delta_logged_consistently_test(int& failures)`
- `main()` registration updates

**Tests created/changed:**

- `control_flow_branch_decode_table_equivalence`
- `mli_error_path_logging_order_stable`
- `trace_dsklistf_delta_logged_consistently`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
test: add round8 phase1 characterization guards

- add branch decode table equivalence characterization coverage
- add MLI error-path logging order stability test coverage
- add trace DskListF delta consistency characterization coverage
- register new phase1 tests in cpu65c02 test main dispatch
- validate cpu65c02 and emulator-startup ctest targets
