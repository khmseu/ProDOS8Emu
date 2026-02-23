#!/usr/bin/env python3
"""Unit tests for linux_to_prodos_text.py - Phase 1 and 2: line ending, ASCII conversion, and file operations."""

import errno
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

# Add tools directory to path so we can import the module
sys.path.insert(0, str(Path(__file__).parent.parent / "tools"))

from linux_to_prodos_text import (
    normalize_line_endings,
    convert_to_ascii,
    set_prodos_text_metadata,
    convert_file_in_place,
)


class TestNormalizeLineEndings(unittest.TestCase):
    """Test normalize_line_endings function."""

    def test_lf_to_cr(self):
        """LF (\\n) should be converted to CR (\\r)."""
        data = b"line1\nline2\nline3\n"
        expected = b"line1\rline2\rline3\r"
        self.assertEqual(normalize_line_endings(data), expected)

    def test_crlf_to_cr(self):
        """CRLF (\\r\\n) should be converted to single CR (\\r)."""
        data = b"line1\r\nline2\r\nline3\r\n"
        expected = b"line1\rline2\rline3\r"
        self.assertEqual(normalize_line_endings(data), expected)

    def test_preserve_existing_cr(self):
        """Standalone CR should be preserved."""
        data = b"line1\rline2\rline3\r"
        expected = b"line1\rline2\rline3\r"
        self.assertEqual(normalize_line_endings(data), expected)

    def test_mixed_line_endings_no_double_cr(self):
        """Mixed line endings should not produce double CR."""
        # Mix of LF, CRLF, and standalone CR
        data = b"line1\nline2\r\nline3\rline4\n"
        expected = b"line1\rline2\rline3\rline4\r"
        self.assertEqual(normalize_line_endings(data), expected)

    def test_empty_data(self):
        """Empty data should remain empty."""
        self.assertEqual(normalize_line_endings(b""), b"")

    def test_no_line_endings(self):
        """Data without line endings should be unchanged."""
        data = b"single line no ending"
        self.assertEqual(normalize_line_endings(data), data)


class TestConvertToAscii(unittest.TestCase):
    """Test convert_to_ascii function."""

    def test_strict_mode_accepts_ascii(self):
        """Strict mode should accept valid ASCII (< 0x80)."""
        data = b"Hello, World!\r"
        self.assertEqual(convert_to_ascii(data, strict=True), data)

    def test_strict_mode_rejects_non_ascii(self):
        """Strict mode should raise ValueError on bytes >= 0x80."""
        data = b"Caf\xc3\xa9"  # Café in UTF-8
        with self.assertRaises(ValueError) as cm:
            convert_to_ascii(data, strict=True)
        self.assertIn("non-ASCII", str(cm.exception))

    def test_lossy_mode_accepts_ascii(self):
        """Lossy mode should accept valid ASCII unchanged."""
        data = b"Hello, World!\r"
        self.assertEqual(convert_to_ascii(data, strict=False), data)

    def test_lossy_mode_replaces_non_ascii(self):
        """Lossy mode should replace bytes >= 0x80 with '?'."""
        data = b"Caf\xc3\xa9"  # Café in UTF-8
        expected = b"Caf??"  # Both 0xc3 and 0xa9 replaced
        self.assertEqual(convert_to_ascii(data, strict=False), expected)

    def test_lossy_mode_mixed_content(self):
        """Lossy mode should preserve ASCII and replace non-ASCII."""
        data = b"Hello \xe4\xb8\x96\xe7\x95\x8c"  # "Hello 世界" (UTF-8)
        # All bytes >= 0x80 should be replaced with '?'
        expected = b"Hello ??????"
        self.assertEqual(convert_to_ascii(data, strict=False), expected)

    def test_empty_data_strict(self):
        """Empty data should remain empty in strict mode."""
        self.assertEqual(convert_to_ascii(b"", strict=True), b"")

    def test_empty_data_lossy(self):
        """Empty data should remain empty in lossy mode."""
        self.assertEqual(convert_to_ascii(b"", strict=False), b"")

    def test_boundary_0x7f_allowed(self):
        """0x7F (DEL) should be allowed as it's the highest ASCII value."""
        data = b"test\x7fdata"
        # Should be accepted in both strict and lossy modes
        self.assertEqual(convert_to_ascii(data, strict=True), data)
        self.assertEqual(convert_to_ascii(data, strict=False), data)

    def test_boundary_0x80_rejected_strict(self):
        """0x80 should be rejected in strict mode (first non-ASCII byte)."""
        data = b"test\x80data"
        with self.assertRaises(ValueError) as cm:
            convert_to_ascii(data, strict=True)
        self.assertIn("non-ASCII", str(cm.exception))

    def test_boundary_0x80_replaced_lossy(self):
        """0x80 should be replaced with '?' in lossy mode."""
        data = b"test\x80data"
        expected = b"test?data"
        self.assertEqual(convert_to_ascii(data, strict=False), expected)


class TestSetProdosTextMetadata(unittest.TestCase):
    """Test set_prodos_text_metadata function."""

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

    def test_sets_all_prodos_xattrs(self):
        """Should set all required ProDOS xattrs with default access."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            set_prodos_text_metadata(path)

            # Verify all xattrs are set correctly
            self.assertEqual(os.getxattr(path, "user.prodos8.file_type"), b"04")
            self.assertEqual(os.getxattr(path, "user.prodos8.aux_type"), b"0000")
            self.assertEqual(os.getxattr(path, "user.prodos8.storage_type"), b"01")
            self.assertEqual(os.getxattr(path, "user.prodos8.access"), b"dn-..-wr")
        finally:
            os.unlink(path)

    def test_sets_custom_access(self):
        """Should set custom access value when provided."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            set_prodos_text_metadata(path, access="dnb..-wr")

            self.assertEqual(os.getxattr(path, "user.prodos8.access"), b"dnb..-wr")
        finally:
            os.unlink(path)

    def test_raises_on_nonexistent_file(self):
        """Should raise an exception for nonexistent files."""
        with self.assertRaises(OSError):
            set_prodos_text_metadata("/nonexistent/file/path")


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

    def test_converts_lf_to_cr_in_place(self):
        """Should convert LF line endings to CR and write back."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            f.write(b"line1\nline2\nline3\n")
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            convert_file_in_place(path)

            # Verify content was converted
            with open(path, 'rb') as f:
                content = f.read()
            self.assertEqual(content, b"line1\rline2\rline3\r")
        finally:
            os.unlink(path)

    def test_converts_crlf_to_cr_in_place(self):
        """Should convert CRLF line endings to CR."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            f.write(b"line1\r\nline2\r\nline3\r\n")
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            convert_file_in_place(path)

            with open(path, 'rb') as f:
                content = f.read()
            self.assertEqual(content, b"line1\rline2\rline3\r")
        finally:
            os.unlink(path)

    def test_sets_xattrs_after_conversion(self):
        """Should set ProDOS xattrs after conversion."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            f.write(b"test\n")
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            convert_file_in_place(path)

            # Verify xattrs are set
            self.assertEqual(os.getxattr(path, "user.prodos8.file_type"), b"04")
            self.assertEqual(os.getxattr(path, "user.prodos8.aux_type"), b"0000")
            self.assertEqual(os.getxattr(path, "user.prodos8.storage_type"), b"01")
            self.assertEqual(os.getxattr(path, "user.prodos8.access"), b"dn-..-wr")
        finally:
            os.unlink(path)

    def test_custom_access_parameter(self):
        """Should use custom access parameter."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            f.write(b"test\n")
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            convert_file_in_place(path, access="dnb..-wr")

            self.assertEqual(os.getxattr(path, "user.prodos8.access"), b"dnb..-wr")
        finally:
            os.unlink(path)

    def test_strict_ascii_mode_raises_on_non_ascii(self):
        """Strict ASCII mode should raise on non-ASCII and not modify file."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            original = b"Caf\xc3\xa9\n"
            f.write(original)
            path = f.name

        try:
            with self.assertRaises(ValueError) as cm:
                convert_file_in_place(path, strict_ascii=True)
            self.assertIn("non-ASCII", str(cm.exception))

            # Verify file was not modified
            with open(path, 'rb') as f:
                content = f.read()
            self.assertEqual(content, original)
        finally:
            os.unlink(path)

    def test_lossy_mode_replaces_non_ascii(self):
        """Lossy mode should replace non-ASCII with '?'."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            f.write(b"Caf\xc3\xa9\n")
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            convert_file_in_place(path, strict_ascii=False)

            with open(path, 'rb') as f:
                content = f.read()
            self.assertEqual(content, b"Caf??\r")
        finally:
            os.unlink(path)

    def test_empty_file_conversion(self):
        """Should handle empty files correctly."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            convert_file_in_place(path)

            with open(path, 'rb') as f:
                content = f.read()
            self.assertEqual(content, b"")
        finally:
            os.unlink(path)

    def test_atomicity_xattr_failure_preserves_original(self):
        """If xattr setting fails, original file should be unchanged."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            original = b"line1\nline2\nline3\n"
            f.write(original)
            path = f.name

        try:
            # Mock os.setxattr to raise EACCES when called
            with mock.patch('linux_to_prodos_text.os.setxattr') as mock_setxattr:
                mock_setxattr.side_effect = OSError(errno.EACCES, "Permission denied")
                
                # Attempt conversion - should fail
                with self.assertRaises(OSError) as cm:
                    convert_file_in_place(path)
                self.assertEqual(cm.exception.errno, errno.EACCES)
                
                # Verify original file is unchanged
                with open(path, 'rb') as f:
                    content = f.read()
                self.assertEqual(content, original, "Original file should be unchanged after xattr failure")
        finally:
            os.unlink(path)

    def test_atomicity_preserves_file_permissions(self):
        """Conversion should preserve original file permissions."""
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            f.write(b"test\n")
            path = f.name

        try:
            if not self._xattr_supported(path):
                self.skipTest("xattrs not supported on this filesystem")

            # Set specific permissions (readable/writable by owner only)
            os.chmod(path, 0o600)
            original_mode = os.stat(path).st_mode
            
            convert_file_in_place(path)
            
            # Verify permissions are preserved
            new_mode = os.stat(path).st_mode
            self.assertEqual(new_mode, original_mode, "File permissions should be preserved")
        finally:
            os.unlink(path)


if __name__ == "__main__":
    unittest.main()
