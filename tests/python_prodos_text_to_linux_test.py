#!/usr/bin/env python3
"""Unit tests for prodos_text_to_linux.py: line ending, and file operations."""

import errno
import os
import sys
import tempfile
import unittest
from pathlib import Path

# Add tools directory to path so we can import the module
sys.path.insert(0, str(Path(__file__).parent.parent / "tools"))

from prodos_text_to_linux import (
    clear_prodos_text_metadata,
    convert_file_in_place,
    main,
    normalize_line_endings,
    parse_args,
)


class TestNormalizeLineEndings(unittest.TestCase):
    """Test normalize_line_endings function."""

    def test_cr_to_lf(self):
        r"""Standalone CR (\\r) should be converted to LF (\\n)."""
        data = b"line1\rline2\rline3\r"
        expected = b"line1\nline2\nline3\n"
        self.assertEqual(normalize_line_endings(data), expected)

    def test_crlf_to_lf(self):
        r"""CRLF (\\r\\n) should be converted to single LF (\\n)."""
        data = b"line1\r\nline2\r\nline3\r\n"
        expected = b"line1\nline2\nline3\n"
        self.assertEqual(normalize_line_endings(data), expected)

    def test_preserve_existing_lf(self):
        """Standalone LF should be preserved."""
        data = b"line1\nline2\nline3\n"
        expected = b"line1\nline2\nline3\n"
        self.assertEqual(normalize_line_endings(data), expected)

    def test_mixed_line_endings_no_double_lf(self):
        """Mixed line endings should not produce double LF."""
        data = b"line1\rline2\r\nline3\nline4\r"
        expected = b"line1\nline2\nline3\nline4\n"
        self.assertEqual(normalize_line_endings(data), expected)

    def test_empty_data(self):
        """Empty data should remain empty."""
        self.assertEqual(normalize_line_endings(b""), b"")

    def test_no_line_endings(self):
        """Data without line endings should be unchanged."""
        data = b"single line no ending"
        self.assertEqual(normalize_line_endings(data), data)


class TestClearProdosTextMetadata(unittest.TestCase):
    """Test clear_prodos_text_metadata function."""

    def _xattr_supported(self, path: str) -> bool:
        """Check if xattrs are supported on the given path."""
        try:
            os.setxattr(path, "user.test", b"test")
            os.removexattr(path, "user.test")
            return True
        except OSError as e:
            if e.errno in (errno.ENOTSUP, errno.EOPNOTSUPP, errno.ENOSYS):
                return False
            raise

    def test_removes_all_prodos_xattrs(self):
        """Should remove all ProDOS xattrs when present."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            # Set the xattrs first
            os.setxattr(path, "user.prodos8.file_type", b"04")
            os.setxattr(path, "user.prodos8.aux_type", b"0000")
            os.setxattr(path, "user.prodos8.storage_type", b"01")
            os.setxattr(path, "user.prodos8.access", b"dn-..-wr")

            clear_prodos_text_metadata(path)

            for name in (
                "user.prodos8.file_type",
                "user.prodos8.aux_type",
                "user.prodos8.storage_type",
                "user.prodos8.access",
            ):
                with self.assertRaises(OSError):
                    os.getxattr(path, name)
        finally:
            os.unlink(path)

    def test_does_not_raise_on_missing_xattrs(self):
        """Should not raise when xattrs are already absent."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            # Call without setting xattrs first – must not raise
            clear_prodos_text_metadata(path)
        finally:
            os.unlink(path)

    def test_raises_on_nonexistent_file(self):
        """Should raise an exception for nonexistent files."""
        with self.assertRaises(OSError):
            clear_prodos_text_metadata("/nonexistent/file/path")


class TestConvertFileInPlace(unittest.TestCase):
    """Test convert_file_in_place function."""

    def _xattr_supported(self, path: str) -> bool:
        """Check if xattrs are supported on the given path."""
        try:
            os.setxattr(path, "user.test", b"test")
            os.removexattr(path, "user.test")
            return True
        except OSError as e:
            if e.errno in (errno.ENOTSUP, errno.EOPNOTSUPP, errno.ENOSYS):
                return False
            raise

    def test_converts_cr_to_lf_in_place(self):
        """Should convert CR line endings to LF and write back."""
        with tempfile.NamedTemporaryFile(mode="wb", delete=False) as f:
            f.write(b"line1\rline2\rline3\r")
            path = f.name

        try:
            convert_file_in_place(path, clear_metadata=False)

            with open(path, "rb") as f:
                content = f.read()
            self.assertEqual(content, b"line1\nline2\nline3\n")
        finally:
            os.unlink(path)

    def test_converts_crlf_to_lf_in_place(self):
        """Should convert CRLF line endings to LF."""
        with tempfile.NamedTemporaryFile(mode="wb", delete=False) as f:
            f.write(b"line1\r\nline2\r\nline3\r\n")
            path = f.name

        try:
            convert_file_in_place(path, clear_metadata=False)

            with open(path, "rb") as f:
                content = f.read()
            self.assertEqual(content, b"line1\nline2\nline3\n")
        finally:
            os.unlink(path)

    def test_clears_xattrs_by_default(self):
        """Should remove ProDOS xattrs after conversion by default."""
        with tempfile.NamedTemporaryFile(mode="wb", delete=False) as f:
            f.write(b"test\r")
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            os.setxattr(path, "user.prodos8.file_type", b"04")
            os.setxattr(path, "user.prodos8.aux_type", b"0000")
            os.setxattr(path, "user.prodos8.storage_type", b"01")
            os.setxattr(path, "user.prodos8.access", b"dn-..-wr")

            convert_file_in_place(path)

            for name in (
                "user.prodos8.file_type",
                "user.prodos8.aux_type",
                "user.prodos8.storage_type",
                "user.prodos8.access",
            ):
                with self.assertRaises(OSError):
                    os.getxattr(path, name)
        finally:
            os.unlink(path)

    def test_keep_metadata_preserves_xattrs(self):
        """Should preserve ProDOS xattrs when clear_metadata=False."""
        with tempfile.NamedTemporaryFile(mode="wb", delete=False) as f:
            f.write(b"test\r")
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            os.setxattr(path, "user.prodos8.file_type", b"04")
            os.setxattr(path, "user.prodos8.access", b"dn-..-wr")

            convert_file_in_place(path, clear_metadata=False)

            self.assertEqual(os.getxattr(path, "user.prodos8.file_type"), b"04")
            self.assertEqual(os.getxattr(path, "user.prodos8.access"), b"dn-..-wr")
        finally:
            os.unlink(path)

    def test_preserves_file_permissions(self):
        """Should preserve original file permissions."""
        with tempfile.NamedTemporaryFile(mode="wb", delete=False) as f:
            f.write(b"test\r")
            path = f.name

        try:
            os.chmod(path, 0o644)
            convert_file_in_place(path, clear_metadata=False)
            mode = os.stat(path).st_mode & 0o777
            self.assertEqual(mode, 0o644)
        finally:
            os.unlink(path)

    def test_raises_on_nonexistent_file(self):
        """Should raise OSError for nonexistent file."""
        with self.assertRaises(OSError):
            convert_file_in_place("/nonexistent/file/path")


class TestParseArgs(unittest.TestCase):
    """Test parse_args function."""

    def test_path_required(self):
        """path argument is required."""
        with self.assertRaises(SystemExit):
            parse_args([])

    def test_path_parsed(self):
        """path argument is captured."""
        args = parse_args(["/some/file.txt"])
        self.assertEqual(args.path, "/some/file.txt")

    def test_keep_metadata_default_false(self):
        """--keep-metadata default is False."""
        args = parse_args(["/some/file.txt"])
        self.assertFalse(args.keep_metadata)

    def test_keep_metadata_flag(self):
        """--keep-metadata flag sets keep_metadata to True."""
        args = parse_args(["/some/file.txt", "--keep-metadata"])
        self.assertTrue(args.keep_metadata)


class TestMain(unittest.TestCase):
    """Test main() entry point."""

    def test_returns_0_on_success(self):
        """main() should return 0 when conversion succeeds."""
        with tempfile.NamedTemporaryFile(mode="wb", delete=False) as f:
            f.write(b"line1\rline2\r")
            path = f.name

        try:
            result = main([path, "--keep-metadata"])
            self.assertEqual(result, 0)
        finally:
            os.unlink(path)

    def test_returns_1_on_oserror(self):
        """main() should return 1 for a non-existent file."""
        result = main(["/nonexistent/path/file.txt"])
        self.assertEqual(result, 1)

    def test_returns_nonzero_for_missing_arg(self):
        """main() should return non-zero when path argument is missing."""
        result = main([])
        self.assertNotEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
