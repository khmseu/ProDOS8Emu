## Phase 5 Complete: Separate step() Trace Instrumentation

Refactored `step()` to delegate trace marker and flag-delta logging to focused helpers while preserving instruction execution ordering and behavior. Added trace-focused characterization tests to lock marker emission, delta logging, and disabled-trace silence.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::log_step_trace_marker`
- `CPU65C02::read_step_trace_flags`
- `CPU65C02::passnbr67_mutator_name`
- `CPU65C02::log_step_trace_flag_deltas`
- `CPU65C02::step`
- `trace_markers_emit_for_known_entry_points`
- `passnbr_genf_listing_deltas_logged_consistently`
- `trace_disabled_emits_no_trace_lines`

**Tests created/changed:**

- `trace_markers_emit_for_known_entry_points`
- `passnbr_genf_listing_deltas_logged_consistently`
- `trace_disabled_emits_no_trace_lines`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: extract step trace instrumentation

- move EdAsm entry-point marker logging into dedicated helper
- extract trace flag snapshot and delta-emission helpers
- keep step execution order and opcode semantics unchanged
- add trace-focused characterization tests for markers and deltas
- verify disabled trace mode emits no trace lines
