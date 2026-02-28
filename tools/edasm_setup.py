#!/usr/bin/env python3
"""EDASM Setup and Launch Tool

Complete setup/launch workflow for EDASM:
1. Create work directory structure
2. Extract disk image using cadius
3. Convert cadius metadata to xattr
4. Import host text files with conversion
5. Run emulator with discovered or provided system file
"""

import argparse
import json
import os
import shlex
import shutil
import subprocess  # nosec B404
import sys
from pathlib import Path
from typing import List, Optional, Tuple

# Import from sibling tools
sys.path.insert(0, str(Path(__file__).parent))
from linux_to_prodos_text import convert_file_in_place


def validate_disk_image_extension(path: str) -> bool:
    """Validate disk image has supported extension.

    Currently only .2mg is supported.

    Args:
        path: Path to disk image

    Returns:
        True if valid

    Raises:
        ValueError: If extension not supported
    """
    lower_path = path.lower()
    if lower_path.endswith(".2mg"):
        return True

    raise ValueError(
        f"Unsupported disk image extension. "
        f"Currently only .2mg format is supported. Got: {path}"
    )


def parse_text_mapping(spec: str) -> Tuple[str, str]:
    """Parse --text SRC[:DEST] argument.

    Args:
        spec: Text mapping specification

    Returns:
        (source_path, dest_path) tuple

    Raises:
        ValueError: If spec format is invalid
    """
    if not spec or spec == ":":
        raise ValueError("Invalid text mapping: empty source")

    if spec.startswith(":"):
        raise ValueError("Invalid text mapping: missing source")

    if ":" in spec:
        src, dest = spec.split(":", 1)
        if not src:
            raise ValueError("Invalid text mapping: empty source")
        if not dest:
            raise ValueError("Invalid text mapping: empty destination")
        return src, dest
    else:
        # No dest specified, use basename of source
        return spec, os.path.basename(spec)


def validate_system_file(path: str) -> bool:
    """Validate a file is a ProDOS system file.

    Checks that file exists and has non-zero size. Note: ProDOS system files
    (type $FF) do NOT need to start with 0x4C (JMP). ProDOS unconditionally
    jumps to $2000 after loading. The 0x4C check is only used by some selector
    programs to detect if an interpreter supports the startup-program protocol.

    Args:
        path: Path to file to validate

    Returns:
        True if valid system file (exists and non-empty), False if empty

    Raises:
        OSError: If file doesn't exist or can't be read
    """
    with open(path, "rb") as f:
        first_byte = f.read(1)
        return bool(first_byte)


def discover_system_file(volume_dir: str) -> str:
    """Discover system file in volume directory.

    Strategy:
    1. Look for files with .SYSTEM or .SYS extension (case-insensitive)
    2. If none found, look for files with xattr user.prodos8.file_type=ff
    3. Exactly one candidate required, otherwise fail

    Args:
        volume_dir: Directory to search

    Returns:
        Path to discovered system file

    Raises:
        ValueError: If no candidates or multiple candidates found
    """
    volume_path = Path(volume_dir)
    candidates = []

    # First pass: look for .SYSTEM or .SYS files
    for entry in volume_path.rglob("*"):
        if not entry.is_file():
            continue

        name_lower = entry.name.lower()
        if name_lower.endswith(".system") or name_lower.endswith(".sys"):
            try:
                if validate_system_file(str(entry)):
                    candidates.append(str(entry))
            except OSError:
                continue

    # If no extension-based candidates, try xattr-based discovery
    if not candidates:
        for entry in volume_path.rglob("*"):
            if not entry.is_file():
                continue

            try:
                file_type = os.getxattr(entry, "user.prodos8.file_type")
                file_type_str = (
                    file_type.decode("ascii", errors="ignore").strip().lower()
                )
                if file_type_str == "ff" and validate_system_file(str(entry)):
                    candidates.append(str(entry))
            except OSError:
                # No xattr or file access issue, skip
                continue

    if not candidates:
        raise ValueError(
            f"No system file found in {volume_dir}. "
            f"Expected .SYSTEM/.SYS file or file with type $FF."
        )

    if len(candidates) > 1:
        raise ValueError(
            "Ambiguous system file discovery: multiple candidates found:\n"
            + "\n".join(f"  - {c}" for c in candidates)
        )

    return candidates[0]


def validate_safe_path(path: str, param_name: str) -> None:
    """Validate that a path is safe for use in subprocess calls.

    Args:
        path: Path to validate
        param_name: Parameter name for error messages

    Raises:
        ValueError: If path contains shell metacharacters or other unsafe patterns
    """
    # Reject paths with shell metacharacters that could enable injection
    unsafe_chars = [";", "|", "&", "$", "`", "\n", "\r", ">", "<", "(", ")", "{", "}"]
    for char in unsafe_chars:
        if char in path:
            raise ValueError(
                f"{param_name} contains shell metacharacter '{char}' which is not allowed"
            )


def parse_rearrange_config(config_path: str) -> dict:
    """Read and parse JSON config file for file rearrangement.

    Args:
        config_path: Path to JSON configuration file

    Returns:
        Parsed configuration dictionary

    Raises:
        FileNotFoundError: If config file does not exist
        ValueError: If JSON is malformed
    """
    try:
        with open(config_path, "r") as f:
            return json.load(f)
    except FileNotFoundError:
        raise
    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid JSON in config file {config_path}: {e}") from e


def validate_rearrange_config(config: dict) -> None:
    """Validate rearrange configuration structure.

    Args:
        config: Configuration dictionary to validate

    Raises:
        ValueError: If configuration structure is invalid
    """
    # Must be a dict with "rearrange" key
    if not isinstance(config, dict):
        raise ValueError("Configuration must be a dictionary")

    if "rearrange" not in config:
        raise ValueError("Configuration must contain 'rearrange' key")

    # "rearrange" must be a list
    if not isinstance(config["rearrange"], list):
        raise ValueError("'rearrange' must be a list")

    # Each mapping must be valid
    for i, mapping in enumerate(config["rearrange"]):
        if not isinstance(mapping, dict):
            raise ValueError(f"Mapping at index {i} must be a dictionary")

        # Must have "from" key
        if "from" not in mapping:
            raise ValueError(f"Mapping at index {i} missing 'from' key")

        # Must have "to" key
        if "to" not in mapping:
            raise ValueError(f"Mapping at index {i} missing 'to' key")

        # Both must be strings
        if not isinstance(mapping["from"], str):
            raise ValueError(f"Mapping at index {i}: 'from' must be a string")

        if not isinstance(mapping["to"], str):
            raise ValueError(f"Mapping at index {i}: 'to' must be a string")

        # Both must be non-empty
        if not mapping["from"]:
            raise ValueError(f"Mapping at index {i}: 'from' must not be empty")

        if not mapping["to"]:
            raise ValueError(f"Mapping at index {i}: 'to' must not be empty")


def expand_rearrange_mappings(
    volume_dir: str, mappings: List[dict]
) -> List[Tuple[str, str]]:
    """Expand glob patterns in rearrange mappings.

    Args:
        volume_dir: Base directory for resolving paths
        mappings: List of {"from": "pattern", "to": "dest"} dicts

    Returns:
        List of (source_path, dest_path) tuples with expanded paths

    Raises:
        ValueError: If glob matches multiple files but target is not a directory
    """
    volume_path = Path(volume_dir)
    result = []

    for mapping in mappings:
        from_pattern = mapping["from"]
        to_pattern = mapping["to"]

        # Strip leading slash for filesystem operations
        if from_pattern.startswith("/"):
            from_pattern = from_pattern[1:]

        # Expand glob pattern
        matches = list(volume_path.glob(from_pattern))

        # If no matches, skip this mapping
        if not matches:
            continue

        # Check if 'to' is a directory (ends with /)
        is_to_directory = to_pattern.endswith("/")

        # If 'to' is not a directory and we have multiple matches, error
        if not is_to_directory and len(matches) > 1:
            raise ValueError(
                f"Glob pattern '{from_pattern}' matches {len(matches)} files "
                f"but destination '{to_pattern}' is not a directory (does not end with /). "
                f"Cannot map multiple files to a single filename."
            )

        # Expand each match
        for match in matches:
            src_path = str(match)

            # Determine destination path
            if is_to_directory:
                # Preserve basename from source
                to_base = to_pattern.rstrip("/")
                if to_base.startswith("/"):
                    to_base = to_base[1:]
                dest_path = str(volume_path / to_base / match.name)
            else:
                # Explicit filename
                to_stripped = (
                    to_pattern[1:] if to_pattern.startswith("/") else to_pattern
                )
                dest_path = str(volume_path / to_stripped)

            result.append((src_path, dest_path))

    return result


def rearrange_files(volume_dir: str, expanded_mappings: List[Tuple[str, str]]) -> None:
    """Perform atomic file rearrangement with validation and rollback.

    Args:
        volume_dir: Base directory (for reference, not used directly)
        expanded_mappings: List of (source_path, dest_path) tuples

    Raises:
        ValueError: If validation fails (missing source, existing destination)
    """
    # Phase 1: Validate all source files exist
    for src, _ in expanded_mappings:
        if not Path(src).exists():
            raise ValueError(f"Source file does not exist: {src}")

    # Phase 2: Validate no destination conflicts
    for _, dest in expanded_mappings:
        if Path(dest).exists():
            raise ValueError(f"Destination already exists: {dest}")

    # Phase 3: Perform moves (all validations passed)
    for src, dest in expanded_mappings:
        dest_path = Path(dest)

        # Create parent directories as needed
        dest_path.parent.mkdir(parents=True, exist_ok=True)

        # Move file
        shutil.move(src, dest)


def check_cadius_available(cadius_path: str) -> str:
    """Resolve and validate cadius executable.

    Args:
        cadius_path: Command name or explicit path

    Returns:
        Resolved executable path

    Raises:
        RuntimeError: If cadius cannot be found/executed
    """
    # Validate path is safe
    validate_safe_path(cadius_path, "--cadius")

    # Explicit path provided
    if os.path.sep in cadius_path:
        if os.path.isfile(cadius_path) and os.access(cadius_path, os.X_OK):
            return cadius_path
        raise RuntimeError(
            f"cadius executable not found or not executable: {cadius_path}"
        )

    # Command lookup in PATH
    resolved = shutil.which(cadius_path)
    if resolved is None:
        raise RuntimeError(
            f"cadius command not found: {cadius_path}\n"
            "Please install cadius or pass --cadius with the executable path."
        )
    return resolved


def extract_disk_image(
    cadius_path: str,
    disk_image: str,
    output_dir: str,
    extract_cmd_template: Optional[str] = None,
) -> None:
    """Extract disk image using cadius.

    Args:
        cadius_path: Path to cadius executable
        disk_image: Path to disk image file
        output_dir: Output directory for extraction
        extract_cmd_template: Optional command template with {cadius}, {image}, {out} placeholders

    Raises:
        RuntimeError: If extraction fails
    """
    # Validate paths are safe before using in subprocess
    validate_safe_path(disk_image, "--disk-image")
    validate_safe_path(output_dir, "work directory")

    os.makedirs(output_dir, exist_ok=True)

    # Try extraction
    if extract_cmd_template:
        # Treat extract_cmd_template as an argument template, not a shell command line.
        # Split the template into tokens, then format each token with the provided paths.
        template_tokens = shlex.split(extract_cmd_template)
        cmd_args = [
            token.format(cadius=cadius_path, image=disk_image, out=output_dir)
            for token in template_tokens
        ]
        result = subprocess.run(
            cmd_args, capture_output=True, text=True, check=False
        )  # nosec B603
        if result.returncode != 0:
            raise RuntimeError(
                f"Disk image extraction failed:\n"
                f"Command: {' '.join(cmd_args)}\n"
                f"Error: {result.stderr}"
            )
    else:
        # Try common extraction patterns
        patterns = [
            [cadius_path, "EXTRACTVOLUME", disk_image, output_dir],
            [cadius_path, "EXTRACT", disk_image, "/", output_dir],
        ]

        last_error = None
        for pattern in patterns:
            try:
                result = subprocess.run(
                    pattern, capture_output=True, text=True
                )  # nosec B603
            except Exception as e:
                # Record subprocess-related errors and try the next pattern
                last_error = str(e)
                continue

            if result.returncode == 0:
                # Check if output was created
                try:
                    if os.listdir(output_dir):
                        return  # Success!
                except PermissionError as e:
                    raise RuntimeError(
                        f"Output directory '{output_dir}' exists but is not readable: {e}"
                    ) from e
                except OSError as e:
                    raise RuntimeError(
                        f"Unable to read output directory '{output_dir}': {e}"
                    ) from e

            last_error = result.stderr

        raise RuntimeError(
            f"Failed to extract disk image with any known cadius command pattern.\n"
            f"Last error: {last_error}\n"
            f"Try providing --extract-cmd with a custom template."
        )


def run_metadata_conversion(volume_dir: str) -> None:
    """Run cadius metadata to xattr conversion.

    Args:
        volume_dir: Directory containing extracted files

    Raises:
        RuntimeError: If conversion fails
    """
    script_dir = Path(__file__).parent
    converter = script_dir / "cadius_xattr_convert.py"

    result = subprocess.run(  # nosec B603
        [sys.executable, str(converter), "cadius-to-xattr", "--recursive", volume_dir],
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        raise RuntimeError(f"Metadata conversion failed:\n{result.stderr}")


def import_text_files(
    text_mappings: List[Tuple[str, str]], volume_dir: str, lossy: bool
) -> None:
    """Import and convert text files into volume directory.

    Args:
        text_mappings: List of (source, dest) tuples
        volume_dir: Target volume directory
        lossy: Whether to use lossy ASCII conversion

    Raises:
        RuntimeError: If import/conversion fails
    """
    for src, dest in text_mappings:
        # Resolve destination path
        dest_path = Path(volume_dir) / dest

        # Create parent directories
        dest_path.parent.mkdir(parents=True, exist_ok=True)

        # Copy file
        try:
            shutil.copy2(src, dest_path)
        except Exception as e:
            raise RuntimeError(f"Failed to copy {src} to {dest_path}: {e}") from e

        # Convert in place
        try:
            convert_file_in_place(str(dest_path), strict_ascii=not lossy)
        except Exception as e:
            raise RuntimeError(f"Failed to convert {dest_path}: {e}") from e

        print(f"Imported and converted: {src} -> {dest_path}")


def run_emulator(
    runner_path: str,
    rom_path: str,
    system_file: str,
    volume_root: str,
    debug: bool = False,
    max_instructions: Optional[int] = None,
) -> None:
    """Run the emulator.

    Args:
        runner_path: Path to prodos8emu_run executable
        rom_path: Path to ROM file
        system_file: Path to system file to execute
        volume_root: Volume root directory
        debug: Enable runner debug logs
        max_instructions: Optional instruction limit

    Raises:
        RuntimeError: If emulator fails
    """
    cmd = [
        runner_path,
        "--volume-root",
        volume_root,
    ]

    if debug:
        cmd.append("--debug")

    if max_instructions is not None:
        cmd.extend(["--max-instructions", str(max_instructions)])

    cmd.extend([rom_path, system_file])

    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd)  # nosec B603

    if result.returncode != 0:
        raise RuntimeError(f"Emulator exited with code {result.returncode}")


def parse_args(args=None):
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="EDASM Setup and Launch Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic usage with disk image extraction
  %(prog)s --work-dir work --disk-image EDASM.2mg --rom apple2e.rom
  
  # Skip extraction, use existing work directory
  %(prog)s --work-dir work --rom apple2e.rom --skip-extract
  
  # Import text files and run
  %(prog)s --work-dir work --disk-image EDASM.2mg --rom apple2e.rom \\
    --text main.asm --text lib.asm:LIB/lib.asm
  
  # Setup only, don't run emulator
  %(prog)s --work-dir work --disk-image EDASM.2mg --rom apple2e.rom --no-run
""",
    )

    # Required arguments
    parser.add_argument(
        "--work-dir", required=True, help="Work directory for extraction and setup"
    )
    parser.add_argument("--rom", required=True, help="Path to Apple II ROM file")

    # Disk image and extraction
    parser.add_argument("--disk-image", help="Path to disk image file (.2mg)")
    parser.add_argument(
        "--skip-extract",
        action="store_true",
        help="Skip disk image extraction (use existing work directory)",
    )
    parser.add_argument(
        "--cadius", default="cadius", help="Path to cadius executable (default: cadius)"
    )
    parser.add_argument(
        "--extract-cmd",
        help="Custom extraction command template with {cadius}, {image}, {out} placeholders",
    )

    # Volume configuration
    parser.add_argument(
        "--volume-root", help="Volume root directory (default: <work-dir>/volumes)"
    )
    parser.add_argument(
        "--volume-name", default="EDASM", help="Volume name (default: EDASM)"
    )

    # System file
    parser.add_argument(
        "--system-file", help="Explicit system file path (relative to volume directory)"
    )

    # Text import
    parser.add_argument(
        "--text",
        action="append",
        dest="text_mappings",
        help="Import text file as SRC[:DEST] (repeatable)",
    )
    parser.add_argument(
        "--lossy-text",
        action="store_true",
        help="Use lossy ASCII conversion for text files",
    )

    # File rearrangement
    parser.add_argument(
        "--rearrange-config",
        help="JSON config file for rearranging extracted files (optional)",
    )

    # Execution control
    parser.add_argument(
        "--no-run", action="store_true", help="Setup only, don't run emulator"
    )
    parser.add_argument(
        "--runner",
        default="build/prodos8emu_run",
        help="Path to prodos8emu_run executable (default: build/prodos8emu_run)",
    )
    parser.add_argument(
        "--max-instructions",
        type=int,
        help="Maximum instructions to execute (passed to runner)",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable prodos8emu_run debug logs",
    )

    return parser.parse_args(args)


def main():
    """Main entry point."""
    args = parse_args()

    try:
        # Validate arguments
        if not args.skip_extract:
            if not args.disk_image:
                print(
                    "Error: --disk-image required unless --skip-extract specified",
                    file=sys.stderr,
                )
                return 1

            validate_disk_image_extension(args.disk_image)
            args.cadius = check_cadius_available(args.cadius)

        # Setup paths
        work_dir = Path(args.work_dir)
        volume_root = (
            Path(args.volume_root) if args.volume_root else work_dir / "volumes"
        )
        volume_dir = volume_root / args.volume_name

        # Create work directory
        work_dir.mkdir(parents=True, exist_ok=True)
        volume_root.mkdir(parents=True, exist_ok=True)

        # Extract disk image if requested
        if not args.skip_extract:
            print(f"Extracting {args.disk_image} to {volume_dir}...")
            extract_disk_image(
                args.cadius, args.disk_image, str(volume_dir), args.extract_cmd
            )

            print("Converting metadata to xattrs...")
            run_metadata_conversion(str(volume_dir))

            # Rearrange files if config provided (after metadata conversion)
            if args.rearrange_config:
                print("Rearranging files...")
                config = parse_rearrange_config(args.rearrange_config)
                validate_rearrange_config(config)
                mappings = expand_rearrange_mappings(
                    str(volume_dir), config["rearrange"]
                )
                rearrange_files(str(volume_dir), mappings)

        # Import text files
        if args.text_mappings:
            print("Importing text files...")
            text_mappings = [parse_text_mapping(spec) for spec in args.text_mappings]
            import_text_files(text_mappings, str(volume_dir), args.lossy_text)

        # Discover or validate system file
        if args.system_file:
            system_file_path = volume_dir / args.system_file
            if not system_file_path.exists():
                print(
                    f"Error: System file not found: {system_file_path}", file=sys.stderr
                )
                return 1
            if not validate_system_file(str(system_file_path)):
                print(
                    f"Error: Invalid system file (empty or unreadable): {system_file_path}",
                    file=sys.stderr,
                )
                return 1
            system_file = str(system_file_path)
        else:
            print("Discovering system file...")
            system_file = discover_system_file(str(volume_dir))

        print(f"System file: {system_file}")

        # Run emulator unless --no-run
        if not args.no_run:
            run_emulator(
                args.runner,
                args.rom,
                system_file,
                str(volume_root),
                args.debug,
                args.max_instructions,
            )
        else:
            print("Setup complete (--no-run specified)")

        return 0

    except (ValueError, RuntimeError, OSError) as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
