#!/usr/bin/env python3
"""Unit tests for edasm_setup.py"""

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

# Add tools directory to path so we can import the module
sys.path.insert(0, str(Path(__file__).parent.parent / "tools"))

from edasm_setup import (  # type: ignore[import-not-found]
    check_cadius_available,
    discover_system_file,
    parse_text_mapping,
    run_emulator,
    validate_disk_image_extension,
    validate_system_file,
)


class TestDiskImageValidation(unittest.TestCase):
    """Test disk image extension validation."""

    def test_accepts_2mg_extension(self):
        """Should accept .2mg extension."""
        self.assertTrue(validate_disk_image_extension("disk.2mg"))

    def test_rejects_3mg_extension(self):
        """Should reject .3mg extension (not currently supported)."""
        with self.assertRaises(ValueError) as cm:
            validate_disk_image_extension("disk.3mg")
        self.assertIn("extension", str(cm.exception).lower())
        self.assertIn(".2mg", str(cm.exception))

    def test_rejects_other_extensions(self):
        """Should reject other extensions."""
        for ext in [".po", ".dsk", ".img", ".iso"]:
            with self.assertRaises(ValueError):
                validate_disk_image_extension(f"disk{ext}")

    def test_case_insensitive(self):
        """Extension check should be case-insensitive."""
        self.assertTrue(validate_disk_image_extension("DISK.2MG"))
        self.assertTrue(validate_disk_image_extension("disk.2Mg"))

    def test_no_extension_fails(self):
        """Files without extension should fail."""
        with self.assertRaises(ValueError):
            validate_disk_image_extension("disk")


class TestTextMappingParsing(unittest.TestCase):
    """Test parsing of --text SRC[:DEST] arguments."""

    def test_simple_source_only(self):
        """SRC without :DEST should map to basename of SRC."""
        src, dest = parse_text_mapping("main.asm")
        self.assertEqual(src, "main.asm")
        self.assertEqual(dest, "main.asm")

    def test_source_with_path_only(self):
        """SRC with path but no :DEST should map to basename."""
        src, dest = parse_text_mapping("/path/to/source.asm")
        self.assertEqual(src, "/path/to/source.asm")
        self.assertEqual(dest, "source.asm")

    def test_explicit_dest(self):
        """SRC:DEST should map to specified DEST."""
        src, dest = parse_text_mapping("file.txt:target.txt")
        self.assertEqual(src, "file.txt")
        self.assertEqual(dest, "target.txt")

    def test_explicit_dest_with_path(self):
        """SRC:DEST with DEST path should preserve it."""
        src, dest = parse_text_mapping("file.txt:subdir/target.txt")
        self.assertEqual(src, "file.txt")
        self.assertEqual(dest, "subdir/target.txt")

    def test_empty_string_fails(self):
        """Empty string should fail."""
        with self.assertRaises(ValueError):
            parse_text_mapping("")

    def test_only_colon_fails(self):
        """Only a colon should fail."""
        with self.assertRaises(ValueError):
            parse_text_mapping(":")

    def test_missing_source_fails(self):
        """:DEST without source should fail."""
        with self.assertRaises(ValueError):
            parse_text_mapping(":dest.txt")

    def test_empty_dest_fails(self):
        """SRC: with empty destination should fail."""
        with self.assertRaises(ValueError):
            parse_text_mapping("src.txt:")


class TestSystemFileValidation(unittest.TestCase):
    """Test system file validation."""

    def test_valid_system_file(self):
        """File starting with 0x4C (JMP) should be valid."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"\x4c\x00\x08")  # JMP $0800
            f.flush()
            path = f.name

        try:
            self.assertTrue(validate_system_file(path))
        finally:
            os.unlink(path)

    def test_invalid_first_byte(self):
        """File not starting with 0x4C should be invalid."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"\x00\x00\x00")
            f.flush()
            path = f.name

        try:
            self.assertFalse(validate_system_file(path))
        finally:
            os.unlink(path)

    def test_empty_file(self):
        """Empty file should be invalid."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.flush()
            path = f.name

        try:
            self.assertFalse(validate_system_file(path))
        finally:
            os.unlink(path)

    def test_nonexistent_file(self):
        """Nonexistent file should raise OSError."""
        with self.assertRaises(OSError):
            validate_system_file("/nonexistent/file")


class TestSystemFileDiscovery(unittest.TestCase):
    """Test automatic system file discovery."""

    def test_single_system_file_found(self):
        """Single .SYSTEM file with JMP should be discovered."""
        with tempfile.TemporaryDirectory() as tmpdir:
            system_path = Path(tmpdir) / "EDASM.SYSTEM"
            system_path.write_bytes(b"\x4c\x00\x08")

            result = discover_system_file(tmpdir)
            self.assertEqual(result, str(system_path))

    def test_case_insensitive_system_extension(self):
        """Should find .system, .SYSTEM, .System, etc."""
        with tempfile.TemporaryDirectory() as tmpdir:
            system_path = Path(tmpdir) / "EDASM.system"
            system_path.write_bytes(b"\x4c\x00\x08")

            result = discover_system_file(tmpdir)
            self.assertEqual(result, str(system_path))

    def test_sys_extension_works(self):
        """Should also find .SYS extension."""
        with tempfile.TemporaryDirectory() as tmpdir:
            system_path = Path(tmpdir) / "PRODOS.SYS"
            system_path.write_bytes(b"\x4c\x00\x08")

            result = discover_system_file(tmpdir)
            self.assertEqual(result, str(system_path))

    def test_multiple_candidates_fails(self):
        """Multiple system file candidates should fail."""
        with tempfile.TemporaryDirectory() as tmpdir:
            sys1 = Path(tmpdir) / "EDASM.SYSTEM"
            sys2 = Path(tmpdir) / "PRODOS.SYSTEM"
            sys1.write_bytes(b"\x4c\x00\x08")
            sys2.write_bytes(b"\x4c\x00\x20")

            with self.assertRaises(ValueError) as cm:
                discover_system_file(tmpdir)
            self.assertIn("ambiguous", str(cm.exception).lower())
            self.assertIn("multiple", str(cm.exception).lower())

    def test_no_candidates_fails(self):
        """No system file candidates should fail."""
        with tempfile.TemporaryDirectory() as tmpdir:
            with self.assertRaises(ValueError) as cm:
                discover_system_file(tmpdir)
            self.assertIn("no system file", str(cm.exception).lower())

    def test_ignores_non_jmp_files(self):
        """Files not starting with 0x4C should be ignored."""
        with tempfile.TemporaryDirectory() as tmpdir:
            fake = Path(tmpdir) / "FAKE.SYSTEM"
            fake.write_bytes(b"\x00\x00\x00")

            with self.assertRaises(ValueError) as cm:
                discover_system_file(tmpdir)
            self.assertIn("no system file", str(cm.exception).lower())

    def test_fallback_to_xattr_ff(self):
        """Should fallback to checking file_type=ff xattr."""
        with tempfile.TemporaryDirectory() as tmpdir:
            sys_file = Path(tmpdir) / "SYSTEM"
            sys_file.write_bytes(b"\x4c\x00\x08")

            # Set the xattr
            try:
                os.setxattr(sys_file, "user.prodos8.file_type", b"ff")
            except OSError:
                # Skip test if xattrs not supported
                self.skipTest("xattrs not supported on this filesystem")

            result = discover_system_file(tmpdir)
            self.assertEqual(result, str(sys_file))

    def test_prefers_system_extension_over_xattr(self):
        """Should prefer .SYSTEM/.SYS files over xattr-based discovery."""
        with tempfile.TemporaryDirectory() as tmpdir:
            sys1 = Path(tmpdir) / "EDASM.SYSTEM"
            sys2 = Path(tmpdir) / "OTHER"
            sys1.write_bytes(b"\x4c\x00\x08")
            sys2.write_bytes(b"\x4c\x00\x20")

            try:
                os.setxattr(sys2, "user.prodos8.file_type", b"ff")
            except OSError:
                pass  # OK if xattrs not supported

            # Should find sys1 (extension-based) even if sys2 has xattr
            result = discover_system_file(tmpdir)
            self.assertEqual(result, str(sys1))


class TestCadiusAvailability(unittest.TestCase):
    """Test cadius availability checking."""

    @mock.patch("shutil.which")
    def test_cadius_missing_hard_fails(self, mock_which):
        """Missing cadius should cause hard failure when extraction needed."""
        mock_which.return_value = None

        with self.assertRaises(RuntimeError) as cm:
            check_cadius_available("cadius")
        self.assertIn("cadius", str(cm.exception).lower())

    @mock.patch("shutil.which")
    def test_cadius_present_succeeds(self, mock_which):
        """Present cadius should pass check."""
        mock_which.return_value = "/usr/local/bin/cadius"

        # Should return resolved path
        resolved = check_cadius_available("cadius")
        self.assertEqual(resolved, "/usr/local/bin/cadius")

    def test_explicit_cadius_path_missing_fails(self):
        """Explicit non-existent cadius path should fail."""
        with self.assertRaises(RuntimeError):
            check_cadius_available("/does/not/exist/cadius")


class TestRunEmulator(unittest.TestCase):
    """Test emulator command invocation formatting."""

    @mock.patch("subprocess.run")
    def test_run_emulator_uses_split_options(self, mock_run):
        """Runner options should be passed as separate argv entries."""
        mock_run.return_value = mock.Mock(returncode=0)

        run_emulator(
            runner_path="build/prodos8emu_run",
            rom_path="rom.bin",
            system_file="EDASM.SYSTEM",
            volume_root="work/volumes",
            max_instructions=1234,
        )

        called_cmd = mock_run.call_args[0][0]
        self.assertEqual(called_cmd[0], "build/prodos8emu_run")
        self.assertIn("--volume-root", called_cmd)
        self.assertIn("work/volumes", called_cmd)
        self.assertIn("--max-instructions", called_cmd)
        self.assertIn("1234", called_cmd)


class TestEndToEndMocking(unittest.TestCase):
    """Test end-to-end scenarios with mocked external dependencies."""

    @mock.patch("subprocess.run")
    @mock.patch("shutil.which")
    def test_no_run_flag_skips_execution(self, mock_which, mock_run):
        """--no-run should perform setup without invoking runner."""
        mock_which.return_value = "/usr/local/bin/cadius"
        mock_run.return_value = mock.Mock(returncode=0, stdout="", stderr="")

        # Import locally to test argument parsing
        from edasm_setup import parse_args  # type: ignore[import-not-found]

        with tempfile.TemporaryDirectory() as tmpdir:
            args = parse_args(
                [
                    "--work-dir",
                    tmpdir,
                    "--disk-image",
                    "test.2mg",
                    "--rom",
                    "test.rom",
                    "--no-run",
                ]
            )

            self.assertTrue(args.no_run)
            self.assertEqual(args.disk_image, "test.2mg")


if __name__ == "__main__":
    unittest.main()
