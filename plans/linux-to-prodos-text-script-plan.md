## Plan: Linux to ProDOS Text Converter Script

Create a small Python script that takes a Linux text file and converts it into a ProDOS TEXT file in this repo’s xattr-based metadata format. Conversion normalizes line endings to CR and applies `user.prodos8.*` xattrs consistent with existing tooling.

**Phases**

1. **Phase 1: Core byte conversion**
   - **Objective:** Convert Linux line endings to ProDOS CR and apply an ASCII policy.
   - **Files/Functions to Modify/Create:**
     - Create `tools/linux_to_prodos_text.py`
     - Functions: `normalize_line_endings(...)`, `convert_to_ascii(...)`
   - **Tests to Write:**
     - Normalize: LF→CR, CRLF→CR, preserve CR
     - ASCII: strict rejects non-ASCII, lossy replaces with `?`
   - **Steps:** Write failing unit tests, implement minimal conversion functions, re-run tests.

2. **Phase 2: Write file + xattr metadata**
   - **Objective:** Write converted bytes and set ProDOS metadata xattrs for TEXT ($04).
   - **Files/Functions to Modify/Create:**
     - Update `tools/linux_to_prodos_text.py`
     - Functions: `set_prodos_text_metadata(...)`, `convert_file(...)`
   - **Tests to Write:**
     - Xattrs are set: `file_type=04`, `aux_type=0000`, `storage_type=01`, `access=...`
     - Error handling when xattrs unsupported or permission denied
   - **Steps:** Add unit/integration tests using temp files, implement minimal xattr setter.

3. **Phase 3: CLI interface**
   - **Objective:** Provide a CLI to convert files (and optionally directories) with safe defaults.
   - **Files/Functions to Modify/Create:**
     - Update `tools/linux_to_prodos_text.py`
     - Functions: `parse_args(...)`, `main(...)`
   - **Tests to Write:**
     - `--help` works
     - Basic CLI invocation converts a file
   - **Steps:** Add argparse, wire to conversion functions, add CLI-level tests.

4. **Phase 4: Edge cases and polish**
   - **Objective:** Handle CRLF input, empty files, and already-tagged files cleanly.
   - **Files/Functions to Modify/Create:**
     - Update `tools/linux_to_prodos_text.py`
   - **Tests to Write:**
     - Empty file remains empty but gets metadata
     - Existing xattrs behavior matches chosen policy
   - **Steps:** Expand tests, implement edge-case behavior, re-run tests.

**Defaults (chosen for implementation unless changed)**

- ASCII policy: strict by default (error on non-ASCII); optional `--lossy` replaces with `?`.
- Output: in-place rewrite of the file contents.
- Xattrs: overwrite the ProDOS xattr keys this script manages.
- Metadata values: `file_type=04`, `aux_type=0000` (sequential TEXT), `storage_type=01`, `access=dn-..-wr`.

**Open Questions**

1. Do you want an option to set the high bit on printable characters (Apple II “high-bit text” style), or keep 7-bit ASCII only?
2. Should the default access byte be more permissive (e.g., `dnb..-wr`) or keep `dn-..-wr`?
