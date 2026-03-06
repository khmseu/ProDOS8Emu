## Phase 5 Complete: Tableize Trace Marker and Delta Emission

Phase 5 converted trace marker and flag-delta emission logic to table-driven mappings while preserving output compatibility and step-time emission conditions. Added characterization tests lock byte-for-byte marker/delta output and non-emission behavior for unmapped cases.

**Files created/changed:**

- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::log_step_trace_marker(uint16_t pc)`
- `CPU65C02::passnbr67_mutator_name(uint8_t opcode)`
- `CPU65C02::log_step_trace_flag_deltas(uint8_t opcode, const TraceFlagSnapshot& oldFlags, const TraceFlagSnapshot& newFlags)`
- `run_trace_marker_table_equivalence_test(int& failures)`
- `run_trace_flag_delta_output_equivalence_test(int& failures)`

**Tests created/changed:**

- `trace_marker_table_equivalence`
- `trace_flag_delta_output_equivalence`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: table-drive trace marker emission

- convert trace marker mapping to table-driven emission logic
- convert trace flag-delta reporting to table-driven field iteration
- preserve output compatibility and trace emit conditions
- add marker/delta output equivalence characterization tests
- validate cpu65c02 and emulator-startup ctest targets
