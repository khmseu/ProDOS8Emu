## Phase 2 Complete: Extract Pure Logging/Path Helpers

Extracted and reused pure helpers for counted-path log formatting in CPU MLI logging paths. This removes duplicated formatting branches while preserving behavior and log ordering.

**Files created/changed:**

- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `format_counted_path_for_log`
- `read_and_format_counted_path_for_log`
- `extract_mli_pathnames`
- `mli_call_name`

**Tests created/changed:**

- `extract_pathname_len_zero_formats_empty`
- `extract_pathname_invalid_length_formats_error`
- `mli_get_prefix_logging_remains_stable`
- Existing regression target: `prodos8emu_cpu65c02_tests`
- Integration target: `prodos8emu_emulator_startup_tests`

**Review Status:** APPROVED

**Git Commit Message:**
refactor: extract counted-path logging helpers

- add pure helper to format counted paths for log output
- centralize counted-path read plus formatting into one utility
- replace duplicated OPEN/SET_PREFIX/GET_PREFIX path formatting branches
- add characterization tests for empty and invalid counted-path lengths
- preserve MLI log ordering and existing runtime behavior
