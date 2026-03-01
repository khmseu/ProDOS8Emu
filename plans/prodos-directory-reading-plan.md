## Plan: ProDOS Directory Entry Reading

Implement full ProDOS directory entry reading so that READ calls on open directories return properly formatted 512-byte ProDOS directory blocks instead of immediately returning EOF.

**Phases: 4**

1. **Phase 1: Directory Block Data Structures**
    - **Objective:** Create C++ structures and helper functions for ProDOS directory blocks and entries
    - **Files/Functions to Modify/Create:**
        - [src/mli_filing.cpp](../src/mli_filing.cpp): Add directory block synthesis helpers
        - [include/prodos8emu/mli.hpp](../include/prodos8emu/mli.hpp): Add `OpenFile::directoryBlocks` field to cache synthesized blocks
    - **Tests to Write:**
        - Unit tests for directory block formatting helpers
        - Tests for entry creation (file entry, header entry)
        - Tests for date/time encoding
    - **Steps:**
        1. Write failing tests for block formatting functions
        2. Define `DirectoryBlock` struct (512 bytes with prev/next pointers + entries)
        3. Define `DirectoryEntry` struct (39 bytes per ProDOS spec)
        4. Implement helper: `encodeProDOSDateTime(time_t)` returning 4-byte date/time
        5. Implement helper: `createFileEntry(...)` building a 39-byte entry
        6. Implement helper: `createHeaderEntry(...)` building volume/subdir header
        7. Implement helper: `buildDirectoryBlock(...)` creating a 512-byte block
        8. Run tests to confirm all pass

2. **Phase 2: Directory Synthesis on OPEN**
    - **Objective:** When opening a directory, enumerate Unix filesystem and build ProDOS directory blocks
    - **Files/Functions to Modify/Create:**
        - [src/mli_filing.cpp](../src/mli_filing.cpp): Modify `openCall` to synthesize directory blocks
        - [include/prodos8emu/mli.hpp](../include/prodos8emu/mli.hpp): Add `std::vector<std::array<uint8_t, 512>> directoryBlocks` to `OpenFile`
    - **Tests to Write:**
        - Test opening directory synthesizes blocks correctly
        - Test directory with multiple files
        - Test empty directory
        - Test directory with subdirectories
    - **Steps:**
        1. Write failing tests for directory block synthesis
        2. In `openCall`, after detecting directory, enumerate with `std::filesystem::directory_iterator`
        3. For each entry, read xattrs (file_type, aux_type, creation time, access)
        4. Build file entries using `createFileEntry` helper
        5. Calculate `file_count` (active entries)
        6. Build header entry (volume or subdir header based on parent)
        7. Organize entries into 512-byte blocks (13 entries per block, key block has header + 12 files)
        8. Store blocks in `of.directoryBlocks`
        9. Set `of.mark = 0` and synthesize EOF as `blocks.size() * 512`
        10. Run tests to confirm synthesis works

3. **Phase 3: READ Implementation for Directories**
    - **Objective:** Modify `readCall` to return synthesized directory blocks based on MARK
    - **Files/Functions to Modify/Create:**
        - [src/mli_filing.cpp](../src/mli_filing.cpp): Replace EOF stub in `readCall` with block-reading logic
    - **Tests to Write:**
        - Test reading 512 bytes from directory
        - Test reading with MARK at various positions
        - Test reading past EOF returns appropriate error
        - Test SET_MARK/GET_MARK work with directories
        - Test reading multiple blocks from multi-block directory
    - **Steps:**
        1. Write failing tests for directory reading
        2. In `readCall`, check if `of.isDirectory`
        3. Calculate EOF as `of.directoryBlocks.size() * 512`
        4. Calculate starting block: `blockNum = of.mark / 512`, `offset = of.mark % 512`
        5. Copy bytes from `of.directoryBlocks[blockNum]` starting at `offset`
        6. Handle reads spanning multiple blocks
        7. Update `of.mark` and `trans_count`
        8. Return `ERR_EOF_ENCOUNTERED` only if no bytes read
        9. Run tests to confirm all pass

4. **Phase 4: Integration Testing and Edge Cases**
    - **Objective:** Test complete directory reading workflow with real scenarios
    - **Files/Functions to Modify/Create:**
        - [tests/filing_test.cpp](../tests/filing_test.cpp): Add comprehensive integration tests
    - **Tests to Write:**
        - Test OPEN directory, READ multiple 512-byte blocks, parse entries
        - Test directory with >12 files (multi-block)
        - Test mixed content (files, subdirectories, various types)
        - Test reading partial blocks
        - Test CLOSE and re-OPEN directory
    - **Steps:**
        1. Write integration tests covering realistic use cases
        2. Create test directory with various file types and subdirs
        3. OPEN directory and READ all blocks sequentially
        4. Parse and validate header entry fields
        5. Parse and validate file entry fields
        6. Verify `file_count` matches actual entries
        7. Test edge cases (empty dir, single file, many files)
        8. Run all tests to confirm everything works

**Open Questions:**

1. Should we support volume directory headers or only subdirectory headers? (Recommend: subdirectory headers only for now, as volumes are mapped to Unix paths)
2. How should we handle Unix-specific files that don't have ProDOS xattrs? (Recommend: assign default file_type=$06 BIN, aux_type=$0000, current timestamp)
3. Should directory blocks be cached or regenerated on each OPEN? (Recommend: cache in OpenFile for consistency during a session)
4. How to determine parent_pointer for subdirectory headers? (Recommend: set to 0 as it's not critical for reading-only use case)
