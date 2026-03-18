#!/usr/bin/env python3
"""Diff two files after folding single continuation lines.

Pipeline:
1. Read each file and fold continuation lines (lines starting with a space)
   into the previous main line using `|`.
2. Compute a diff of changed lines only.
3. Sort by the first token after the diff marker, then by marker.
4. Split folded lines back on `|` and print to stdout.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from difflib import SequenceMatcher
from pathlib import Path


@dataclass(frozen=True)
class DiffEntry:
    marker: str  # '<' for left-only, '>' for right-only
    text: str


def fold_continuation_lines(path: Path) -> list[str]:
    """Fold continuation lines (starting with a space) into main lines.

    Continuations are appended with `|` to the preceding non-continuation line.
    """

    folded: list[str] = []
    current: str | None = None

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = raw.rstrip("\n")

            if line.startswith(" ") and current is not None:
                # Keep continuation payload but drop the leading continuation marker space.
                current += "|" + line[1:]
                continue

            if current is not None:
                folded.append(current)
            current = line

    if current is not None:
        folded.append(current)

    return folded


def diff_changed_lines(left: list[str], right: list[str]) -> list[DiffEntry]:
    """Return changed lines only, marked as left ('<') or right ('>')."""

    matcher = SequenceMatcher(a=left, b=right, autojunk=False)
    out: list[DiffEntry] = []

    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag in {"replace", "delete"}:
            out.extend(DiffEntry("<", line) for line in left[i1:i2])
        if tag in {"replace", "insert"}:
            out.extend(DiffEntry(">", line) for line in right[j1:j2])

    return out


def first_token(text: str) -> str:
    stripped = text.strip()
    if not stripped:
        return ""
    return stripped.split(None, 1)[0]


def first_token_sort_key(text: str) -> tuple[int, int | str]:
    """Build a sort key for the first token after the diff marker.

    If the token starts with `@`, the remainder is interpreted as a decimal
    integer and sorted numerically. All other tokens fall back to lexical sort.
    """

    token = first_token(text)
    if token.startswith("@"):
        number_text = token[1:]
        if number_text.isdigit():
            return (0, int(number_text))
    return (1, token)


def sort_entries(entries: list[DiffEntry]) -> list[DiffEntry]:
    """Sort by first token after marker, then by marker, then full text."""

    return sorted(
        entries, key=lambda e: (first_token_sort_key(e.text), e.marker, e.text)
    )


def print_split_entries(entries: list[DiffEntry]) -> None:
    """Print entries to stdout, splitting folded text back on `|`."""

    for entry in entries:
        parts = entry.text.split("|")
        if not parts:
            continue

        print(f"{entry.marker} {parts[0]}")
        for continuation in parts[1:]:
            print(f"  {continuation}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", type=Path, help="First input file")
    parser.add_argument("right", type=Path, help="Second input file")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    left_lines = fold_continuation_lines(args.left)
    right_lines = fold_continuation_lines(args.right)

    entries = diff_changed_lines(left_lines, right_lines)
    sorted_entries = sort_entries(entries)
    print_split_entries(sorted_entries)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
