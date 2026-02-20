## Phase 4 Complete: Housekeeping Calls

Phase 4 adds ProDOS 8 MLI housekeeping calls (CREATE/DESTROY/RENAME/SET_FILE_INFO/GET_FILE_INFO/ON_LINE) implemented against the banked-memory parameter blocks, backed by the host filesystem and ProDOS metadata xattrs.

**Files created/changed:**

- CMakeLists.txt
- include/prodos8emu/mli.hpp
- src/mli_housekeeping.cpp
- src/xattr.cpp
- tests/housekeeping_test.cpp

**Functions created/changed:**

- prodos8emu::MLIContext::createCall
- prodos8emu::MLIContext::destroyCall
- prodos8emu::MLIContext::renameCall
- prodos8emu::MLIContext::setFileInfoCall
- prodos8emu::MLIContext::getFileInfoCall
- prodos8emu::MLIContext::onLineCall
- prodos8emu::prodos8_set_xattr (errno mapping improvement)

**Tests created/changed:**

- prodos8emu_housekeeping_tests

**Review Status:** APPROVED with minor recommendations

**Git Commit Message:**
feat: implement ProDOS housekeeping MLI calls

- Add CREATE/DESTROY/RENAME/SET_FILE_INFO/GET_FILE_INFO/ON_LINE calls
- Persist ProDOS metadata via user.prodos8.* xattrs with safe defaults
- Add housekeeping test suite and wire into CTest
