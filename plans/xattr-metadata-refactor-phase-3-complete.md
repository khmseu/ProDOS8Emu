## Phase 3 Complete: Host mtime for mod time

Stopped persisting mod_date/mod_time as xattrs; derived them from host mtime. When SET_FILE_INFO provides mod_date/mod_time, updated host mtime via utimensat().

**Files created/changed:**

- src/mli_housekeeping.cpp
- tests/housekeeping_test.cpp

**Functions created/changed:**

- loadMetadata (derives mod_date/mod_time from st.st_mtime via stat())
- setFileInfoCall (updates filesystem mtime via utimensat() when mod_date/mod_time provided)
- ProDOS time conversion helpers: encode_prodos_datetime, decode_prodos_datetime

**Tests created/changed:**

- GET_FILE_INFO reads mod_date/mod_time from filesystem mtime
- SET_FILE_INFO updates filesystem mtime

**Review Status:** APPROVED

**Git Commit Message:**
feat: derive ProDOS mod times from host mtime

- GET_FILE_INFO reads mod_date/mod_time from stat() mtime
- SET_FILE_INFO updates host mtime via utimensat()
- Add error handling for utimensat() failures
- Remove mod_date/mod_time xattr persistence
