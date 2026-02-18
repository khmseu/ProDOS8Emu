## Plan: ProDOS 8 MLI Emulation Library

Build a modern C++ library on Linux that emulates ProDOS 8 MLI calls with 1:1-style parameter blocks stored in emulated 6502 memory (16 banks × 4KB). Each MLI call is exposed as a function (method) taking `(memoryBanks, uint16_t paramBlockOffset)` and returning a ProDOS error code byte (0 on success). Linux filesystem operations are used where possible; ProDOS-only metadata is stored in Linux extended attributes (xattrs) under a `user.prodos8.*` namespace.

Decisions captured from user:

- ProDOS date/time encoding/decoding follows the TechRef documentation bit layouts.
- ProDOS access bits are reflected into Linux permissions (in addition to being enforced by MLI semantics).
- xattrs are mandatory: if xattrs are unavailable/unsupported, calls that require them fail with an appropriate ProDOS error code.

**Phases**

1. **Phase 1: Project + Test Scaffolding**
    - **Objective:** Create a buildable/testable C++ library skeleton suitable for incremental MLI implementation.
    - **Files/Functions to Modify/Create:** CMakeLists.txt; include/; src/; tests/; basic public API header.
    - **Tests to Write:** Build/link smoke test; minimal unit test runner sanity.
    - **Steps:**
        1. Create CMake targets for the library and for tests (C++20 or later).
        2. Add minimal public headers and a compiled translation unit.
        3. Add a test framework via CMake and a smoke test.

2. **Phase 2: Emulated Memory + Error Codes + Parameter Helpers**
    - **Objective:** Provide safe helpers to read/write bytes/words from banked memory and canonical ProDOS error codes.
    - **Files/Functions to Modify/Create:** include/prodos8emu/memory.hpp; include/prodos8emu/errors.hpp; src/memory.cpp (optional).
    - **Tests to Write:** Banked address translation; little-endian u16; bounded reads/writes; Pascal count-string decode; bad pointer handling.
    - **Steps:**
        1. Implement bank+offset translation for a 16×4096 memory model.
        2. Implement helpers for reading/writing u8/u16/u24 (little-endian) and copying to/from host buffers.
        3. Define error constants per TechRef MLI error code summary.

3. **Phase 3: Pathname Mapping + Prefix + Xattr Backend**
    - **Objective:** Implement ProDOS pathname rules (count byte strings, absolute vs prefix) and a mandatory xattr backend.
    - **Files/Functions to Modify/Create:** include/prodos8emu/path.hpp; include/prodos8emu/xattr.hpp; src/path.cpp; src/xattr.cpp; include/prodos8emu/mli.hpp.
    - **Tests to Write:** Absolute vs partial pathname mapping; prefix concatenation; SET_PREFIX/GET_PREFIX buffer formatting; xattr read/write roundtrip on temp files.
    - **Steps:**
        1. Implement pathname parsing from emulated memory (count byte + ASCII) and prefix application.
        2. Implement volume mapping: ProDOS path `/VOL/FILE` maps to `${volumesRoot}/VOL/FILE`.
        3. Implement xattr helpers (`get/set/remove`) in `user.prodos8.*`; treat missing xattr support as an error.

4. **Phase 4: Housekeeping Calls**
    - **Objective:** Implement housekeeping calls mapped to Linux filesystem operations and xattrs.
    - **Files/Functions to Modify/Create:** src/mli_housekeeping.cpp; include/prodos8emu/mli.hpp.
    - **Tests to Write:** CREATE file/dir; DESTROY; RENAME restrictions; SET_FILE_INFO/GET_FILE_INFO persistence; ON_LINE records from volumesRoot.
    - **Steps:**
        1. CREATE ($C0): create regular file (storage_type $01) or directory (storage_type $0D); initialize metadata in xattrs.
        2. DESTROY ($C1) and RENAME ($C2).
        3. GET_FILE_INFO ($C4) and SET_FILE_INFO ($C3): map supported fields to stat/chmod/utimens; store full ProDOS fields in xattrs.
        4. ON_LINE ($C5): enumerate mounted volumes by listing volumesRoot; populate returned records per TechRef.

5. **Phase 5: Filing Calls (Open File Table + I/O)**
    - **Objective:** Implement filing calls with an emulator context that holds open-file state (ref_num table), newline mode, MARK/EOF.
    - **Files/Functions to Modify/Create:** src/mli_filing.cpp; include/prodos8emu/mli.hpp.
    - **Tests to Write:** OPEN/READ/WRITE/CLOSE/FLUSH end-to-end; newline-terminated reads; trans_count behavior; EOF error ($4C) when trans_count=0; 24-bit MARK/EOF semantics.
    - **Steps:**
        1. Implement OPEN ($C8) returning ref_num and tracking io_buffer pointer.
        2. Implement NEWLINE ($C9) state per ref_num.
        3. Implement READ ($CA) and WRITE ($CB) with request_count/trans_count and mark updates.
        4. Implement CLOSE ($CC) and FLUSH ($CD); SET_MARK/GET_MARK; SET_EOF/GET_EOF.

6. **Phase 6: Buffer Calls + System Calls + Stubs**
    - **Objective:** Implement SET_BUF/GET_BUF and GET_TIME; validate/stub interrupts and block I/O calls.
    - **Files/Functions to Modify/Create:** src/mli_system.cpp; include/prodos8emu/mli.hpp.
    - **Tests to Write:** SET_BUF/GET_BUF pointer tracking; GET_TIME writes $BF90–$BF93; block calls return chosen not-supported error consistently.
    - **Steps:**
        1. Implement SET_BUF ($D2) and GET_BUF ($D3) with basic pointer validation.
        2. Implement GET_TIME ($82) to update emulated system date/time locations.
        3. Implement ALLOC_INTERRUPT ($40) and DEALLOC_INTERRUPT ($41) as stubs with parameter validation.
        4. Implement READ_BLOCK ($80) and WRITE_BLOCK ($81) as stubs returning not-supported errors until a disk backend exists.
