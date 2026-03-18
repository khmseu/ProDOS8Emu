#!/usr/bin/env python3
"""Diff two files after folding stack continuation lines.

Pipeline:
1. Read each file and fold continuation lines using `|`.
    - For `STACK META[...]` lines, attach by `INSN`/`PHASE`.
    - For legacy untagged indented lines, attach to current line.
2. Compute a diff of changed lines only.
3. Sort by the first token after the diff marker, then by marker.
4. Split folded lines back on `|` and print to stdout.
"""

from __future__ import annotations

import argparse
import re
from collections import defaultdict
from dataclasses import dataclass
from difflib import SequenceMatcher
from pathlib import Path

INSTRUCTION_INDEX_RE = re.compile(r"^@(?P<index>\d+)\b")
STACK_META_RE = re.compile(r"^\s*STACK\s+META\[(?P<meta>[^\]]+)\]")
STACK_META_FIELDS_RE = re.compile(
    r"\bINSN=(?P<index>\d+)\s+PHASE=(?P<phase>PRE|POST)\b"
)


@dataclass(frozen=True)
class DiffEntry:
    marker: str  # '<' for left-only, '>' for right-only
    text: str


def parse_stack_meta_target(line: str) -> tuple[int, str] | None:
    """Return (instruction_index, phase) for STACK META lines, else None."""

    meta_match = STACK_META_RE.match(line)
    if meta_match is None:
        return None

    fields_match = STACK_META_FIELDS_RE.search(meta_match.group("meta"))
    if fields_match is None:
        return None

    return (int(fields_match.group("index")), fields_match.group("phase"))


def fold_continuation_lines(path: Path) -> list[str]:
    """Fold continuation lines into instruction lines.

    Rules:
    - Tagged stack lines (STACK META[INSN=.. PHASE=..]) are attached by INSN.
      This keeps PRE lines with their target instruction instead of the previous line.
    - Legacy indented continuation lines are attached to the current line (old behavior).
    """

    folded: list[str] = []
    instruction_positions: dict[int, int] = {}
    pending_pre: dict[int, list[str]] = defaultdict(list)
    pending_post: dict[int, list[str]] = defaultdict(list)

    current: str | None = None
    current_instruction_index: int | None = None

    def append_payload_to_folded(position: int, payload: str) -> None:
        folded[position] += "|" + payload

    def finalize_current() -> None:
        nonlocal current
        nonlocal current_instruction_index

        if current is None:
            return

        folded.append(current)
        if current_instruction_index is not None:
            instruction_positions[current_instruction_index] = len(folded) - 1

        current = None
        current_instruction_index = None

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = raw.rstrip("\n")

            meta_target = parse_stack_meta_target(line)
            if meta_target is not None:
                instruction_index, phase = meta_target
                payload = line[1:] if line.startswith(" ") else line

                if (
                    current_instruction_index == instruction_index
                    and current is not None
                ):
                    current += "|" + payload
                    continue

                target_pos = instruction_positions.get(instruction_index)
                if target_pos is not None:
                    append_payload_to_folded(target_pos, payload)
                    continue

                if phase == "PRE":
                    pending_pre[instruction_index].append(payload)
                else:
                    pending_post[instruction_index].append(payload)
                continue

            if line.startswith(" ") and current is not None:
                # Legacy continuation payload (non-META): keep old behavior.
                current += "|" + line[1:]
                continue

            finalize_current()

            current = line
            instruction_match = INSTRUCTION_INDEX_RE.match(line)
            if instruction_match is not None:
                instruction_index = int(instruction_match.group("index"))
                current_instruction_index = instruction_index

                for payload in pending_pre.pop(instruction_index, []):
                    current += "|" + payload
                for payload in pending_post.pop(instruction_index, []):
                    current += "|" + payload

    finalize_current()

    # Keep any malformed/orphan META records visible instead of dropping them.
    for instruction_index in sorted(set(pending_pre) | set(pending_post)):
        for payload in pending_pre.get(instruction_index, []):
            folded.append(payload)
        for payload in pending_post.get(instruction_index, []):
            folded.append(payload)

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
