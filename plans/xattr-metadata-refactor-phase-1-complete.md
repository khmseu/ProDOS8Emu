## Phase 1 Complete: Access string codec

Implemented a strict 8-character ProDOS access-byte formatter/parser and added focused tests for required examples, roundtrips, and invalid inputs.

**Files created/changed:**

- include/prodos8emu/access_byte.hpp
- tests/housekeeping_test.cpp
- plans/xattr-metadata-refactor-plan.md

**Functions created/changed:**

- prodos8emu::format_access_byte
- prodos8emu::parse_access_byte

**Tests created/changed:**

- prodos8emu_housekeeping_tests (adds codec coverage)

**Review Status:** APPROVED

**Git Commit Message:**
feat: add ProDOS access-byte string codec

- Add strict formatter/parser for 8-bit access flags
- Add housekeeping tests for examples, roundtrips, invalid inputs
