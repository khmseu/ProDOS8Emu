#!/usr/bin/env python3

"""Convert between Cadius filename metadata and ProDOS8Emu xattr metadata.

This repo stores ProDOS metadata in extended attributes under the user.prodos8.*
namespace. The relevant keys for type metadata are:
  - user.prodos8.file_type : 2 lowercase hex chars (e.g. "06")
  - user.prodos8.aux_type  : 4 lowercase hex chars (e.g. "2000")

Cadius commonly encodes these two values in host filenames as a suffix:
  NAME#TTAAAA
where TT is file_type (hex byte) and AAAA is aux_type (hex word).
"""

from __future__ import annotations

import argparse
import errno
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, Optional

XATTR_PREFIX = "user.prodos8."
XATTR_FILE_TYPE = XATTR_PREFIX + "file_type"
XATTR_AUX_TYPE = XATTR_PREFIX + "aux_type"


CADIUS_SUFFIX_RE = re.compile(
    r"^(?P<stem>.*)#(?P<ft>[0-9A-Fa-f]{2})(?P<aux>[0-9A-Fa-f]{4})$"
)


@dataclass(frozen=True)
class ParsedCadiusName:
    stem: str
    file_type: int
    aux_type: int


def eprint(*args: object) -> None:
    print(*args, file=sys.stderr)


def parse_cadius_suffix(name: str) -> Optional[ParsedCadiusName]:
    m = CADIUS_SUFFIX_RE.match(name)
    if not m:
        return None
    stem = m.group("stem")
    file_type = int(m.group("ft"), 16)
    aux_type = int(m.group("aux"), 16)
    return ParsedCadiusName(stem=stem, file_type=file_type, aux_type=aux_type)


def format_hex_byte(value: int) -> str:
    if not (0 <= value <= 0xFF):
        raise ValueError(f"byte out of range: {value}")
    return f"{value:02x}"


def format_hex_word(value: int) -> str:
    if not (0 <= value <= 0xFFFF):
        raise ValueError(f"word out of range: {value}")
    return f"{value:04x}"


def get_xattr_str(path: Path, key: str) -> Optional[str]:
    try:
        raw = os.getxattr(path, key)
    except OSError as ex:
        missing = {
            errno.ENODATA,
            getattr(errno, "ENOATTR", -1),
            errno.EOPNOTSUPP,
            errno.ENOTSUP,
        }
        if ex.errno in missing:
            return None
        raise
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return None


def set_xattr_str(path: Path, key: str, value: str) -> None:
    os.setxattr(path, key, value.encode("utf-8"))


def iter_paths(inputs: Iterable[Path], recursive: bool) -> Iterator[Path]:
    for p in inputs:
        if recursive and p.is_dir():
            for child in p.rglob("*"):
                yield child
        else:
            yield p


def safe_rename(src: Path, dst: Path, dry_run: bool) -> None:
    if src == dst:
        return
    if dst.exists():
        raise FileExistsError(f"target exists: {dst}")
    if dry_run:
        print(f"RENAME {src} -> {dst}")
        return
    src.rename(dst)


def cmd_cadius_to_xattr(args: argparse.Namespace) -> int:
    failures = 0

    for p in iter_paths([Path(x) for x in args.paths], recursive=args.recursive):
        if not p.exists():
            eprint(f"skip missing: {p}")
            failures += 1
            continue
        if p.is_symlink():
            if args.follow_symlinks:
                p = p.resolve()
            else:
                continue

        parsed = parse_cadius_suffix(p.name)
        if parsed is None:
            continue

        try:
            ft_str = format_hex_byte(parsed.file_type)
            aux_str = format_hex_word(parsed.aux_type)

            if args.dry_run:
                print(
                    f"XATTR  {p} {XATTR_FILE_TYPE}={ft_str} {XATTR_AUX_TYPE}={aux_str}"
                )
            else:
                set_xattr_str(p, XATTR_FILE_TYPE, ft_str)
                set_xattr_str(p, XATTR_AUX_TYPE, aux_str)

            if not args.keep_name:
                safe_rename(p, p.with_name(parsed.stem), dry_run=args.dry_run)

        except OSError as ex:
            eprint(f"error: {p}: {ex}")
            failures += 1
        except Exception as ex:
            eprint(f"error: {p}: {ex}")
            failures += 1

    return 1 if failures else 0


def parse_hex_byte_str(value: str) -> Optional[int]:
    if not re.fullmatch(r"[0-9a-fA-F]{2}", value):
        return None
    return int(value, 16)


def parse_hex_word_str(value: str) -> Optional[int]:
    if not re.fullmatch(r"[0-9a-fA-F]{4}", value):
        return None
    return int(value, 16)


def cmd_xattr_to_cadius(args: argparse.Namespace) -> int:
    failures = 0

    for p in iter_paths([Path(x) for x in args.paths], recursive=args.recursive):
        if not p.exists():
            eprint(f"skip missing: {p}")
            failures += 1
            continue
        if p.is_dir() and not args.include_dirs:
            continue
        if p.is_symlink():
            if args.follow_symlinks:
                p = p.resolve()
            else:
                continue

        ft = get_xattr_str(p, XATTR_FILE_TYPE)
        aux = get_xattr_str(p, XATTR_AUX_TYPE)
        if ft is None or aux is None:
            if args.require_xattrs:
                eprint(f"missing xattrs: {p}")
                failures += 1
            continue

        ft_b = parse_hex_byte_str(ft)
        aux_w = parse_hex_word_str(aux)
        if ft_b is None or aux_w is None:
            eprint(f"invalid xattr format: {p} file_type={ft!r} aux_type={aux!r}")
            failures += 1
            continue

        # Strip an existing Cadius suffix if present and overwrite is requested.
        base_name = p.name
        existing = parse_cadius_suffix(base_name)
        if existing is not None:
            if not args.overwrite:
                continue
            base_name = existing.stem

        suffix = (
            f"#{ft_b:02X}{aux_w:04X}" if args.uppercase else f"#{ft_b:02x}{aux_w:04x}"
        )
        dst = p.with_name(base_name + suffix)

        try:
            safe_rename(p, dst, dry_run=args.dry_run)
        except OSError as ex:
            eprint(f"error: {p}: {ex}")
            failures += 1
        except Exception as ex:
            eprint(f"error: {p}: {ex}")
            failures += 1

    return 1 if failures else 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="cadius_xattr_convert",
        description="Convert between Cadius filename metadata and ProDOS8Emu xattrs.",
    )

    sub = p.add_subparsers(dest="cmd", required=True)

    p1 = sub.add_parser(
        "cadius-to-xattr",
        help="Parse NAME#TTAAAA into user.prodos8.file_type/aux_type (and optionally strip suffix).",
    )
    p1.add_argument("paths", nargs="+", help="Files/directories to process")
    p1.add_argument("--recursive", action="store_true", help="Recurse into directories")
    p1.add_argument(
        "--keep-name",
        action="store_true",
        help="Do not rename files (keep #TTAAAA suffix)",
    )
    p1.add_argument(
        "--dry-run", action="store_true", help="Print actions without changing anything"
    )
    p1.add_argument(
        "--follow-symlinks",
        action="store_true",
        help="Follow symlinks (default: skip symlinks)",
    )
    p1.set_defaults(func=cmd_cadius_to_xattr)

    p2 = sub.add_parser(
        "xattr-to-cadius",
        help="Rename files by appending #TTAAAA computed from user.prodos8.file_type/aux_type.",
    )
    p2.add_argument("paths", nargs="+", help="Files/directories to process")
    p2.add_argument("--recursive", action="store_true", help="Recurse into directories")
    p2.add_argument(
        "--dry-run", action="store_true", help="Print actions without changing anything"
    )
    p2.add_argument(
        "--overwrite",
        action="store_true",
        help="If a file already has #TTAAAA, replace it with the xattr values",
    )
    p2.add_argument(
        "--require-xattrs",
        action="store_true",
        help="Fail (non-zero exit) if file_type/aux_type xattrs are missing",
    )
    p2.add_argument(
        "--include-dirs",
        action="store_true",
        help="Also rename directories (default: skip directories)",
    )
    p2.add_argument(
        "--uppercase",
        action="store_true",
        default=True,
        help="Use uppercase hex in filename suffix (default)",
    )
    p2.add_argument(
        "--lowercase",
        action="store_false",
        dest="uppercase",
        help="Use lowercase hex in filename suffix",
    )
    p2.add_argument(
        "--follow-symlinks",
        action="store_true",
        help="Follow symlinks (default: skip symlinks)",
    )
    p2.set_defaults(func=cmd_xattr_to_cadius)

    return p


def main(argv: Optional[Iterable[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(list(argv) if argv is not None else None)
    try:
        return int(args.func(args))
    except OSError as ex:
        eprint(f"fatal: {ex}")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
