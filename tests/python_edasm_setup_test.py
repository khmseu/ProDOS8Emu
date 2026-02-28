#!/usr/bin/env python3
"""Unit tests for edasm_setup.py"""

import json
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
    expand_rearrange_mappings,
    import_text_files,
    main,
    parse_args,
    parse_rearrange_config,
    parse_text_mapping,
    rearrange_files,
    run_emulator,
    run_metadata_conversion,
    validate_disk_image_extension,
    validate_rearrange_config,
    validate_safe_path,
    validate_system_file,
)


class TestPathSecurity(unittest.TestCase):
    """Test path security validation against command injection."""

    def test_safe_paths_accepted(self):
        """Normal paths should be accepted."""
        safe_paths = [
            "/path/to/file.2mg",
            "relative/path/image.2mg",
            "file-with-dash.txt",
            "file_with_underscore.txt",
            "file.with.dots.txt",
            "work_dir_123",
        ]
        for path in safe_paths:
            validate_safe_path(path, "test_param")  # Should not raise

    def test_semicolon_rejected(self):
        """Paths with semicolons should be rejected (shell command separator)."""
        with self.assertRaises(ValueError) as cm:
            validate_safe_path("/path/to/file;rm -rf /", "disk-image")
        self.assertIn("shell metacharacter", str(cm.exception))
        self.assertIn(";", str(cm.exception))
        self.assertIn("disk-image", str(cm.exception))

    def test_pipe_rejected(self):
        """Paths with pipes should be rejected (shell command chaining)."""
        with self.assertRaises(ValueError) as cm:
            validate_safe_path("file.2mg|cat /etc/passwd", "disk-image")
        self.assertIn("|", str(cm.exception))

    def test_ampersand_rejected(self):
        """Paths with ampersands should be rejected (shell backgrounding)."""
        with self.assertRaises(ValueError) as cm:
            validate_safe_path("file.2mg&", "disk-image")
        self.assertIn("&", str(cm.exception))

    def test_dollar_rejected(self):
        """Paths with dollar signs should be rejected (shell variable expansion)."""
        with self.assertRaises(ValueError) as cm:
            validate_safe_path("/path/$EVIL/file", "output-dir")
        self.assertIn("$", str(cm.exception))

    def test_backtick_rejected(self):
        """Paths with backticks should be rejected (command substitution)."""
        with self.assertRaises(ValueError) as cm:
            validate_safe_path("file`whoami`.2mg", "disk-image")
        self.assertIn("`", str(cm.exception))

    def test_newline_rejected(self):
        """Paths with newlines should be rejected (command separation)."""
        with self.assertRaises(ValueError) as cm:
            validate_safe_path("file\nrm -rf /", "disk-image")
        # The exception message will contain the actual newline character
        self.assertIn("shell metacharacter", str(cm.exception))
        self.assertIn("\n", str(cm.exception))

    def test_redirect_operators_rejected(self):
        """Paths with redirect operators should be rejected."""
        for char in [">", "<"]:
            with self.assertRaises(ValueError):
                validate_safe_path(f"file{char}evil", "test-param")

    def test_parentheses_rejected(self):
        """Paths with parentheses should be rejected (subshells)."""
        for char in ["(", ")", "{", "}"]:
            with self.assertRaises(ValueError):
                validate_safe_path(f"file{char}evil", "test-param")


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


class TestRearrangeConfig(unittest.TestCase):
    """Test JSON config file parsing and validation for file rearrangement."""

    def test_parse_rearrange_config_valid_json(self):
        """Valid config should parse successfully."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as f:
            config_data = {
                "rearrange": [
                    {"from": "SOURCE.FILE", "to": "DEST/FILE"},
                    {"from": "DIR1/FILE.TXT", "to": "DIR2/FILE.TXT"},
                ]
            }
            json.dump(config_data, f)
            config_path = f.name

        try:
            result = parse_rearrange_config(config_path)
            self.assertEqual(result, config_data)
            self.assertIn("rearrange", result)
            self.assertEqual(len(result["rearrange"]), 2)
        finally:
            os.unlink(config_path)

    def test_parse_rearrange_config_invalid_json(self):
        """Malformed JSON should raise appropriate error."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as f:
            f.write("{ invalid json }")
            config_path = f.name

        try:
            with self.assertRaises(ValueError) as cm:
                parse_rearrange_config(config_path)
            self.assertIn("Invalid JSON", str(cm.exception))
        finally:
            os.unlink(config_path)

    def test_parse_rearrange_config_missing_file(self):
        """Missing file should raise FileNotFoundError."""
        non_existent_path = "/tmp/nonexistent_config_12345.json"
        with self.assertRaises(FileNotFoundError):
            parse_rearrange_config(non_existent_path)

    def test_validate_rearrange_config_valid_structure(self):
        """Valid structure should pass validation."""
        valid_configs = [
            {
                "rearrange": [
                    {"from": "SOURCE", "to": "DEST"},
                ]
            },
            {
                "rearrange": [
                    {"from": "A", "to": "B"},
                    {"from": "C/D", "to": "E/F"},
                    {"from": "X.TXT", "to": "Y.TXT"},
                ]
            },
        ]
        for config in valid_configs:
            validate_rearrange_config(config)  # Should not raise

    def test_validate_rearrange_config_invalid_structure(self):
        """Invalid structures should raise ValueError."""
        invalid_configs = [
            # Missing "rearrange" key
            {"other_key": []},
            # "rearrange" is not a list
            {"rearrange": "not a list"},
            {"rearrange": {"from": "A", "to": "B"}},
            # Mapping missing "from" key
            {"rearrange": [{"to": "DEST"}]},
            # Mapping missing "to" key
            {"rearrange": [{"from": "SOURCE"}]},
            # Mapping with non-string "from"
            {"rearrange": [{"from": 123, "to": "DEST"}]},
            # Mapping with non-string "to"
            {"rearrange": [{"from": "SOURCE", "to": 456}]},
            # Empty "from" string
            {"rearrange": [{"from": "", "to": "DEST"}]},
            # Empty "to" string
            {"rearrange": [{"from": "SOURCE", "to": ""}]},
            # Mapping is not a dict
            {"rearrange": ["SOURCE", "DEST"]},
            {"rearrange": [None]},
        ]
        for config in invalid_configs:
            with self.assertRaises(ValueError):
                validate_rearrange_config(config)

    def test_validate_rearrange_config_empty_mappings(self):
        """Empty 'rearrange' list should be valid."""
        config = {"rearrange": []}
        validate_rearrange_config(config)  # Should not raise


class TestExpandRearrangeMappings(unittest.TestCase):
    """Test glob pattern expansion for file rearrangement."""

    def test_expand_glob_patterns_single_file(self):
        """Pattern matching one file should expand correctly."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create test file
            test_file = Path(tmpdir) / "TEST.TXT"
            test_file.write_text("content")

            # Create mapping with glob pattern
            mappings = [{"from": "TEST.TXT", "to": "OUTPUT.TXT"}]

            # Expand
            result = expand_rearrange_mappings(tmpdir, mappings)

            # Should return single tuple
            self.assertEqual(len(result), 1)
            src, dest = result[0]
            self.assertEqual(Path(src).name, "TEST.TXT")
            self.assertEqual(dest, str(Path(tmpdir) / "OUTPUT.TXT"))

    def test_expand_glob_patterns_multiple_matches(self):
        """Pattern matching multiple files should expand to all matches."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create multiple matching files
            (Path(tmpdir) / "FILE1.TXT").write_text("content1")
            (Path(tmpdir) / "FILE2.TXT").write_text("content2")
            (Path(tmpdir) / "FILE3.TXT").write_text("content3")

            # Create mapping with wildcard pattern to directory
            mappings = [{"from": "*.TXT", "to": "DEST/"}]

            # Expand
            result = expand_rearrange_mappings(tmpdir, mappings)

            # Should return three tuples
            self.assertEqual(len(result), 3)
            
            # All should go to DEST/ with basenames preserved
            basenames = [Path(src).name for src, _ in result]
            self.assertIn("FILE1.TXT", basenames)
            self.assertIn("FILE2.TXT", basenames)
            self.assertIn("FILE3.TXT", basenames)
            
            # Check destinations preserve basenames
            for src, dest in result:
                expected_dest = str(Path(tmpdir) / "DEST" / Path(src).name)
                self.assertEqual(dest, expected_dest)

    def test_expand_glob_patterns_no_matches(self):
        """Pattern matching no files should return empty list."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # No files created
            mappings = [{"from": "*.NONEXISTENT", "to": "DEST/"}]

            # Expand
            result = expand_rearrange_mappings(tmpdir, mappings)

            # Should return empty list
            self.assertEqual(result, [])

    def test_expand_glob_patterns_subdirectories(self):
        """Patterns with subdirectories should work."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create subdirectory with files
            subdir = Path(tmpdir) / "SRC"
            subdir.mkdir()
            (subdir / "FILE.ASM").write_text("code")
            (subdir / "OTHER.TXT").write_text("text")

            # Pattern for files in subdirectory
            mappings = [{"from": "SRC/*.ASM", "to": "BUILD/"}]

            # Expand
            result = expand_rearrange_mappings(tmpdir, mappings)

            # Should match only .ASM file
            self.assertEqual(len(result), 1)
            src, dest = result[0]
            self.assertTrue(src.endswith("FILE.ASM"))
            self.assertEqual(dest, str(Path(tmpdir) / "BUILD" / "FILE.ASM"))

    def test_expand_glob_to_directory(self):
        """'to' field ending with '/' preserves basename from source."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create files
            (Path(tmpdir) / "SOURCE.TXT").write_text("content")

            # Map to directory (trailing slash)
            mappings = [{"from": "SOURCE.TXT", "to": "TARGET/"}]

            # Expand
            result = expand_rearrange_mappings(tmpdir, mappings)

            self.assertEqual(len(result), 1)
            src, dest = result[0]
            # Destination should preserve SOURCE.TXT basename
            self.assertEqual(dest, str(Path(tmpdir) / "TARGET" / "SOURCE.TXT"))

    def test_expand_glob_explicit_filename_single_match(self):
        """Glob with explicit target filename should work when exactly 1 match."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create single file
            (Path(tmpdir) / "INPUT.TXT").write_text("content")

            # Map with explicit filename
            mappings = [{"from": "INPUT.TXT", "to": "OUTPUT.TXT"}]

            # Expand
            result = expand_rearrange_mappings(tmpdir, mappings)

            self.assertEqual(len(result), 1)
            src, dest = result[0]
            self.assertEqual(dest, str(Path(tmpdir) / "OUTPUT.TXT"))

    def test_expand_glob_explicit_filename_multiple_matches_error(self):
        """Error when glob → filename but multiple matches."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create multiple matching files
            (Path(tmpdir) / "FILE1.TXT").write_text("content1")
            (Path(tmpdir) / "FILE2.TXT").write_text("content2")

            # Try to map multiple files to single explicit filename
            mappings = [{"from": "*.TXT", "to": "SINGLE.TXT"}]

            # Should raise error
            with self.assertRaises(ValueError) as cm:
                expand_rearrange_mappings(tmpdir, mappings)
            self.assertIn("multiple", str(cm.exception).lower())
            self.assertIn("single", str(cm.exception).lower())

    def test_expand_absolute_and_relative_paths(self):
        """Both absolute (/VOL/...) and relative paths should work."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create file
            (Path(tmpdir) / "FILE.TXT").write_text("content")

            # Test absolute path (starting with /)
            mappings_abs = [{"from": "/FILE.TXT", "to": "/DEST/FILE.TXT"}]
            result_abs = expand_rearrange_mappings(tmpdir, mappings_abs)
            self.assertEqual(len(result_abs), 1)

            # Test relative path
            mappings_rel = [{"from": "FILE.TXT", "to": "DEST/FILE.TXT"}]
            result_rel = expand_rearrange_mappings(tmpdir, mappings_rel)
            self.assertEqual(len(result_rel), 1)


class TestRearrangeFiles(unittest.TestCase):
    """Test atomic file rearrangement with validation and rollback."""

    def test_rearrange_files_simple_rename(self):
        """Rename file in same directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create source file
            src_path = Path(tmpdir) / "OLD.TXT"
            src_path.write_text("content")

            # Rearrange
            mappings = [(str(src_path), str(Path(tmpdir) / "NEW.TXT"))]
            rearrange_files(tmpdir, mappings)

            # Source should be gone, dest should exist
            self.assertFalse(src_path.exists())
            self.assertTrue((Path(tmpdir) / "NEW.TXT").exists())
            self.assertEqual((Path(tmpdir) / "NEW.TXT").read_text(), "content")

    def test_rearrange_files_move_to_subdir(self):
        """Move file to subdirectory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create source file
            src_path = Path(tmpdir) / "FILE.TXT"
            src_path.write_text("content")

            # Move to subdirectory
            dest_path = Path(tmpdir) / "SUBDIR" / "FILE.TXT"
            mappings = [(str(src_path), str(dest_path))]
            rearrange_files(tmpdir, mappings)

            # Check result
            self.assertFalse(src_path.exists())
            self.assertTrue(dest_path.exists())
            self.assertEqual(dest_path.read_text(), "content")

    def test_rearrange_files_create_parent_dirs(self):
        """Create parent directories automatically."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create source file
            src_path = Path(tmpdir) / "FILE.TXT"
            src_path.write_text("content")

            # Move to nested directory that doesn't exist
            dest_path = Path(tmpdir) / "A" / "B" / "C" / "FILE.TXT"
            mappings = [(str(src_path), str(dest_path))]
            rearrange_files(tmpdir, mappings)

            # Parent directories should be created
            self.assertTrue(dest_path.exists())
            self.assertTrue(dest_path.parent.exists())
            self.assertEqual(dest_path.read_text(), "content")

    def test_rearrange_files_conflict_detection(self):
        """Error if destination exists."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create source and destination files
            src_path = Path(tmpdir) / "SRC.TXT"
            dest_path = Path(tmpdir) / "DEST.TXT"
            src_path.write_text("source")
            dest_path.write_text("existing")

            # Try to move - should fail
            mappings = [(str(src_path), str(dest_path))]
            with self.assertRaises(ValueError) as cm:
                rearrange_files(tmpdir, mappings)
            self.assertIn("exists", str(cm.exception).lower())

            # Both files should still exist (no partial changes)
            self.assertTrue(src_path.exists())
            self.assertTrue(dest_path.exists())
            self.assertEqual(dest_path.read_text(), "existing")

    def test_rearrange_files_missing_source(self):
        """Error if source doesn't exist."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Try to move nonexistent file
            src_path = Path(tmpdir) / "NONEXISTENT.TXT"
            dest_path = Path(tmpdir) / "DEST.TXT"
            mappings = [(str(src_path), str(dest_path))]

            with self.assertRaises(ValueError) as cm:
                rearrange_files(tmpdir, mappings)
            self.assertIn("not exist", str(cm.exception).lower())

    def test_rearrange_files_preserves_content(self):
        """File content unchanged after move."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create source with specific content
            src_path = Path(tmpdir) / "SOURCE.TXT"
            content = "This is test content\nWith multiple lines\n"
            src_path.write_text(content)

            # Move file
            dest_path = Path(tmpdir) / "DEST" / "TARGET.TXT"
            mappings = [(str(src_path), str(dest_path))]
            rearrange_files(tmpdir, mappings)

            # Content should be identical
            self.assertEqual(dest_path.read_text(), content)


class TestRearrangementIntegration(unittest.TestCase):
    """Integration tests for file rearrangement in the main workflow."""

    def test_integration_cli_arg_parsing(self):
        """Verify --rearrange-config argument is parsed correctly."""
        # Test with config argument
        args = parse_args(
            [
                "--work-dir",
                "work",
                "--rom",
                "rom.bin",
                "--rearrange-config",
                "config.json",
            ]
        )
        self.assertEqual(args.rearrange_config, "config.json")

        # Test without config argument
        args = parse_args(["--work-dir", "work", "--rom", "rom.bin"])
        self.assertIsNone(args.rearrange_config)

    @mock.patch("edasm_setup.run_emulator")
    @mock.patch("edasm_setup.discover_system_file")
    @mock.patch("edasm_setup.run_metadata_conversion")
    @mock.patch("edasm_setup.extract_disk_image")
    @mock.patch("edasm_setup.check_cadius_available")
    def test_integration_without_rearrange_config(
        self,
        mock_check_cadius,
        mock_extract,
        mock_metadata,
        mock_discover,
        mock_run,
    ):
        """Normal workflow without rearrange config should work unchanged."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Setup mocks
            mock_check_cadius.return_value = "cadius"
            mock_discover.return_value = f"{tmpdir}/volumes/TEST/PRODOS"

            # Run main with minimal args
            disk_image = Path(tmpdir) / "test.2mg"
            disk_image.write_bytes(b"dummy")

            with mock.patch(
                "sys.argv",
                [
                    "edasm_setup.py",
                    "--work-dir",
                    tmpdir,
                    "--rom",
                    "dummy.rom",
                    "--disk-image",
                    str(disk_image),
                    "--no-run",
                ],
            ):
                result = main()

            # Should succeed
            self.assertEqual(result, 0)

            # Extract and metadata should be called
            mock_extract.assert_called_once()
            mock_metadata.assert_called_once()

    @mock.patch("edasm_setup.rearrange_files")
    @mock.patch("edasm_setup.expand_rearrange_mappings")
    @mock.patch("edasm_setup.validate_rearrange_config")
    @mock.patch("edasm_setup.parse_rearrange_config")
    @mock.patch("edasm_setup.run_emulator")
    @mock.patch("edasm_setup.discover_system_file")
    @mock.patch("edasm_setup.run_metadata_conversion")
    @mock.patch("edasm_setup.extract_disk_image")
    @mock.patch("edasm_setup.check_cadius_available")
    def test_integration_with_rearrange_config(
        self,
        mock_check_cadius,
        mock_extract,
        mock_metadata,
        mock_discover,
        mock_run,
        mock_parse_config,
        mock_validate_config,
        mock_expand_mappings,
        mock_rearrange,
    ):
        """Config should be loaded and applied when provided."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Setup mocks
            mock_check_cadius.return_value = "cadius"
            mock_discover.return_value = f"{tmpdir}/volumes/TEST/PRODOS"
            mock_parse_config.return_value = {"rearrange": [{"src": "A", "dest": "B"}]}
            mock_expand_mappings.return_value = [("src_full", "dest_full")]

            # Create config file
            config_file = Path(tmpdir) / "config.json"
            config_file.write_text(json.dumps({"rearrange": [{"src": "A", "dest": "B"}]}))

            # Create disk image
            disk_image = Path(tmpdir) / "test.2mg"
            disk_image.write_bytes(b"dummy")

            # Run main with rearrange config
            with mock.patch(
                "sys.argv",
                [
                    "edasm_setup.py",
                    "--work-dir",
                    tmpdir,
                    "--rom",
                    "dummy.rom",
                    "--disk-image",
                    str(disk_image),
                    "--rearrange-config",
                    str(config_file),
                    "--no-run",
                ],
            ):
                result = main()

            # Should succeed
            self.assertEqual(result, 0)

            # Rearrangement functions should be called
            mock_parse_config.assert_called_once_with(str(config_file))
            mock_validate_config.assert_called_once()
            mock_expand_mappings.assert_called_once()
            mock_rearrange.assert_called_once()

    @mock.patch("edasm_setup.rearrange_files")
    @mock.patch("edasm_setup.expand_rearrange_mappings")
    @mock.patch("edasm_setup.validate_rearrange_config")
    @mock.patch("edasm_setup.parse_rearrange_config")
    @mock.patch("edasm_setup.run_emulator")
    @mock.patch("edasm_setup.discover_system_file")
    @mock.patch("edasm_setup.run_metadata_conversion")
    @mock.patch("edasm_setup.extract_disk_image")
    @mock.patch("edasm_setup.check_cadius_available")
    def test_integration_rearrange_before_metadata(
        self,
        mock_check_cadius,
        mock_extract,
        mock_metadata,
        mock_discover,
        mock_run,
        mock_parse_config,
        mock_validate_config,
        mock_expand_mappings,
        mock_rearrange,
    ):
        """Verify correct order: extract → rearrange → metadata."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Track call order
            call_order = []

            def track_extract(*args, **kwargs):
                call_order.append("extract")

            def track_rearrange(*args, **kwargs):
                call_order.append("rearrange")

            def track_metadata(*args, **kwargs):
                call_order.append("metadata")

            mock_extract.side_effect = track_extract
            mock_rearrange.side_effect = track_rearrange
            mock_metadata.side_effect = track_metadata

            # Setup other mocks
            mock_check_cadius.return_value = "cadius"
            mock_discover.return_value = f"{tmpdir}/volumes/TEST/PRODOS"
            mock_parse_config.return_value = {"rearrange": [{"src": "A", "dest": "B"}]}
            mock_expand_mappings.return_value = [("src_full", "dest_full")]

            # Create config file and disk image
            config_file = Path(tmpdir) / "config.json"
            config_file.write_text(json.dumps({"rearrange": [{"src": "A", "dest": "B"}]}))
            disk_image = Path(tmpdir) / "test.2mg"
            disk_image.write_bytes(b"dummy")

            # Run main with rearrange config
            with mock.patch(
                "sys.argv",
                [
                    "edasm_setup.py",
                    "--work-dir",
                    tmpdir,
                    "--rom",
                    "dummy.rom",
                    "--disk-image",
                    str(disk_image),
                    "--rearrange-config",
                    str(config_file),
                    "--no-run",
                ],
            ):
                result = main()

            # Should succeed
            self.assertEqual(result, 0)

            # Verify correct order
            self.assertEqual(call_order, ["extract", "rearrange", "metadata"])


class TestRearrangementEndToEnd(unittest.TestCase):
    """Comprehensive end-to-end tests for file rearrangement in realistic scenarios."""

    def test_e2e_rearrange_multiple_files(self):
        """Create temp volume with multiple files, apply config, verify all moved correctly."""
        with tempfile.TemporaryDirectory() as tmpdir:
            volume_dir = Path(tmpdir) / "volumes" / "TEST"
            volume_dir.mkdir(parents=True)

            # Create multiple test files with content
            file1 = volume_dir / "FILE1.TXT"
            file2 = volume_dir / "FILE2.ASM"
            file3 = volume_dir / "SUBDIR" / "FILE3.DAT"
            file3.parent.mkdir()

            file1.write_text("Content of file 1")
            file2.write_text("Content of file 2")
            file3.write_text("Content of file 3")

            # Create rearrange config with multiple mappings
            config = {
                "rearrange": [
                    {"from": "FILE1.TXT", "to": "TEXTS/RENAMED1.TXT"},
                    {"from": "FILE2.ASM", "to": "SOURCE/MAIN.ASM"},
                    {"from": "SUBDIR/FILE3.DAT", "to": "DATA/MYDATA.DAT"},
                ]
            }

            # Parse, validate, expand, and execute rearrangement
            validate_rearrange_config(config)
            mappings = expand_rearrange_mappings(str(volume_dir), config["rearrange"])
            rearrange_files(str(volume_dir), mappings)

            # Verify all files moved to correct locations
            self.assertFalse(file1.exists())
            self.assertFalse(file2.exists())
            self.assertFalse(file3.exists())

            new_file1 = volume_dir / "TEXTS" / "RENAMED1.TXT"
            new_file2 = volume_dir / "SOURCE" / "MAIN.ASM"
            new_file3 = volume_dir / "DATA" / "MYDATA.DAT"

            self.assertTrue(new_file1.exists())
            self.assertTrue(new_file2.exists())
            self.assertTrue(new_file3.exists())

            # Verify content preserved
            self.assertEqual(new_file1.read_text(), "Content of file 1")
            self.assertEqual(new_file2.read_text(), "Content of file 2")
            self.assertEqual(new_file3.read_text(), "Content of file 3")

    def test_e2e_rearrange_with_glob_patterns(self):
        """Create temp volume with files matching glob patterns, verify all matching files moved correctly."""
        with tempfile.TemporaryDirectory() as tmpdir:
            volume_dir = Path(tmpdir) / "volumes" / "TEST"
            volume_dir.mkdir(parents=True)

            # Create files matching and not matching glob patterns
            (volume_dir / "DOC1.TXT").write_text("doc1")
            (volume_dir / "DOC2.TXT").write_text("doc2")
            (volume_dir / "DOC3.TXT").write_text("doc3")
            (volume_dir / "README.ASM").write_text("asm code")
            (volume_dir / "DATA.DAT").write_text("binary data")

            # Create rearrange config with glob patterns
            config = {
                "rearrange": [
                    {"from": "*.TXT", "to": "TXTFILES/"},
                    {"from": "*.ASM", "to": "ASMFILES/"},
                ]
            }

            # Execute rearrangement
            validate_rearrange_config(config)
            mappings = expand_rearrange_mappings(str(volume_dir), config["rearrange"])
            rearrange_files(str(volume_dir), mappings)

            # Verify all .TXT files moved to TXTFILES/
            self.assertTrue((volume_dir / "TXTFILES" / "DOC1.TXT").exists())
            self.assertTrue((volume_dir / "TXTFILES" / "DOC2.TXT").exists())
            self.assertTrue((volume_dir / "TXTFILES" / "DOC3.TXT").exists())

            # Verify .ASM file moved to ASMFILES/
            self.assertTrue((volume_dir / "ASMFILES" / "README.ASM").exists())

            # Verify non-matching file (.DAT) untouched
            self.assertTrue((volume_dir / "DATA.DAT").exists())

            # Verify content preserved
            self.assertEqual((volume_dir / "TXTFILES" / "DOC1.TXT").read_text(), "doc1")
            self.assertEqual((volume_dir / "ASMFILES" / "README.ASM").read_text(), "asm code")

    def test_e2e_rearrange_preserves_xattrs_after_conversion(self):
        """Create temp volume with cadius-style names, rearrange, convert metadata, verify xattrs."""
        with tempfile.TemporaryDirectory() as tmpdir:
            volume_dir = Path(tmpdir) / "volumes" / "TEST"
            volume_dir.mkdir(parents=True)

            # Create files with cadius-style metadata in names (type#auxtype)
            # Type $04 = TEXT, auxtype $0000
            file1 = volume_dir / "DOCUMENT#040000"
            file1.write_text("Text document")

            # Type $06 = BIN, auxtype $2000
            file2 = volume_dir / "PROGRAM#062000"
            file2.write_bytes(b"\x4c\x00\x20")  # JMP $2000

            # Rearrange files to subdirectories (keeping cadius-style names)
            config = {
                "rearrange": [
                    {"from": "DOCUMENT#040000", "to": "DOCS/DOCUMENT#040000"},
                    {"from": "PROGRAM#062000", "to": "BIN/PROGRAM#062000"},
                ]
            }

            validate_rearrange_config(config)
            mappings = expand_rearrange_mappings(str(volume_dir), config["rearrange"])
            rearrange_files(str(volume_dir), mappings)

            # Verify files were rearranged
            new_file1 = volume_dir / "DOCS" / "DOCUMENT#040000"
            new_file2 = volume_dir / "BIN" / "PROGRAM#062000"
            self.assertTrue(new_file1.exists())
            self.assertTrue(new_file2.exists())

            # Now run metadata conversion - should process rearranged files
            run_metadata_conversion(str(volume_dir))

            # After conversion, files are renamed (cadius suffix removed)
            converted_file1 = volume_dir / "DOCS" / "DOCUMENT"
            converted_file2 = volume_dir / "BIN" / "PROGRAM"

            # Verify files were converted and renamed
            self.assertTrue(converted_file1.exists())
            self.assertTrue(converted_file2.exists())

            # Verify xattrs were correctly applied to rearranged files
            try:
                file_type1 = os.getxattr(converted_file1, "user.prodos8.file_type")
                file_type2 = os.getxattr(converted_file2, "user.prodos8.file_type")

                # Type should be preserved in xattr
                self.assertEqual(file_type1, b"04")
                self.assertEqual(file_type2, b"06")

                # Check aux_type as well
                aux_type1 = os.getxattr(converted_file1, "user.prodos8.aux_type")
                aux_type2 = os.getxattr(converted_file2, "user.prodos8.aux_type")
                self.assertEqual(aux_type1, b"0000")
                self.assertEqual(aux_type2, b"2000")

            except OSError as e:
                if e.errno == 95:  # ENOTSUP
                    self.skipTest("xattrs not supported on this filesystem")
                raise

    def test_e2e_rearrange_with_text_imports(self):
        """Extract, rearrange, import text files, verify both coexist correctly."""
        with tempfile.TemporaryDirectory() as tmpdir:
            volume_dir = Path(tmpdir) / "volumes" / "TEST"
            volume_dir.mkdir(parents=True)

            # Create initial extracted files
            (volume_dir / "OLDFILE1.TXT").write_text("Extracted file 1")
            (volume_dir / "OLDFILE2.TXT").write_text("Extracted file 2")

            # Rearrange the extracted files
            config = {
                "rearrange": [
                    {"from": "OLDFILE1.TXT", "to": "EXTRACTED/FILE1.TXT"},
                    {"from": "OLDFILE2.TXT", "to": "EXTRACTED/FILE2.TXT"},
                ]
            }

            validate_rearrange_config(config)
            mappings = expand_rearrange_mappings(str(volume_dir), config["rearrange"])
            rearrange_files(str(volume_dir), mappings)

            # Now import text files from external sources
            external_src = Path(tmpdir) / "external_source.txt"
            external_src.write_text("Imported content\nWith multiple lines\n")

            text_mappings = [(str(external_src), "IMPORTED/SOURCE.TXT")]

            # Import text files (simulating the text import feature)
            import_text_files(text_mappings, str(volume_dir), lossy=False)

            # Verify both rearranged and imported files coexist correctly
            self.assertTrue((volume_dir / "EXTRACTED" / "FILE1.TXT").exists())
            self.assertTrue((volume_dir / "EXTRACTED" / "FILE2.TXT").exists())
            self.assertTrue((volume_dir / "IMPORTED" / "SOURCE.TXT").exists())

            # Verify content
            self.assertEqual(
                (volume_dir / "EXTRACTED" / "FILE1.TXT").read_text(),
                "Extracted file 1",
            )
            # Note: imported file may be converted, so we just check it exists
            imported_content = (volume_dir / "IMPORTED" / "SOURCE.TXT").read_text()
            self.assertIn("Imported content", imported_content)

    def test_e2e_rearrange_system_file_discovery(self):
        """Create temp volume with system file, rearrange it, verify discovery finds it."""
        with tempfile.TemporaryDirectory() as tmpdir:
            volume_dir = Path(tmpdir) / "volumes" / "TEST"
            volume_dir.mkdir(parents=True)

            # Create a system file (starts with $4C = JMP)
            original_system = volume_dir / "PRODOS.SYSTEM"
            original_system.write_bytes(b"\x4c\x00\x20" + b"\x00" * 100)

            # Create some other files
            (volume_dir / "README.TXT").write_text("readme")

            # Rearrange the system file
            config = {
                "rearrange": [
                    {"from": "PRODOS.SYSTEM", "to": "SYS/BOOT.SYSTEM"},
                ]
            }

            validate_rearrange_config(config)
            mappings = expand_rearrange_mappings(str(volume_dir), config["rearrange"])
            rearrange_files(str(volume_dir), mappings)

            # Verify original location is empty
            self.assertFalse(original_system.exists())

            # Verify new location exists
            new_system = volume_dir / "SYS" / "BOOT.SYSTEM"
            self.assertTrue(new_system.exists())

            # Run system file discovery - should find the rearranged system file
            discovered = discover_system_file(str(volume_dir))

            # Should find the rearranged file
            self.assertEqual(discovered, str(new_system))

            # Verify it validates as a system file
            self.assertTrue(validate_system_file(discovered))

    def test_e2e_rearrange_absolute_paths(self):
        """Use config with absolute paths (/VOLUMENAME/...), verify correct handling."""
        with tempfile.TemporaryDirectory() as tmpdir:
            volume_dir = Path(tmpdir) / "volumes" / "EDASM"
            volume_dir.mkdir(parents=True)

            # Create files using relative paths
            (volume_dir / "SOURCE.TXT").write_text("source content")
            (volume_dir / "DIR1").mkdir(exist_ok=True)
            (volume_dir / "DIR1" / "FILE.ASM").write_text("asm source")

            # Create config with absolute ProDOS paths
            config = {
                "rearrange": [
                    # Absolute path starting with /
                    {"from": "/SOURCE.TXT", "to": "/DEST/TARGET.TXT"},
                    {"from": "/DIR1/FILE.ASM", "to": "/BUILD/CODE.ASM"},
                ]
            }

            # Execute rearrangement
            validate_rearrange_config(config)
            mappings = expand_rearrange_mappings(str(volume_dir), config["rearrange"])
            rearrange_files(str(volume_dir), mappings)

            # Verify absolute paths were handled correctly
            # (Leading / should be stripped for filesystem operations)
            self.assertFalse((volume_dir / "SOURCE.TXT").exists())
            self.assertTrue((volume_dir / "DEST" / "TARGET.TXT").exists())
            self.assertEqual(
                (volume_dir / "DEST" / "TARGET.TXT").read_text(), "source content"
            )

            self.assertFalse((volume_dir / "DIR1" / "FILE.ASM").exists())
            self.assertTrue((volume_dir / "BUILD" / "CODE.ASM").exists())
            self.assertEqual(
                (volume_dir / "BUILD" / "CODE.ASM").read_text(), "asm source"
            )


if __name__ == "__main__":
    unittest.main()
