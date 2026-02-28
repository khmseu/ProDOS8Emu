# Phase 1 Complete: Project + Test Scaffolding

CMake build + CTest-based smoke tests are in place, with a minimal public API surface to support incremental MLI implementation.

**Files created/changed:**

- CMakeLists.txt
- .gitignore
- README.md
- include/prodos8emu/mli.hpp
- src/mli.cpp
- tests/smoke_test.cpp
- plans/prodos8-mli-emulation-plan.md

**Functions created/changed:**

- prodos8emu::MLIContext::MLIContext
- prodos8emu::MLIContext::~MLIContext
- prodos8emu::MLIContext::isInitialized
- prodos8emu::getVersion

**Tests created/changed:**

- prodos8emu_tests (tests/smoke_test.cpp)

**Review Status:** APPROVED

**Git Commit Message:**
feat: add CMake scaffold and smoke tests

- Add prodos8emu library target and public header
- Add CTest-registered smoke test executable
- Document build workflow and ignore dev caches
