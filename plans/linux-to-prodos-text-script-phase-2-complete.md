# Phase 2 Complete: In-place Convert + ProDOS TEXT Xattrs

Extended the Linux-to-ProDOS text conversion script to rewrite a file in-place (atomic temp-file replace) and apply ProDOS TEXT metadata using the repo’s `user.prodos8.*` xattr schema. Added unittest coverage for metadata setting, conversion behavior, and failure-atomicity.

**Files created/changed:**

- tools/linux_to_prodos_text.py
- tests/python_linux_to_prodos_text_test.py

**Functions created/changed:**

- set_prodos_text_metadata
- convert_file_in_place

**Tests created/changed:**

- Metadata xattr set/get tests (skip cleanly when xattrs unsupported)
- In-place conversion tests (LF/CRLF → CR)
- Atomicity regression test for xattr failure

**Review Status:** APPROVED

**Git Commit Message:**
feat: Convert files to ProDOS TEXT xattrs

- Convert text files in-place with atomic temp-file replace
- Set ProDOS TEXT metadata via user.prodos8.* xattrs
- Extend unittests for conversion, xattrs, and atomicity
