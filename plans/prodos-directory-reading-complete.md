## ProDOS Directory Entry Reading Complete

Implemented full ProDOS directory entry reading, allowing READ calls on open directories to return properly formatted 512-byte ProDOS directory blocks following the ProDOS 8 Technical Reference specification.

**Files created/changed:**

- [include/prodos8emu/mli.hpp](../include/prodos8emu/mli.hpp)
- [src/mli_filing.cpp](../src/mli_filing.cpp)
- [tests/filing_test.cpp](../tests/filing_test.cpp)

**Key changes implemented:**

### Phase 1: Directory Block Data Structures

- Added `directoryBlocks` field to `OpenFile` struct for caching synthesized blocks
- Created ProDOS directory constants (ENTRY_LENGTH=39, ENTRIES_PER_BLOCK=13, storage types)
- Implemented date/time encoding functions (`encodeProDOSDate`, `encodeProDOSTime`)
- Implemented little-endian write helpers (`writeLittleEndian16`, `writeLittleEndian24`)
- Created `FileMetadata` struct for directory entry data
- Implemented `createFileEntry()` - builds 39-byte ProDOS file entries
- Implemented `createDirectoryHeaderEntry()` - builds volume/subdirectory headers (both 0x0F and 0x0E)
- Implemented `buildDirectoryBlocks()` - organizes entries into linked 512-byte blocks

### Phase 2: Directory Synthesis on OPEN

- Modified `openCall()` to synthesize directory blocks when opening directories
- Implemented `synthesizeDirectoryBlocks()` function that:
  - Enumerates Unix filesystem directory using `std::filesystem::directory_iterator`
  - Reads ProDOS metadata from xattrs (file_type, aux_type, access, creation/mod times)
  - Applies defaults for files without xattrs (BIN type $06, default access $C3)
  - Determines storage type based on file size (seedling/sapling/tree/subdirectory)
  - Calculates blocks_used
  - Sorts entries alphabetically (ProDOS convention)
  - Creates both volume headers (0x0F) and subdirectory headers (0x0E)
- Cached synthesized blocks in `OpenFile::directoryBlocks` for consistent reads

### Phase 3: READ Implementation for Directories

- Modified `readCall()` to handle directory reads from synthesized blocks
- Calculates EOF as `directoryBlocks.size() * 512`
- Reads bytes from appropriate block based on MARK position
- Handles reads spanning multiple blocks
- Updates MARK and trans_count appropriately
- Returns `ERR_EOF_ENCOUNTERED` only when no bytes can be read

### Phase 4: Integration Testing

- Updated directory test to verify ProDOS directory block format
- Test creates directory with file, opens it, reads 512-byte block
- Validates prev/next block pointers (should be 0 for single-block directory)
- Validates header entry storage type (0x0E for subdirectory or 0x0F for volume)
- All 11 test suites pass (15 total test cases in filing tests)

**ProDOS compliance:**

- Follows ProDOS 8 Technical Reference directory format specification
- 512-byte blocks with 4-byte header (prev/next pointers) + entries
- 39-byte entries (ENTRY_LENGTH=$27)
- 13 entries per block (ENTRIES_PER_BLOCK=$0D)
- Key block: header entry + 12 file entries
- Subsequent blocks: 13 file entries each
- Proper date/time encoding (16-bit date + 16-bit time words)
- Support for both volume directory headers (storage_type 0x0F) and subdirectory headers (0x0E)
- Storage types: seedling (0x01), sapling (0x02), tree (0x03), subdirectory (0x0D)
- File entries include: name, storage_type, file_type, key_pointer, blocks_used, EOF, creation/mod times, access, aux_type

**Test results:**
All 11 test suites pass, including comprehensive directory reading test.

**Technical decisions:**

- Directories cached at OPEN time for consistency during session
- Both volume and subdirectory headers supported
- Files without xattrs get sensible defaults (BIN type, current timestamp, default access)
- Parent pointers set to 0 (acceptable for read-only use case)
- Directories always opened read-only
- Directory enumeration errors result in empty directory blocks

**Future enhancements:**

- Support for WRITE to directories (creating/deleting entries)
- Dynamic directory updates (refresh on subsequent opens)
- Volume bitmap support
- Key block pointer tracking for nested directories
