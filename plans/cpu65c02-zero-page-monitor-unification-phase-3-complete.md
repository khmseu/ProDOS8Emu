## Phase 3 Complete: Remove Legacy Snapshot/Whitelist Paths

Phase 3 removed legacy zero-page monitor snapshot/whitelist constructs and kept unified `write8`-triggered monitoring as the sole source of monitored zero-page delta events. Added nonregression tests lock legacy-path removal behavior and trace delta format stability.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `begin_step_zp_monitor_capture()`
- `log_step_zp_monitor_events(uint8_t opcode)`
- `passnbr_monitor_mutator_name(uint8_t opcode)`
- `CPU65C02::step()` (capture/flush integration updates)
- `run_zp_monitor_legacy_path_removal_nonregression_test(int& failures)`
- `run_zp_monitor_trace_delta_format_stability_test(int& failures)`

**Tests created/changed:**

- `zp_monitor_legacy_path_removal_nonregression`
- `zp_monitor_trace_delta_format_stability`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: remove legacy zp monitor paths

- remove legacy snapshot and whitelist monitor state paths
- keep write8-triggered monitor capture as sole event source
- retain trace marker and delta output format compatibility
- add nonregression tests for legacy-path removal and delta stability
- validate cpu65c02 and emulator-startup ctest targets
