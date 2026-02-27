#!/usr/bin/env python3
"""Convert Linux text files to ProDOS TEXT format.

Phase 1: Core byte conversion functions for line endings and ASCII.
Phase 2: File operations and xattr metadata.
Phase 3: Command-line interface.
"""

import argparse
import os
import sys


def normalize_line_endings(data: bytes) -> bytes:
    """Normalize line endings to ProDOS CR (\\r).
    
    Converts:
    - CRLF (\\r\\n) -> CR (\\r)
    - LF (\\n) -> CR (\\r)
    - Preserves standalone CR (\\r)
    
    Args:
        data: Input bytes to normalize
        
    Returns:
        Bytes with normalized line endings
    """
    # Replace CRLF with CR first to avoid double conversion
    data = data.replace(b'\r\n', b'\r')
    # Then replace remaining LF with CR
    data = data.replace(b'\n', b'\r')
    return data


def convert_to_ascii(data: bytes, *, strict: bool) -> bytes:
    """Convert data to ASCII, handling high-bit characters.
    
    Args:
        data: Input bytes
        strict: If True, raise ValueError on non-ASCII (>= 0x80).
                If False, replace non-ASCII bytes with ord('?').
                
    Returns:
        ASCII-only bytes
        
    Raises:
        ValueError: If strict=True and non-ASCII bytes found
    """
    # Check for non-ASCII bytes
    non_ascii_found = any(byte >= 0x80 for byte in data)
    
    if non_ascii_found and strict:
        raise ValueError("Input contains non-ASCII bytes (>= 0x80)")
    
    if non_ascii_found:
        # Replace all bytes >= 0x80 with '?'
        result = bytearray()
        for byte in data:
            if byte >= 0x80:
                result.append(ord('?'))
            else:
                result.append(byte)
        return bytes(result)
    
    return data


def set_prodos_text_metadata(path: str, *, access: str = "dn-..-wr") -> None:
    """Set ProDOS TEXT metadata xattrs on a file.
    
    Sets the following xattrs:
    - user.prodos8.file_type = b"04" (TEXT file)
    - user.prodos8.aux_type = b"0000" (sequential TEXT)
    - user.prodos8.storage_type = b"01" (seedling file)
    - user.prodos8.access = access encoded as ASCII bytes
    
    Args:
        path: Path to the file to set metadata on
        access: ProDOS access string (default: "dn-..-wr")
        
    Raises:
        OSError: If xattr operations fail (e.g., file not found, xattrs unsupported)
    """
    os.setxattr(path, "user.prodos8.file_type", b"04")
    os.setxattr(path, "user.prodos8.aux_type", b"0000")
    os.setxattr(path, "user.prodos8.storage_type", b"01")
    os.setxattr(path, "user.prodos8.access", access.encode("ascii"))


def convert_file_in_place(path: str, *, strict_ascii: bool = True, access: str = "dn-..-wr") -> None:
    """Convert a file to ProDOS TEXT format in-place.
    
    Atomically reads the file, applies line ending normalization and ASCII conversion,
    writes the result to a temp file, sets xattrs, and replaces the original.
    If any step fails, the original file remains unchanged.
    
    Args:
        path: Path to the file to convert
        strict_ascii: If True, raise ValueError on non-ASCII bytes.
                     If False, replace non-ASCII bytes with '?'.
        access: ProDOS access string for xattr metadata (default: "dn-..-wr")
        
    Raises:
        ValueError: If strict_ascii=True and non-ASCII bytes are found
        OSError: If file operations or xattr operations fail
    """
    import tempfile
    import stat
    
    # Read the original file
    with open(path, 'rb') as f:
        data = f.read()
    
    # Get original file permissions
    original_mode = os.stat(path).st_mode
    
    # Apply conversions (this may raise ValueError in strict mode)
    data = normalize_line_endings(data)
    data = convert_to_ascii(data, strict=strict_ascii)
    
    # Create temp file in the same directory for atomic replacement
    dir_path = os.path.dirname(os.path.abspath(path))
    temp_fd = None
    temp_path = None
    
    try:
        # Create temporary file in same directory (delete=False to control cleanup)
        temp_fd, temp_path = tempfile.mkstemp(dir=dir_path, prefix=".prodos_tmp_")
        
        # Write converted data to temp file (use fdopen to ensure complete write)
        with os.fdopen(temp_fd, 'wb') as f:
            f.write(data)
        temp_fd = None
        
        # Copy permissions from original to temp
        os.chmod(temp_path, stat.S_IMODE(original_mode))
        
        # Set ProDOS metadata on temp file
        set_prodos_text_metadata(temp_path, access=access)
        
        # Atomically replace original with temp
        os.replace(temp_path, path)
        temp_path = None  # Successfully moved, don't clean up
        
    except Exception:
        # Clean up temp file on any error
        if temp_fd is not None:
            try:
                os.close(temp_fd)
            except Exception:
                pass
        if temp_path is not None and os.path.exists(temp_path):
            try:
                os.unlink(temp_path)
            except Exception:
                pass
        raise


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Parse command-line arguments.
    
    Args:
        argv: Command-line arguments (defaults to sys.argv[1:] if None)
        
    Returns:
        Parsed arguments namespace
    """
    parser = argparse.ArgumentParser(
        description="Convert Linux text files to ProDOS TEXT format with CR line endings and xattrs."
    )
    parser.add_argument(
        "path",
        help="Path to the text file to convert"
    )
    parser.add_argument(
        "--lossy",
        action="store_true",
        help="Allow non-ASCII characters by replacing them with '?' (default: strict mode rejects non-ASCII)"
    )
    parser.add_argument(
        "--access",
        default="dn-..-wr",
        help="ProDOS access string for xattr metadata (default: dn-..-wr)"
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    """Main entry point for the CLI.
    
    Args:
        argv: Command-line arguments (defaults to sys.argv[1:] if None)
        
    Returns:
        Exit code (0 for success, non-zero for failure)
    """
    try:
        args = parse_args(argv)
    except SystemExit as e:
        # argparse calls sys.exit() for --help or parse errors
        # Return the exit code (ensure it's an int)
        if isinstance(e.code, int):
            return e.code
        return 2 if e.code is None else 1
    
    try:
        convert_file_in_place(
            args.path,
            strict_ascii=not args.lossy,
            access=args.access
        )
        return 0
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except OSError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
