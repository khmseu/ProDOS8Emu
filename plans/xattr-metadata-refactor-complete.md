## Plan Complete: Split ProDOS metadata xattrs

Implemented per-field ProDOS metadata xattrs with a strict access-byte codec and ISO 8601 UTC creation time, and now derive mod time from host mtime (with SET_FILE_INFO updating mtime). This removes the packed metadata xattr and aligns mod-time behavior with the host filesystem.

**Phases Completed:** 3 of 3
1. OK Phase 1: Access string codec
2. OK Phase 2: Separate xattrs (no legacy fallback)
3. OK Phase 3: Host mtime for mod time

**All Files Created/Modified:**
- include/prodos8emu/access_byte.hpp
- src/mli_housekeeping.cpp
- src/mli_filing.cpp
- tests/housekeeping_test.cpp
- plans/xattr-metadata-refactor-plan.md
- plans/xattr-metadata-refactor-phase-1-complete.md
- plans/xattr-metadata-refactor-phase-2-complete.md
- plans/xattr-metadata-refactor-phase-3-complete.md
- plans/xattr-metadata-refactor-complete.md

**Key Functions/Classes Added:**
- format_access_byte
- parse_access_byte
- storeMetadata
- loadMetadata
- setFileInfoCall
- encodeProDOSDate
- encodeProDOSTime
- decodeProDOSDateTime

**Test Coverage:**
- Total tests written: 6
- All tests passing: Yes

**Recommendations for Next Steps:**
- Consider preserving atime when updating mtime (use UTIME_OMIT) if desired
