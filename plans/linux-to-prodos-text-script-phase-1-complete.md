# Phase 1 Complete: Core Byte Conversion

Implemented and tested the Phase 1 pure conversion functions for ProDOS text preparation: newline normalization to CR and an ASCII policy (strict vs lossy). The new Python unit test suite is integrated into CTest and avoids generating bytecode artifacts.

**Files created/changed:**

- .gitignore
- CMakeLists.txt
- tools/linux_to_prodos_text.py
- tests/python_linux_to_prodos_text_test.py
- plans/linux-to-prodos-text-script-plan.md

**Functions created/changed:**

- normalize_line_endings
- convert_to_ascii

**Tests created/changed:**

- python_linux_to_prodos_text_test (CTest)
- tests.python_linux_to_prodos_text_test (unittest module)

**Review Status:** APPROVED

**Git Commit Message:**
feat: Add Linux to ProDOS text converters

- Add pure newline normalization and ASCII conversion helpers
- Add unittest coverage and wire into CTest
- Ignore Python bytecode artifacts and run tests with -B
