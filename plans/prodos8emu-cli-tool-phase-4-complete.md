## Phase 4 Complete: CLI Tool Scaffolding

Added a new `prodos8emu_run` command-line tool and wired it into CMake. The tool currently parses arguments and prints the chosen configuration; CPU execution is implemented in later phases.

**Files created/changed:**
- tools/prodos8emu_run.cpp
- CMakeLists.txt

**Functions created/changed:**
- CLI argument parsing and usage printing in `prodos8emu_run`

**Tests created/changed:**
- No new tests (CLI scaffolding only)

**Review Status:** APPROVED

**Git Commit Message:**
feat: Add prodos8emu_run CLI scaffold

- Add prodos8emu_run executable target wired into CMake
- Parse ROM path, system file path, and optional flags
- Implement --help, strict --max-instructions parsing, and error handling
- Print parsed configuration (execution added later)
