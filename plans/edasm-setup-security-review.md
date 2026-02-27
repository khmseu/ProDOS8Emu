# Security Review: edasm_setup.py Subprocess Usage

**Date:** 2025-01-27  
**Reviewer:** GitHub Copilot  
**Scope:** Subprocess calls in `tools/edasm_setup.py` for command injection vulnerabilities

## Summary

✅ **No security vulnerabilities found.** All subprocess calls in `edasm_setup.py` are implemented securely against command injection attacks.

## Security Analysis

### Subprocess Call Inventory

Found 4 subprocess.run() calls in the codebase:

1. **Line 252** - `extract_disk_image()` with custom template
2. **Line 269** - `extract_disk_image()` default patterns
3. **Line 310** - `run_metadata_conversion()`
4. **Line 386** - `run_emulator()`

### Security Assessment

All calls are **SECURE** because:

1. ✅ **Argument lists used everywhere** - All subprocess.run() calls use list arguments, not strings
2. ✅ **No shell=True anywhere** - Shell interpreter is never invoked (default shell=False)
3. ✅ **Path validation added** - `validate_safe_path()` rejects shell metacharacters
4. ✅ **Token-based templating** - Custom extraction commands use `shlex.split()` + `format()`

### Why This Is Secure

When `subprocess.run()` receives a list (not a string) and `shell=False`:

- Arguments are passed directly to the executable via `execve()`
- Shell metacharacters (`;`, `|`, `&`, `$`, etc.) are treated as **literal characters**
- No shell is spawned to interpret commands

**Example:** Even if a user provides a malicious path like `"; rm -rf /"`, it's passed as a literal filename argument to cadius, not executed as a shell command.

### Defense-in-Depth Measures

Additional security layers were implemented:

1. **`validate_safe_path()` function** (lines 64-81)
   - Rejects paths containing shell metacharacters
   - Provides clear error messages
   - Prevents future mistakes if code is modified

2. **List of rejected characters:**
   - `;` - Command separator
   - `|` - Pipe operator
   - `&` - Background operator
   - `$` - Variable expansion
   - `` ` `` - Command substitution
   - `\n`, `\r` - Line separators
   - `>`, `<` - I/O redirection
   - `(`, `)`, `{`, `}` - Subshells

3. **Validation points:**
   - `--disk-image` path (line 240)
   - `work directory` path (line 241)
   - `--cadius` executable path (line 200)

## Test Coverage

Added comprehensive security tests in `tests/python_edasm_setup_test.py`:

- **TestPathSecurity** class with 9 test cases
- Tests verify rejection of all shell metacharacters
- Tests verify acceptance of normal safe paths
- All 39 Python tests pass ✅
- All 11 CTest suites pass ✅

### Example Security Tests

```python
def test_semicolon_rejected(self):
    """Paths with semicolons should be rejected (shell command separator)."""
    with self.assertRaises(ValueError) as cm:
        validate_safe_path("/path/to/file;rm -rf /", "disk-image")
    self.assertIn("shell metacharacter", str(cm.exception))

def test_pipe_rejected(self):
    """Paths with pipes should be rejected (shell command chaining)."""
    with self.assertRaises(ValueError):
        validate_safe_path("file.2mg|cat /etc/passwd", "disk-image")
```

## Recommendations

✅ **Current implementation is production-ready**

The code follows security best practices:

1. Argument lists instead of shell strings
2. No shell invocation
3. Input validation with clear error messages
4. Comprehensive test coverage

## References

- Python subprocess security: <https://docs.python.org/3/library/subprocess.html#security-considerations>
- CWE-78: OS Command Injection: <https://cwe.mitre.org/data/definitions/78.html>
- OWASP Command Injection: <https://owasp.org/www-community/attacks/Command_Injection>

## Approval

**Status:** ✅ APPROVED - No security issues found  
**Reviewer:** GitHub Copilot  
**Next Action:** Ready for commit
