## Phase 2 Complete: Separate metadata xattrs

Replaced the packed metadata xattr with separate per-field xattrs (access string, hex types, ISO 8601 created) and updated callers/tests accordingly.

**Files created/changed:**

- src/mli_housekeeping.cpp
- src/mli_filing.cpp
- tests/housekeeping_test.cpp

**Functions created/changed:**

- storeMetadata (now writes separate xattrs)
- loadMetadata (now reads separate xattrs + created ISO parsing)
- MLIContext::openCall (reads access xattr)

**Tests created/changed:**

- prodos8emu_housekeeping_tests (updated corrupted-xattr test, added direct xattr verification, added created readback)

**Review Status:** APPROVED

**Git Commit Message:**
refactor: split ProDOS metadata xattrs

- Store access as 8-char flags and types as hex xattrs
- Add ISO 8601 UTC created xattr with strict parsing
- Update OPEN and housekeeping tests to use new xattrs
