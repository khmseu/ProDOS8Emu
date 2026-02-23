#!/usr/bin/env python3
"""Unit tests for linux_to_prodos_text.py - Phase 1 line ending and ASCII conversion."""

import sys
import unittest
from pathlib import Path

# Add tools directory to path so we can import the module
sys.path.insert(0, str(Path(__file__).parent.parent / "tools"))

from linux_to_prodos_text import normalize_line_endings, convert_to_ascii


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


if __name__ == "__main__":
    unittest.main()
