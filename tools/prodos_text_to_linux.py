#!/usr/bin/env python3
"""Convert ProDOS TEXT files to Linux text format.

Phase 1: Core byte conversion functions for line endings.
Phase 2: File operations and xattr metadata cleanup.
Phase 3: Command-line interface.
"""

import argparse
import os
import sys


def normalize_line_endings(data: bytes) -> bytes:
    r"""Normalize ProDOS CR (\\r) line endings to Linux LF (\\n).

    Converts:
    - CR (\\r) not followed by LF -> LF (\\n)
    - CRLF (\\r\\n) -> LF (\\n)   (defensive; ProDOS files use plain CR)
    - Standalone LF (\\n) -> LF (\\n)  (preserved as-is)

    Args:
        data: Input bytes to normalize

    Returns:
        Bytes with LF line endings
    """
    # Normalise CRLF first to avoid double conversion
    data = data.replace(b"\r\n", b"\n")
    # Then replace remaining CR with LF
    data = data.replace(b"\r", b"\n")
    return data


def clear_prodos_text_metadata(path: str) -> None:
    """Remove ProDOS TEXT metadata xattrs from a file.

    Removes the following xattrs if present (ignores missing xattrs silently):
    - user.prodos8.file_type
    - user.prodos8.aux_type
    - user.prodos8.storage_type
    - user.prodos8.access

    Args:
        path: Path to the file to clear metadata from

    Raises:
        OSError: If xattr operations fail for a reason other than the xattr
                 not being present (ENODATA / ENOATTR).
    """
    import errno

    _ENODATA = getattr(errno, "ENODATA", 61)  # ENODATA = 61 on Linux
    _ENOATTR = getattr(errno, "ENOATTR", _ENODATA)

    for name in (
        "user.prodos8.file_type",
        "user.prodos8.aux_type",
        "user.prodos8.storage_type",
        "user.prodos8.access",
    ):
        try:
            os.removexattr(path, name)
        except OSError as e:
            if e.errno in (_ENODATA, _ENOATTR):
                pass  # xattr was never set – that's fine
            else:
                raise


def convert_file_in_place(path: str, *, clear_metadata: bool = True) -> None:
    """Convert a ProDOS TEXT file to Linux format in-place.

    Atomically reads the file, converts CR line endings to LF, writes the
    result to a temp file, optionally removes ProDOS xattrs, and replaces
    the original.  If any step fails, the original file remains unchanged.

    Args:
        path: Path to the file to convert
        clear_metadata: If True, remove ProDOS xattrs after conversion
                        (default: True).

    Raises:
        OSError: If file operations or xattr operations fail
    """
    import stat
    import tempfile

    with open(path, "rb") as f:
        data = f.read()

    original_mode = os.stat(path).st_mode

    # If keeping metadata, snapshot the existing ProDOS xattrs before we
    # replace the file (os.replace does not carry xattrs from the original).
    saved_xattrs: dict[str, bytes] = {}
    if not clear_metadata:
        for name in (
            "user.prodos8.file_type",
            "user.prodos8.aux_type",
            "user.prodos8.storage_type",
            "user.prodos8.access",
        ):
            try:
                saved_xattrs[name] = os.getxattr(path, name)
            except OSError:
                pass  # xattr not present – skip silently

    data = normalize_line_endings(data)

    dir_path = os.path.dirname(os.path.abspath(path))
    temp_fd = None
    temp_path = None

    try:
        temp_fd, temp_path = tempfile.mkstemp(dir=dir_path, prefix=".linux_tmp_")

        with os.fdopen(temp_fd, "wb") as f:
            f.write(data)
        temp_fd = None

        os.chmod(temp_path, stat.S_IMODE(original_mode))

        # Restore any saved xattrs to the temp file before replacing
        for name, value in saved_xattrs.items():
            os.setxattr(temp_path, name, value)

        os.replace(temp_path, path)
        temp_path = None

    except Exception:
        if temp_fd is not None:
            try:
                os.close(temp_fd)
            except OSError:
                pass
        if temp_path is not None and os.path.exists(temp_path):
            try:
                os.unlink(temp_path)
            except OSError:
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
        description="Convert ProDOS TEXT files to Linux format with LF line endings."
    )
    parser.add_argument("path", help="Path to the ProDOS TEXT file to convert")
    parser.add_argument(
        "--keep-metadata",
        action="store_true",
        help="Keep ProDOS xattr metadata instead of removing it (default: remove xattrs)",
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
        if isinstance(e.code, int):
            return e.code
        return 2 if e.code is None else 1

    try:
        convert_file_in_place(args.path, clear_metadata=not args.keep_metadata)
        return 0
    except OSError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
