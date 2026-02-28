# Plan: File Rearrangement Config for Disk Image Extraction

**TL;DR:** Add optional JSON config file support to `edasm_setup.py` that allows rearranging extracted files before metadata conversion. Config will specify source→destination mappings with glob pattern support for bulk operations. Atomic file operations prevent partial state on errors.

## Phases: 5

## Phase 1: Config File Format and Parsing

**Objective:** Design and implement JSON config file parsing with validation

**Files/Functions to Modify/Create:**

- [tools/edasm_setup.py](../tools/edasm_setup.py) - Add `parse_rearrange_config()`, `validate_rearrange_config()`

**Tests to Write:**

- `test_parse_rearrange_config_valid_json`
- `test_parse_rearrange_config_invalid_json`
- `test_parse_rearrange_config_missing_file`
- `test_validate_rearrange_config_valid_structure`
- `test_validate_rearrange_config_invalid_structure`
- `test_validate_rearrange_config_empty_mappings`

**Steps:**

1. Write tests for config parsing (empty config, valid mappings, invalid JSON, invalid structure)
2. Implement `parse_rearrange_config(config_path)` that reads JSON file and returns dict
3. Implement `validate_rearrange_config(config)` that validates structure (must be dict with "rearrange" key containing list of {"from": ..., "to": ...} objects)
4. Run tests to verify parsing and validation work correctly

**Config File Format:**

```json
{
  "rearrange": [
    { "from": "DIR1/FILE.TXT", "to": "DIR2/NEWFILE.TXT" },
    { "from": "OLD/*.ASM", "to": "SRC/" },
    { "from": "/VOLUME/ABSOLUTE/PATH.TXT", "to": "REL/PATH.TXT" }
  ]
}
```

**Design Decisions:**

- ✅ Glob pattern support (`*.TXT`, `DIR/*.ASM`) in "from" field
- ❌ NO file deletion support (only move/rename)
- ✅ Both relative and absolute ProDOS paths supported ("/VOLUME/..." for absolute)
- ✅ Files only (no directory operations)

## Phase 2: File Rearrangement Logic with Glob Support

**Objective:** Implement atomic file rearrangement with glob expansion and proper error handling

**Files/Functions to Modify/Create:**

- [tools/edasm_setup.py](../tools/edasm_setup.py) - Add `expand_rearrange_mappings()`, `rearrange_files()`

**Tests to Write:**

- `test_expand_glob_patterns_single_file`
- `test_expand_glob_patterns_multiple_matches`
- `test_expand_glob_patterns_no_matches`
- `test_expand_glob_patterns_subdirectories`
- `test_rearrange_files_simple_rename`
- `test_rearrange_files_move_to_subdir`
- `test_rearrange_files_glob_to_directory`
- `test_rearrange_files_create_parent_dirs`
- `test_rearrange_files_conflict_detection`
- `test_rearrange_files_missing_source`
- `test_rearrange_files_preserves_content`
- `test_rearrange_files_absolute_and_relative_paths`

**Steps:**

1. Write tests for glob expansion (single match, multiple matches, no matches, patterns with subdirs)
2. Implement `expand_rearrange_mappings(volume_dir, mappings)` that expands globs:
   - Use `pathlib.Path.glob()` for pattern matching
   - Handle absolute paths (starting with `/`) by stripping leading slash
   - For glob→directory mappings (to ends with `/`), preserve basenames
   - Return flat list of (source, dest) tuples
3. Write tests for file rearrangement operations
4. Implement `rearrange_files(volume_dir, expanded_mappings)` with atomic operations:
   - Validate all sources exist before making changes
   - Detect destination conflicts (file already exists)
   - Create parent directories as needed
   - Move files one by one with error rollback on failure
5. Add comprehensive error messages for each failure case
6. Run tests to verify all scenarios work correctly

**Glob Expansion Rules:**

- Pattern: `DIR/*.TXT` → matches all `.TXT` files in `DIR/`
- Destination ending with `/` → directory (preserve basename): `*.ASM` → `SRC/` becomes `file.ASM` → `SRC/file.ASM`
- Destination without `/` → explicit filename (only valid if glob matches exactly 1 file)

## Phase 3: Integration into Workflow

**Objective:** Integrate rearrangement step between extraction and metadata conversion

**Files/Functions to Modify/Create:**

- [tools/edasm_setup.py](../tools/edasm_setup.py) - Modify `main()`, `parse_args()`

**Tests to Write:**

- `test_integration_with_rearrange_config`
- `test_integration_without_rearrange_config`
- `test_integration_rearrange_before_metadata_conversion`
- `test_integration_cli_arg_parsing`

**Steps:**

1. Write integration tests that verify rearrangement happens at correct point in workflow
2. Add `--rearrange-config` argument to `parse_args()` with help text explaining feature
3. Modify `main()` to:
   - Parse config file if `--rearrange-config` provided
   - Call `expand_rearrange_mappings()` to expand globs
   - Call `rearrange_files()` after extraction, before metadata conversion
4. Ensure file operations are idempotent (can be run multiple times safely with `--skip-extract`)
5. Run full integration tests to verify workflow correctness

**Integration Point in main():**

```python
# Extract disk image if requested
if not args.skip_extract:
    print(f"Extracting {args.disk_image} to {volume_dir}...")
    extract_disk_image(...)

    # NEW: Rearrange files if config provided
    if args.rearrange_config:
        print("Rearranging files...")
        config = parse_rearrange_config(args.rearrange_config)
        validate_rearrange_config(config)
        mappings = expand_rearrange_mappings(str(volume_dir), config["rearrange"])
        rearrange_files(str(volume_dir), mappings)

    print("Converting metadata to xattrs...")
    run_metadata_conversion(str(volume_dir))
```

## Phase 4: End-to-End Testing

**Objective:** Comprehensive testing of entire feature with realistic scenarios

**Files/Functions to Modify/Create:**

- [tests/python_edasm_setup_test.py](../tests/python_edasm_setup_test.py) - Add `TestFileRearrangement` class

**Tests to Write:**

- `test_e2e_rearrange_multiple_files`
- `test_e2e_rearrange_with_glob_patterns`
- `test_e2e_rearrange_preserves_xattrs_after_conversion`
- `test_e2e_rearrange_with_text_imports`
- `test_e2e_rearrange_system_file_discovery`
- `test_e2e_rearrange_absolute_paths`

**Steps:**

1. Write end-to-end tests that exercise full workflow with rearrangement
2. Create test fixtures (sample config files with realistic mappings)
3. Verify rearranged files get correct metadata from cadius conversion
4. Verify system file discovery works after rearrangement
5. Verify text imports work when combined with rearrangement
6. Run all tests (unit + integration + e2e) to ensure complete correctness

## Phase 5: Documentation

**Objective:** Document new feature in README and provide example configs

**Files/Functions to Modify/Create:**

- [README.md](../README.md) - Add rearrangement config documentation
- Create example config file: `examples/rearrange_example.json`

**Tests to Write:**

- None (documentation phase)

**Steps:**

1. Add section to README explaining rearrangement config feature
2. Document JSON schema with examples (including glob patterns)
3. Provide use cases:
   - Organizing extracted files into logical directory structure
   - Renaming files for ProDOS naming convention compatibility
   - Bulk moving files matching patterns (e.g., all `.ASM` files to `SRC/`)
4. Create example config files showing common patterns:
   - Simple rename
   - Move to subdirectory
   - Glob pattern matching
   - Absolute path handling
5. Update usage examples in README to show `--rearrange-config` option
6. Add notes about execution order (extraction → rearrangement → metadata conversion)

**Example Documentation:**

````markdown
## File Rearrangement

The `--rearrange-config` option allows you to reorganize extracted disk image files before metadata conversion and emulator launch.

### Config Format

JSON file with "rearrange" array of mappings:

```json
{
  "rearrange": [
    { "from": "OLDNAME.TXT", "to": "NEWNAME.TXT" },
    { "from": "SOURCE/*.ASM", "to": "SRC/" },
    { "from": "/VOLUME/FILE.BIN", "to": "BIN/PROG.BIN" }
  ]
}
```
````

### Glob Patterns

The "from" field supports glob patterns:

- `*.TXT` - All .TXT files in volume root
- `DIR/*.ASM` - All .ASM files in DIR/
- `**/*.BIN` - All .BIN files recursively

### Path Types

- Relative: `DIR/FILE.TXT` (relative to volume root)
- Absolute: `/VOLUMENAME/DIR/FILE.TXT` (leading `/` makes it absolute)

### Destination Rules

- Ending with `/`: Directory (preserve basename)
  - `*.ASM` → `SRC/` becomes `file.ASM` → `SRC/file.ASM`
- Without `/`: Explicit filename (glob must match exactly 1 file)
  - `OLD.TXT` → `NEW.TXT` (rename)

---

## Design Decisions Summary

1. **Glob patterns:** ✅ Supported in "from" field using Python's `pathlib.Path.glob()`
2. **File deletion:** ❌ Not supported (only move/rename operations)
3. **Path handling:** ✅ Both relative (default) and absolute (starts with `/`) ProDOS paths
4. **Directory operations:** ❌ Files only (no directory moves, but can move files INTO directories)

## Execution Order

1. Extract disk image (cadius)
2. [NEW] Rearrange files (if --rearrange-config provided)
3. Convert cadius metadata to xattrs
4. Import text files
5. Discover system file
6. Run emulator

Rearrangement happens **after extraction but before metadata conversion** to ensure:

- Files are extracted with cadius naming conventions (e.g., `FILE#040000`)
- Rearrangement can rename them to clean names
- Metadata conversion operates on final file locations
- System file discovery finds the correctly-named/located system file
