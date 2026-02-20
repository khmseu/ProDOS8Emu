## Plan: Split ProDOS metadata xattrs

Replace the packed user.prodos8.metadata with separate xattrs. Store created time as a single ISO 8601 UTC string. Use host mtime for mod date/time (and update host mtime on SET_FILE_INFO). Store access as an 8-character per-bit string.

**Phases**

1. **Phase 1: Access string codec**
    - **Objective:** Encode/decode the ProDOS access byte as an 8-character bitstring for bits 7..0 with one character per bit.
    - **Mapping:**
        - Bit 7 (Destroy enable): 'd' when set, '-' when clear
        - Bit 6 (Rename enable): 'n' when set, '-' when clear
        - Bit 5 (Backup needed): 'b' when set, '-' when clear
        - Bit 4 (Reserved): '.' always
        - Bit 3 (Reserved): '.' always
        - Bit 2 (Invisible): 'i' when set, '-' when clear
        - Bit 1 (Write enable): 'w' when set, '-' when clear
        - Bit 0 (Read enable): 'r' when set, '-' when clear
      Reserved bits are not stored; parsing should ignore '.' and ensure reserved bits remain clear.
    - **Files/Functions to Modify/Create:**
        - include/prodos8emu (new small header for formatting/parsing helpers)
        - src/mli_housekeeping.cpp (use helpers where access is interpreted)
        - tests/housekeeping_test.cpp (tests for formatting/parsing helpers)
    - **Tests to Write:**
        - Format examples: 0xC3 -> dn-..-wr; 0xE3 -> dnb..-wr
        - Parse roundtrip for supported letters; invalid strings rejected safely

2. **Phase 2: Separate xattrs (no legacy fallback)**
    - **Objective:** Replace the single packed metadata xattr with separate xattrs.
    - **Xattrs:**
        - user.prodos8.access (8-char string)
        - user.prodos8.file_type (hex byte)
        - user.prodos8.aux_type (hex word)
        - user.prodos8.storage_type (hex byte)
        - user.prodos8.created (ISO 8601 UTC string)
      No migration or fallback from user.prodos8.metadata is needed.
    - **Files/Functions to Modify/Create:**
        - src/mli_housekeeping.cpp (storeMetadata/loadMetadata)
        - src/mli_filing.cpp (openCall access check)
        - tests/housekeeping_test.cpp (update corrupted-metadata test to new attrs)
    - **Tests to Write:**
        - Corrupted xattr values fall back to sensible defaults

3. **Phase 3: Host mtime for mod time**
    - **Objective:** Stop persisting mod_date/mod_time as xattrs; derive them from host mtime. When SET_FILE_INFO provides mod_date/mod_time, update host mtime accordingly.
    - **Files/Functions to Modify/Create:**
        - src/mli_housekeeping.cpp (GET_FILE_INFO and SET_FILE_INFO paths)
    - **Tests to Write:**
        - SET_FILE_INFO updates mod date/time observable via subsequent GET_FILE_INFO
