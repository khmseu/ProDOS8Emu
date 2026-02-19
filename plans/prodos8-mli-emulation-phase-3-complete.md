# Phase 3 Complete: Pathname + Prefix + Xattrs

Phase 3 implements ProDOS pathname parsing/mapping foundations, adds `SET_PREFIX`/`GET_PREFIX` MLI calls using emulated banked memory parameter blocks, and introduces a mandatory Linux xattr backend under `user.prodos8.*`.

**Files created/changed:**

- CMakeLists.txt
- include/prodos8emu/mli.hpp
- src/mli.cpp
- include/prodos8emu/path.hpp
- src/path.cpp
- include/prodos8emu/xattr.hpp
- src/xattr.cpp
- tests/path_prefix_test.cpp

**Functions created/changed:**

- prodos8emu::MLIContext::setPrefixCall
- prodos8emu::MLIContext::getPrefixCall
- prodos8emu::path::* (pathname normalize/validate/resolve helpers)
- prodos8emu::xattr::* (user.prodos8.* helpers)

**Tests created/changed:**

- prodos8emu_path_prefix_tests

**Review Status:** APPROVED

**Git Commit Message:**
feat: add pathname, prefix, and xattr helpers

- Implement SET_PREFIX/GET_PREFIX MLI calls with param blocks
- Add ProDOS pathname normalization and host path mapping
- Add mandatory user.prodos8.* xattr helper backend
