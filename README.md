# ProDOS8Emu

ProDOS8Emu is a C++20 emulator library for key ProDOS 8 behaviors, including MLI dispatch, banked Apple II memory semantics, a 65C02 core, path/prefix handling, and xattr-backed ProDOS metadata on Linux.

## Current Capabilities

- ProDOS MLI dispatch and core call handling in `src/mli_*.cpp` and `src/mli_dispatch.cpp`
- Apple II memory model with language card behavior in `src/apple2mem.cpp`
- 65C02 CPU core in `src/cpu65c02.cpp`
- Path normalization and prefix resolution in `src/path.cpp`
- ProDOS metadata mapping to Linux xattrs (`user.prodos8.*`) in `src/xattr.cpp`
- System file loading helpers in `src/system_loader.cpp`

## Requirements

- Linux (xattr-capable filesystem required for metadata features)
- CMake 3.20+
- C++20 compiler (GCC/Clang)
- Python 3 (for Python-based tooling and tests)

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

Current CTest targets:

- `prodos8emu_tests`
- `prodos8emu_memory_tests`
- `prodos8emu_path_prefix_tests`
- `prodos8emu_housekeeping_tests`
- `prodos8emu_filing_tests`
- `prodos8emu_system_tests`
- `prodos8emu_apple2mem_tests`
- `prodos8emu_cpu65c02_tests`
- `prodos8emu_emulator_startup_tests`
- `python_linux_to_prodos_text_test`

## CLI Tools

### `prodos8emu_run` (C++)

Build target: `build/prodos8emu_run`

```bash
./build/prodos8emu_run [OPTIONS] ROM_PATH SYSTEM_FILE_PATH
```

Purpose:

- Load an Apple II ROM image
- Load a ProDOS system file
- Initialize execution state and run the emulated CPU

Options:

- `--max-instructions N` stop after N instructions
- `--volume-root PATH` set root for volume mappings
- `-h`, `--help` show usage

### `linux_to_prodos_text.py` (Python)

Script path: `tools/linux_to_prodos_text.py`

```bash
python3 tools/linux_to_prodos_text.py [--lossy] [--access ACCESS] path
```

Purpose:

- Convert Linux text line endings to ProDOS style (`CR`)
- Optionally enforce strict ASCII or lossy replacement (`?`) for non-ASCII bytes
- Apply ProDOS xattr metadata:
  - `user.prodos8.file_type = 04`
  - `user.prodos8.aux_type = 0000`
  - `user.prodos8.storage_type = 01`
  - `user.prodos8.access = dn-..-wr` (default)

Examples:

```bash
# strict ASCII mode (default)
python3 tools/linux_to_prodos_text.py notes.txt

# allow non-ASCII by replacing with '?'
python3 tools/linux_to_prodos_text.py --lossy notes.txt

# set custom ProDOS access string
python3 tools/linux_to_prodos_text.py --access dnb..-wr notes.txt
```

## Repository Layout

```text
include/prodos8emu/   Public headers
src/                  C++ library implementation
tests/                C++ and Python tests
tools/                Utility tools and CLIs
plans/                Plan and phase-completion notes
third_party_docs/     Cached technical references
```

## Notes

- `third_party_docs/` is a local documentation cache used during development.
- See `AGENTS.md` and `.github/copilot-instructions.md` for repository workflow conventions.
