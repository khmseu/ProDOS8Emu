#!/usr/bin/env python3
# nosec B101
"""Interactive analyzer to align disassembly trace with EDASM source.

This tool:
- Parses `prodos8emu_disassembly_trace.log`.
- Parses source instructions from `EDASM.SRC/**/*.S`.
- Parses existing `kMonitorSymbols` entries from `src/cpu65c02.cpp`.
- Tries to sync trace mnemonics with source mnemonics.
- Discovers new labels for traced PCs (aliases retained).
- Writes a partial annotated trace and insertion-ready `kMonitorSymbols` entries.
- Prompts for help when sync cannot continue automatically.
"""

from __future__ import annotations

import argparse
import io
import re
import tempfile
from collections import defaultdict, deque
from contextlib import redirect_stdout
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Deque, Iterable, Iterator, Sequence, TypeVar
from unittest.mock import patch

T = TypeVar("T")


@dataclass(frozen=True)
class TraceEntry:
    index: int
    pc: int
    opcode: int
    mnemonic: str
    operand: str
    full_line: str
    pre_pc: int | None
    post_pc: int | None
    pc_symbol: str | None = None
    pre_a: int | None = None
    pre_x: int | None = None
    pre_y: int | None = None
    pre_p: int | None = None
    pre_sp: int | None = None
    pre_stack_bytes: tuple[int, ...] | None = None
    post_stack_bytes: tuple[int, ...] | None = None
    pre_stack_lines: tuple[str, ...] = ()
    post_stack_lines: tuple[str, ...] = ()


@dataclass(frozen=True)
class StackDumpLine:
    raw_line: str
    stack_bytes: tuple[int, ...]
    instruction_index: int | None = None
    phase: str | None = None
    opcode: int | None = None
    pc: int | None = None
    sp: int | None = None


@dataclass(frozen=True)
class SourceInstruction:
    file_path: str
    line_number: int
    label: str | None
    mnemonic: str
    operand: str
    aliases: tuple[str, ...] = ()


@dataclass(frozen=True)
class SyncIssue:
    reason: str
    detail: str


TRACE_LINE_RE = re.compile(
    r"^@(?P<index>\d+)\s+PC=\$(?P<pc>[0-9A-Fa-f]{4})\s+"
    r"(?:\((?P<pcsym>[^\)]+)\)\s+)?"
    r"OP=\$(?P<op>[0-9A-Fa-f]{2})\s+(?P<mnemonic>[A-Za-z0-9]{3,4})\b(?P<tail>.*)$"
)

TRACE_PRE_POST_PC_RE = re.compile(
    r";\s*PRE\s+PC=\$(?P<pre>[0-9A-Fa-f]{4}).*?POST\s+PC=\$(?P<post>[0-9A-Fa-f]{4})"
)

TRACE_PRE_POST_STATE_RE = re.compile(r";\s*PRE\s+(?P<pre>.*?)\s+POST\s+(?P<post>.*)$")

TRACE_STATE_FIELD_RE = re.compile(
    r"\b(?P<name>PC|A|X|Y|SP|P)=\$(?P<value>[0-9A-Fa-f]{2,4})\b"
)

TRACE_STACK_LINE_RE = re.compile(
    r"^\s*STACK(?:\s+META\[(?P<meta>[^\]]+)\])?\s+SP=\$(?P<sp>[0-9A-Fa-f]{2})\s+"
    r"(?:(?P<empty>EMPTY)|USED=(?P<used>\d+):(?P<body>.*))\s*$"
)

TRACE_STACK_BYTE_RE = re.compile(r"\$[0-9A-Fa-f]{4}=\$(?P<value>[0-9A-Fa-f]{2})")

TRACE_STACK_META_RE = re.compile(
    r"\bINSN=(?P<index>\d+)\s+PHASE=(?P<phase>PRE|POST)\s+"
    r"OP=\$(?P<op>[0-9A-Fa-f]{2})\s+PC=\$(?P<pc>[0-9A-Fa-f]{4})\s+"
    r"SP=\$(?P<meta_sp>[0-9A-Fa-f]{2})\b"
)

OPERAND_ADDR16_RE = re.compile(r"\$([0-9A-Fa-f]{4})")

MONITOR_SYMBOL_RE = re.compile(
    r"\{\s*0x(?P<addr>[0-9A-Fa-f]+)\s*,\s*\"(?P<name>[^\"]+)\""
)

SIMPLE_LABEL_EXPR_RE = re.compile(
    r"^(?P<label>[A-Za-z_][A-Za-z0-9_\.]*)" r"(?:\s*(?P<op>[+-])\s*(?P<const>[^\s]+))?$"
)

EQU_STAR_RE = re.compile(
    r"^(?P<label>[^\s:;]+)\s+EQU\s+\*$",
    re.IGNORECASE,
)

RUNTIME_LABEL_ADDR_RE = re.compile(r"^[LX]([0-9A-F]{4})$")

# Matches a $-prefixed hex address of 4 or 2 digits that is not part of a longer hex run.
# 4-digit alternative is listed first so absolute addresses win over ZP prefixes.
OPERAND_ADDR_ANY_RE = re.compile(r"\$([0-9A-Fa-f]{4}|[0-9A-Fa-f]{2})(?![0-9A-Fa-f])")


def parse_trace_state_fields(section: str) -> dict[str, int]:
    fields: dict[str, int] = {}
    for match in TRACE_STATE_FIELD_RE.finditer(section):
        fields[match.group("name").upper()] = int(match.group("value"), 16)
    return fields


# Core 6502/65C02 mnemonics used to identify instruction lines in source.
CPU_MNEMONICS = {
    "ADC",
    "AND",
    "ASL",
    "BBR",
    "BBS",
    "BCC",
    "BCS",
    "BEQ",
    "BIT",
    "BMI",
    "BNE",
    "BPL",
    "BRA",
    "BRK",
    "BVC",
    "BVS",
    "CLC",
    "CLD",
    "CLI",
    "CLV",
    "CMP",
    "CPX",
    "CPY",
    "DEC",
    "DEX",
    "DEY",
    "EOR",
    "INC",
    "INX",
    "INY",
    "JMP",
    "JSR",
    "LDA",
    "LDX",
    "LDY",
    "LSR",
    "MLI",
    "NOP",
    "ORA",
    "PHA",
    "PHP",
    "PHX",
    "PHY",
    "PLA",
    "PLP",
    "PLX",
    "PLY",
    "RMB",
    "ROL",
    "ROR",
    "RTI",
    "RTS",
    "SBC",
    "SEC",
    "SED",
    "SEI",
    "SMB",
    "STA",
    "STP",
    "STX",
    "STY",
    "STZ",
    "TAX",
    "TAY",
    "TRB",
    "TSB",
    "TSX",
    "TXA",
    "TXS",
    "TYA",
    "WAI",
}

BRANCH_MNEMONICS = {
    "BCC",
    "BCS",
    "BEQ",
    "BMI",
    "BNE",
    "BPL",
    "BRA",
    "BVC",
    "BVS",
    "BBR",
    "BBS",
}

STACK_PUSH_MNEMONICS = {
    "JSR",
    "BRK",
    "PHA",
    "PHP",
    "PHX",
    "PHY",
}

STACK_POP_MNEMONICS = {
    "RTS",
    "RTI",
    "PLA",
    "PLP",
    "PLX",
    "PLY",
}

MIN_CONFIDENCE_SCORE_BASE = 4
MIN_CONFIDENCE_SCORE_PER_ENTRY = 2


def normalize_mnemonic(token: str) -> str:
    text = token.upper().rstrip(":")
    if (
        text.startswith(("BBR", "BBS", "RMB", "SMB"))
        and len(text) >= 4
        and text[3:].isdigit()
    ):
        return text[:3]
    return text


def normalize_label_token(token: str) -> str:
    normalized = token.strip().rstrip(":").upper()
    m = re.fullmatch(r"X([0-9A-F]{4})", normalized)
    if m is not None:
        return f"L{m.group(1)}"
    return normalized


def is_pc_label_already_known(
    label: str,
    trace_entry: TraceEntry,
    existing_symbol_tokens_for_pc: set[str],
) -> bool:
    token = normalize_label_token(label)
    if token in existing_symbol_tokens_for_pc:
        return True

    if trace_entry.pc_symbol is not None:
        return token == normalize_label_token(trace_entry.pc_symbol)

    return False


def parse_log_line(line: str) -> TraceEntry | None:
    stripped = line.rstrip("\n")
    match = TRACE_LINE_RE.match(stripped)
    if match is None:
        return None

    tail = match.group("tail")
    operand = tail.split(";", 1)[0].strip()

    pre_pc: int | None = None
    post_pc: int | None = None
    pre_a: int | None = None
    pre_x: int | None = None
    pre_y: int | None = None
    pre_p: int | None = None
    pre_sp: int | None = None
    pc_symbol_raw = match.group("pcsym")
    pc_symbol = pc_symbol_raw.strip() if pc_symbol_raw else None

    pre_fields: dict[str, int] = {}
    post_fields: dict[str, int] = {}
    state_match = TRACE_PRE_POST_STATE_RE.search(stripped)
    if state_match is not None:
        pre_fields = parse_trace_state_fields(state_match.group("pre"))
        post_fields = parse_trace_state_fields(state_match.group("post"))

    pre_pc = pre_fields.get("PC")
    post_pc = post_fields.get("PC")
    pre_a = pre_fields.get("A")
    pre_x = pre_fields.get("X")
    pre_y = pre_fields.get("Y")
    pre_p = pre_fields.get("P")
    pre_sp = pre_fields.get("SP")

    # Fallback for lines where only PRE/POST PC can be extracted.
    pre_post = TRACE_PRE_POST_PC_RE.search(stripped)
    if pre_post is not None:
        if pre_pc is None:
            pre_pc = int(pre_post.group("pre"), 16)
        if post_pc is None:
            post_pc = int(pre_post.group("post"), 16)

    return TraceEntry(
        index=int(match.group("index")),
        pc=int(match.group("pc"), 16),
        opcode=int(match.group("op"), 16),
        mnemonic=normalize_mnemonic(match.group("mnemonic")),
        operand=operand,
        full_line=stripped,
        pre_pc=pre_pc,
        post_pc=post_pc,
        pc_symbol=pc_symbol,
        pre_a=pre_a,
        pre_x=pre_x,
        pre_y=pre_y,
        pre_p=pre_p,
        pre_sp=pre_sp,
    )


def parse_stack_dump_line(line: str) -> StackDumpLine | None:
    stripped = line.rstrip("\n")
    match = TRACE_STACK_LINE_RE.match(stripped)
    if match is None:
        return None

    instruction_index: int | None = None
    phase: str | None = None
    opcode: int | None = None
    pc: int | None = None
    meta_sp: int | None = None
    meta_raw = match.group("meta")
    if meta_raw is not None:
        meta_match = TRACE_STACK_META_RE.search(meta_raw)
        if meta_match is not None:
            instruction_index = int(meta_match.group("index"))
            phase = meta_match.group("phase")
            opcode = int(meta_match.group("op"), 16)
            pc = int(meta_match.group("pc"), 16)
            meta_sp = int(meta_match.group("meta_sp"), 16)

    sp = int(match.group("sp"), 16)

    if match.group("empty") is not None:
        return StackDumpLine(
            raw_line=stripped,
            stack_bytes=(),
            instruction_index=instruction_index,
            phase=phase,
            opcode=opcode,
            pc=pc,
            sp=meta_sp if meta_sp is not None else sp,
        )

    body = match.group("body") or ""
    values = [
        int(byte_match.group("value"), 16)
        for byte_match in TRACE_STACK_BYTE_RE.finditer(body)
    ]

    used_raw = match.group("used")
    if used_raw is not None:
        used = int(used_raw)
        if used == 0:
            values = []

    return StackDumpLine(
        raw_line=stripped,
        stack_bytes=tuple(values),
        instruction_index=instruction_index,
        phase=phase,
        opcode=opcode,
        pc=pc,
        sp=meta_sp if meta_sp is not None else sp,
    )


def trace_entries(log_path: Path) -> Iterator[TraceEntry]:
    pending_untagged_stacks: list[StackDumpLine] = []
    tagged_stacks: dict[tuple[int, str], list[StackDumpLine]] = defaultdict(list)
    current_entry: TraceEntry | None = None
    current_pre_records: list[StackDumpLine] = []
    current_post_records: list[StackDumpLine] = []

    def finalize_current_entry() -> TraceEntry | None:
        nonlocal current_entry, current_pre_records, current_post_records
        if current_entry is None:
            return None

        finalized = replace(
            current_entry,
            pre_stack_bytes=(
                current_pre_records[-1].stack_bytes if current_pre_records else None
            ),
            post_stack_bytes=(
                current_post_records[-1].stack_bytes if current_post_records else None
            ),
            pre_stack_lines=tuple(record.raw_line for record in current_pre_records),
            post_stack_lines=tuple(record.raw_line for record in current_post_records),
        )

        current_entry = None
        current_pre_records = []
        current_post_records = []
        return finalized

    with log_path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            stack_record = parse_stack_dump_line(raw)
            if stack_record is not None:
                if (
                    current_entry is not None
                    and stack_record.instruction_index == current_entry.index
                    and stack_record.phase in {"PRE", "POST"}
                ):
                    if stack_record.phase == "PRE":
                        current_pre_records.append(stack_record)
                    else:
                        current_post_records.append(stack_record)
                    continue

                if (
                    stack_record.instruction_index is not None
                    and stack_record.phase in {"PRE", "POST"}
                ):
                    tagged_stacks[
                        (stack_record.instruction_index, stack_record.phase)
                    ].append(stack_record)
                else:
                    # Legacy untagged format fallback:
                    # If stack line appears immediately after a trace line for a push mnemonic,
                    # treat it as that instruction's post-push dump; otherwise keep it pending
                    # as pre-stack for the next parsed instruction.
                    if (
                        current_entry is not None
                        and current_entry.mnemonic in STACK_PUSH_MNEMONICS
                    ):
                        current_post_records.append(stack_record)
                    else:
                        pending_untagged_stacks.append(stack_record)
                continue

            parsed = parse_log_line(raw)
            if parsed is not None:
                finalized = finalize_current_entry()
                if finalized is not None:
                    yield finalized

                current_entry = parsed
                current_pre_records = tagged_stacks.pop((parsed.index, "PRE"), [])
                current_post_records = tagged_stacks.pop((parsed.index, "POST"), [])

                if (
                    not current_pre_records
                    and not current_post_records
                    and pending_untagged_stacks
                ):
                    current_pre_records = pending_untagged_stacks
                pending_untagged_stacks = []

    finalized = finalize_current_entry()
    if finalized is not None:
        yield finalized


def parse_source_instruction_line(
    file_path: Path,
    line_number: int,
    line: str,
) -> SourceInstruction | None:
    no_newline = line.rstrip("\n")
    code_only = no_newline.split(";", 1)[0].rstrip()
    if not code_only.strip():
        return None

    has_leading_whitespace = code_only[:1].isspace()
    stripped = code_only.strip()
    tokens = stripped.split()
    if not tokens:
        return None

    label: str | None = None
    mnemonic_token: str
    operand_tokens: list[str]

    first = normalize_mnemonic(tokens[0])
    if first in CPU_MNEMONICS:
        mnemonic_token = first
        operand_tokens = tokens[1:]
    elif (
        not has_leading_whitespace
        and len(tokens) >= 2
        and normalize_mnemonic(tokens[1]) in CPU_MNEMONICS
    ):
        label = tokens[0].rstrip(":")
        mnemonic_token = normalize_mnemonic(tokens[1])
        operand_tokens = tokens[2:]
    else:
        return None

    return SourceInstruction(
        file_path=str(file_path),
        line_number=line_number,
        label=label,
        mnemonic=mnemonic_token,
        operand=" ".join(operand_tokens),
    )


def parse_source_tree(source_root: Path) -> list[SourceInstruction]:
    instructions: list[SourceInstruction] = []

    files = sorted(
        path
        for path in source_root.rglob("*")
        if path.is_file() and path.suffix.upper() == ".S"
    )

    for source_file in files:
        pending_equ_star_aliases: list[str] = []
        with source_file.open("r", encoding="utf-8", errors="replace") as handle:
            for line_number, line in enumerate(handle, start=1):
                code_only = line.split(";", 1)[0].strip()
                equ_star = EQU_STAR_RE.match(code_only)
                if equ_star is not None:
                    pending_equ_star_aliases.append(equ_star.group("label").rstrip(":"))
                    continue

                parsed = parse_source_instruction_line(source_file, line_number, line)
                if parsed is not None:
                    if pending_equ_star_aliases:
                        parsed = SourceInstruction(
                            file_path=parsed.file_path,
                            line_number=parsed.line_number,
                            label=parsed.label,
                            mnemonic=parsed.mnemonic,
                            operand=parsed.operand,
                            aliases=tuple(pending_equ_star_aliases),
                        )
                        pending_equ_star_aliases.clear()
                    instructions.append(parsed)

    return instructions


def _eval_preprocessor_condition(expr: str) -> bool:
    cleaned = expr.split("//", 1)[0].strip()
    if not cleaned:
        return True
    if cleaned.startswith("defined"):
        # Macro-aware evaluation is out of scope; default to active.
        return True
    if cleaned.startswith("(") and cleaned.endswith(")"):
        cleaned = cleaned[1:-1].strip()
    try:
        return int(cleaned, 0) != 0
    except ValueError:
        return True


def strip_inactive_preprocessor_blocks(text: str) -> str:
    out_lines: list[str] = []
    stack: list[dict[str, bool]] = []
    active = True

    directive_re = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")

    for line in text.splitlines(keepends=True):
        match = directive_re.match(line)
        if match is None:
            if active:
                out_lines.append(line)
            continue

        kind = match.group(1)
        tail = match.group(2).strip()

        if kind == "if":
            parent_active = active
            cond = _eval_preprocessor_condition(tail)
            branch_active = parent_active and cond
            stack.append(
                {
                    "parent_active": parent_active,
                    "seen_true": cond,
                    "branch_active": branch_active,
                }
            )
            active = branch_active
            continue

        if kind in {"ifdef", "ifndef"}:
            parent_active = active
            branch_active = parent_active
            stack.append(
                {
                    "parent_active": parent_active,
                    "seen_true": True,
                    "branch_active": branch_active,
                }
            )
            active = branch_active
            continue

        if kind == "elif":
            if not stack:
                continue
            frame = stack[-1]
            parent_active = frame["parent_active"]
            if not parent_active or frame["seen_true"]:
                frame["branch_active"] = False
                active = False
            else:
                cond = _eval_preprocessor_condition(tail)
                frame["branch_active"] = parent_active and cond
                frame["seen_true"] = cond
                active = frame["branch_active"]
            continue

        if kind == "else":
            if not stack:
                continue
            frame = stack[-1]
            parent_active = frame["parent_active"]
            branch_active = parent_active and not frame["seen_true"]
            frame["branch_active"] = branch_active
            frame["seen_true"] = True
            active = branch_active
            continue

        if kind == "endif":
            if not stack:
                continue
            frame = stack.pop()
            active = frame["parent_active"]
            continue

    return "".join(out_lines)


def parse_existing_monitor_symbols(symbols_file: Path) -> dict[int, list[str]]:
    text = symbols_file.read_text(encoding="utf-8", errors="replace")

    anchor = text.find("kMonitorSymbols[]")
    if anchor < 0:
        return {}

    block_start = text.find("{", anchor)
    if block_start < 0:
        return {}

    block_end = text.find("};", block_start)
    if block_end < 0:
        block_end = len(text)

    block = strip_inactive_preprocessor_blocks(text[block_start:block_end])

    symbols: dict[int, list[str]] = defaultdict(list)
    for match in MONITOR_SYMBOL_RE.finditer(block):
        addr = int(match.group("addr"), 16)
        name = match.group("name")
        symbols[addr].append(name)

    return dict(symbols)


def parse_trace_log(
    log_path: Path, sample_limit: int = 5
) -> tuple[int, int, list[TraceEntry]]:
    valid_count = 0
    invalid_count = 0
    samples: list[TraceEntry] = []

    with log_path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if parse_stack_dump_line(line) is not None:
                continue

            parsed = parse_log_line(line)
            if parsed is None:
                invalid_count += 1
                continue

            valid_count += 1
            if len(samples) < sample_limit:
                samples.append(parsed)

    return valid_count, invalid_count, samples


def with_fallback_pc_symbol(
    entry: TraceEntry,
    symbols: dict[int, list[str]],
) -> TraceEntry:
    if entry.pc_symbol is not None:
        return entry

    names = symbols.get(entry.pc, [])
    if not names:
        return entry

    return TraceEntry(
        index=entry.index,
        pc=entry.pc,
        opcode=entry.opcode,
        mnemonic=entry.mnemonic,
        operand=entry.operand,
        full_line=entry.full_line,
        pre_pc=entry.pre_pc,
        post_pc=entry.post_pc,
        pc_symbol=names[0],
        pre_a=entry.pre_a,
        pre_x=entry.pre_x,
        pre_y=entry.pre_y,
        pre_p=entry.pre_p,
        pre_sp=entry.pre_sp,
        pre_stack_bytes=entry.pre_stack_bytes,
        post_stack_bytes=entry.post_stack_bytes,
        pre_stack_lines=entry.pre_stack_lines,
        post_stack_lines=entry.post_stack_lines,
    )


def _preview(entries: Iterable[T], limit: int = 3) -> list[T]:
    out: list[T] = []
    for entry in entries:
        out.append(entry)
        if len(out) >= limit:
            break
    return out


def build_ngram_index(
    source: list[SourceInstruction], window: int
) -> dict[tuple[str, ...], list[int]]:
    out: dict[tuple[str, ...], list[int]] = defaultdict(list)
    if window <= 0 or len(source) < window:
        return out

    for i in range(0, len(source) - window + 1):
        key = tuple(inst.mnemonic for inst in source[i : i + window])
        out[key].append(i)
    return dict(out)


def mnemonics_match(trace_mnemonic: str, source_mnemonic: str) -> bool:
    if trace_mnemonic == source_mnemonic:
        return True
    # Trace renders JSR $BF00 as MLI pseudo-op.
    if trace_mnemonic == "MLI" and source_mnemonic == "JSR":
        return True
    return False


def branch_target_from_trace(entry: TraceEntry) -> int | None:
    if entry.mnemonic not in BRANCH_MNEMONICS:
        return None
    # Trace disassembly prints branch destination as $HHHH. For BBR/BBS there can be two
    # addresses in the operand; the destination is the last one.
    all_targets = re.findall(r"\$([0-9A-Fa-f]{4})", entry.operand)
    if not all_targets:
        return None
    return int(all_targets[-1], 16)


def branch_label_from_source(inst: SourceInstruction) -> str | None:
    if inst.mnemonic not in BRANCH_MNEMONICS:
        return None

    pieces = [part.strip() for part in inst.operand.split(",") if part.strip()]
    if not pieces:
        return None

    # For BBR/BBS branch label is usually the last operand; for other branches it is first.
    token = pieces[-1] if inst.mnemonic in {"BBR", "BBS"} else pieces[0]
    parsed = parse_simple_label_expression(token)
    if parsed is None:
        return None
    return parsed[0]


def call_label_from_source(inst: SourceInstruction) -> str | None:
    if inst.mnemonic != "JSR":
        return None

    token = inst.operand.split(",", 1)[0].strip().split(" ", 1)[0]
    if not token:
        return None
    parsed = parse_simple_label_expression(token)
    if parsed is None:
        return None
    return parsed[0]


def jump_label_from_source(inst: SourceInstruction) -> str | None:
    if inst.mnemonic != "JMP":
        return None

    token = inst.operand.split(",", 1)[0].strip().split(" ", 1)[0]
    if not token:
        return None
    if token.startswith("("):
        return None
    parsed = parse_simple_label_expression(token)
    if parsed is None:
        return None
    return parsed[0]


def parse_numeric_constant(token: str) -> int | None:
    text = token.strip()
    if not text:
        return None

    if text.startswith("$"):
        digits = text[1:]
        if not digits or not all(ch in "0123456789ABCDEFabcdef" for ch in digits):
            return None
        return int(digits, 16)

    if text.startswith("0x") or text.startswith("0X"):
        try:
            return int(text, 16)
        except ValueError:
            return None

    if text.isdigit():
        return int(text, 10)

    return None


def parse_simple_label_expression(expr: str) -> tuple[str, int] | None:
    """Parse `LABEL` or `LABEL +/- constant`.

    Returns `(label, signed_offset)` where the source expression means:
        resolved_address = label + signed_offset
    """
    text = expr.strip().rstrip(":")
    if not text:
        return None
    if text.startswith("$") or text.startswith("#") or text[0].isdigit():
        return None

    match = SIMPLE_LABEL_EXPR_RE.fullmatch(text)
    if match is None:
        return None

    label = match.group("label")
    op = match.group("op")
    const_raw = match.group("const")

    if op is None:
        return label, 0
    if const_raw is None:
        return None

    const_value = parse_numeric_constant(const_raw)
    if const_value is None:
        return None

    signed_offset = const_value if op == "+" else -const_value
    return label, signed_offset


def extract_symbolic_operand_expression(operand: str) -> tuple[str, int] | None:
    """Extract symbolic core expression from addressing wrappers.

    Examples handled:
    - `LABEL`
    - `LABEL+1,X`
    - `(LABEL-1),Y`
    """
    stripped = operand.strip()
    if not stripped or stripped.startswith("#"):
        return None

    if stripped.startswith("("):
        close = stripped.find(")")
        if close == -1:
            return None
        inner = stripped[1:close].strip()
        core = inner.split(",", 1)[0].strip()
    else:
        core = stripped.split(",", 1)[0].strip()

    if not core:
        return None

    return parse_simple_label_expression(core)


def operand_symbolic_token(operand: str) -> str | None:
    """Extract the single symbolic (non-numeric, non-hex) label from a source operand.

    Returns None for immediates (#...), already-numeric operands ($hex), or
    operands with no recognisable label token.
    """
    parsed = extract_symbolic_operand_expression(operand)
    if parsed is None:
        return None
    return parsed[0]


def operand_numeric_address_info(operand: str) -> tuple[int, int] | None:
    """Extract resolved address and width from trace operand.

    Returns `(address, bits)` where `bits` is 8 for `$HH` and 16 for `$HHHH`.
    """
    stripped = operand.strip()
    if stripped.startswith("#"):
        return None  # immediate constant, not an address
    m = OPERAND_ADDR_ANY_RE.search(stripped)
    if m is None:
        return None

    token = m.group(1)
    bits = 16 if len(token) == 4 else 8
    return int(token, 16), bits


def operand_numeric_address(operand: str) -> int | None:
    """Extract the resolved numeric address from a trace operand.

    Returns None for immediate operands (#$XX) or operands containing no hex
    address.  Prefers 4-digit (absolute) matches; falls back to 2-digit (ZP).
    """
    info = operand_numeric_address_info(operand)
    if info is None:
        return None
    return info[0]


def runtime_label_fixed_address(label: str) -> int | None:
    """Return fixed hex address for runtime labels like L1234/X1234, else None."""
    match = RUNTIME_LABEL_ADDR_RE.fullmatch(normalize_label_token(label))
    if match is None:
        return None
    return int(match.group(1), 16)


def derive_base_label_address(
    resolved_addr: int,
    addr_bits: int,
    label_offset: int,
) -> int:
    mask = 0xFFFF if addr_bits > 8 else 0x00FF
    # source expression semantics: resolved_addr = label + label_offset
    return (resolved_addr - label_offset) & mask


def extract_operand_label_pair(
    source_inst: SourceInstruction,
    trace_entry: TraceEntry,
) -> tuple[str, int, str] | None:
    """Compatibility wrapper returning at most one extracted operand label pair."""
    pairs = extract_operand_label_pairs(source_inst, trace_entry)
    if not pairs:
        return None
    return pairs[0]


def extract_operand_label_pairs(
    source_inst: SourceInstruction,
    trace_entry: TraceEntry,
) -> list[tuple[str, int, str]]:
    """Given a matched (source_inst, trace_entry) pair, extract operand labels.

    Returns zero or more tuples: (operand_label, resolved_address, flag_string).
    Use an empty flag string for unflagged data labels.
    """
    out: list[tuple[str, int, str]] = []
    mnemonic = source_inst.mnemonic

    # Branch instructions: destination is a PC target.
    if mnemonic in BRANCH_MNEMONICS:
        pieces = [
            part.strip() for part in source_inst.operand.split(",") if part.strip()
        ]
        if not pieces:
            return out
        branch_token = pieces[-1] if mnemonic in {"BBR", "BBS"} else pieces[0]
        parsed = parse_simple_label_expression(branch_token)
        if parsed is None:
            return out
        label, offset = parsed
        addr = branch_target_from_trace(trace_entry)
        if label is not None and addr is not None:
            base_addr = derive_base_label_address(addr, 16, offset)
            out.append((label, base_addr, "MonitorSymbolPc"))
        return out

    # JSR: call destination is a PC target.
    if mnemonic == "JSR":
        token = source_inst.operand.split(",", 1)[0].strip().split(" ", 1)[0]
        parsed = parse_simple_label_expression(token)
        if parsed is not None:
            label, offset = parsed
            if trace_entry.mnemonic == "MLI":
                # MLI pseudo-op carries call metadata in operands; call target is still $BF00.
                base_addr = derive_base_label_address(0xBF00, 16, offset)
                out.append((label, base_addr, "MonitorSymbolPc"))
                return out
            addrs = operand_addresses_16(trace_entry.operand)
            if len(addrs) == 1:
                base_addr = derive_base_label_address(next(iter(addrs)), 16, offset)
                out.append((label, base_addr, "MonitorSymbolPc"))
        return out

    # MLI pseudo-op: treat as two-address instruction where source provides
    # `call_target, param_block`.
    if mnemonic == "MLI":
        pieces = [
            part.strip() for part in source_inst.operand.split(",") if part.strip()
        ]

        if pieces:
            call_parsed = parse_simple_label_expression(pieces[0])
            if call_parsed is not None:
                call_label, call_offset = call_parsed
                call_addr = derive_base_label_address(0xBF00, 16, call_offset)
                out.append((call_label, call_addr, "MonitorSymbolPc"))

        if len(pieces) >= 2:
            parsed = parse_simple_label_expression(pieces[1])
            if parsed is None:
                return out
            label, offset = parsed
            addrs = operand_addresses_16(trace_entry.operand)
            if len(addrs) == 1:
                base_addr = derive_base_label_address(next(iter(addrs)), 16, offset)
                out.append((label, base_addr, ""))
        return out

    # JMP: direct forms are PC targets; indirect vectors fall through.
    if mnemonic == "JMP":
        token = source_inst.operand.split(",", 1)[0].strip().split(" ", 1)[0]
        parsed = parse_simple_label_expression(token)
        if parsed is not None:
            label, offset = parsed
            addrs = operand_addresses_16(trace_entry.operand)
            if len(addrs) == 1:
                base_addr = derive_base_label_address(next(iter(addrs)), 16, offset)
                out.append((label, base_addr, "MonitorSymbolPc"))
                return out
            return out
        # Indirect JMP: the operand is the vector address (data) – fall through.

    # General case: single symbolic token in source vs resolved address in trace.
    expr = extract_symbolic_operand_expression(source_inst.operand)
    if expr is None:
        return out
    label, offset = expr
    addr_info = operand_numeric_address_info(trace_entry.operand)
    if addr_info is None:
        return out
    resolved_addr, addr_bits = addr_info
    base_addr = derive_base_label_address(resolved_addr, addr_bits, offset)
    out.append((label, base_addr, ""))
    return out


def normalize_operand_text(operand: str) -> str:
    return re.sub(r"\s+", "", operand.upper())


def operand_shape_hint(operand: str) -> str:
    text = normalize_operand_text(operand)
    if not text:
        return "implied"
    if text.startswith("#"):
        return "immediate"
    if text.startswith("("):
        if "),Y" in text:
            return "indirect-y"
        if ",X)" in text:
            return "x-indirect"
        return "indirect"
    if text.endswith(",X"):
        base = text[:-2]
        if base.startswith("$") and len(base) == 3:
            return "zeropage-x"
        if base.startswith("$") and len(base) == 5:
            return "absolute-x"
        return "indexed-x"
    if text.endswith(",Y"):
        base = text[:-2]
        if base.startswith("$") and len(base) == 3:
            return "zeropage-y"
        if base.startswith("$") and len(base) == 5:
            return "absolute-y"
        return "indexed-y"
    if text.startswith("$") and len(text) == 3:
        return "zeropage"
    if text.startswith("$") and len(text) == 5:
        return "absolute"
    return "symbolic"


def operand_addresses_16(operand: str) -> set[int]:
    return {int(token, 16) for token in OPERAND_ADDR16_RE.findall(operand)}


def is_confident_sync_metric(
    metric: tuple[int, int, int, int, int], window: int
) -> bool:
    score, exact_operands, addr_overlap_hits, branch_compat_hits, pc_label_hits = metric
    min_score = max(MIN_CONFIDENCE_SCORE_BASE, window * MIN_CONFIDENCE_SCORE_PER_ENTRY)
    strong_signal_hits = (
        exact_operands + addr_overlap_hits + branch_compat_hits + pc_label_hits
    )
    return score >= min_score and strong_signal_hits > 0


def score_trace_source_pair(
    trace_entry: TraceEntry,
    source_inst: SourceInstruction,
    source_index: int,
    source: list[SourceInstruction],
    label_to_source_indexes: dict[str, list[int]],
) -> tuple[int, int, int, int, int]:
    score = 0
    exact_operands = 0
    addr_overlap_hits = 0
    branch_compat_hits = 0
    pc_label_hits = 0

    if trace_entry.pc_symbol is not None:
        trace_pc_symbol = normalize_label_token(trace_entry.pc_symbol)
        source_names: list[str] = []
        if source_inst.label is not None:
            source_names.append(source_inst.label)
        source_names.extend(source_inst.aliases)
        if any(normalize_label_token(name) == trace_pc_symbol for name in source_names):
            score += 10
            pc_label_hits += 1

    trace_operand = normalize_operand_text(trace_entry.operand)
    source_operand = normalize_operand_text(source_inst.operand)
    if trace_operand == source_operand:
        score += 8
        exact_operands += 1
    else:
        trace_shape = operand_shape_hint(trace_entry.operand)
        source_shape = operand_shape_hint(source_inst.operand)
        if trace_shape == source_shape:
            score += 2
        else:
            score -= 1

        trace_addrs = operand_addresses_16(trace_entry.operand)
        source_addrs = operand_addresses_16(source_inst.operand)
        if trace_addrs and source_addrs:
            overlap = trace_addrs & source_addrs
            if overlap:
                score += 6
                addr_overlap_hits += 1
            else:
                score -= 4
        elif trace_addrs and not source_addrs:
            score -= 2

    if (
        trace_entry.mnemonic in BRANCH_MNEMONICS
        and source_inst.mnemonic in BRANCH_MNEMONICS
    ):
        trace_target = branch_target_from_trace(trace_entry)
        source_label = branch_label_from_source(source_inst)
        if trace_target is not None and source_label is not None:
            target_idx = choose_source_label_target(
                source_label,
                source_index,
                source,
                label_to_source_indexes,
            )
            if target_idx is not None:
                trace_backwards = trace_target <= trace_entry.pc
                source_backwards = target_idx <= source_index
                if trace_backwards == source_backwards:
                    # Branch direction compatibility is only a soft bonus.
                    score += 1
                    branch_compat_hits += 1

    return score, exact_operands, addr_overlap_hits, branch_compat_hits, pc_label_hits


def score_sync_candidate(
    pending_window: list[TraceEntry],
    source_start: int,
    source: list[SourceInstruction],
    label_to_source_indexes: dict[str, list[int]],
) -> tuple[int, int, int, int, int]:
    total_score = 0
    exact_operands = 0
    addr_overlap_hits = 0
    branch_compat_hits = 0
    pc_label_hits = 0

    if source_start < 0 or source_start + len(pending_window) > len(source):
        return (-10_000, 0, 0, 0, 0)

    for offset, trace_entry in enumerate(pending_window):
        source_inst = source[source_start + offset]
        if not mnemonics_match(trace_entry.mnemonic, source_inst.mnemonic):
            return (-10_000, 0, 0, 0, 0)

        pair_score, pair_exact, pair_overlap, pair_branch, pair_pc_label = (
            score_trace_source_pair(
                trace_entry,
                source_inst,
                source_start + offset,
                source,
                label_to_source_indexes,
            )
        )
        total_score += pair_score
        exact_operands += pair_exact
        addr_overlap_hits += pair_overlap
        branch_compat_hits += pair_branch
        pc_label_hits += pair_pc_label

    return (
        total_score,
        exact_operands,
        addr_overlap_hits,
        branch_compat_hits,
        pc_label_hits,
    )


def choose_source_label_target(
    label: str,
    current_source_index: int,
    source: list[SourceInstruction],
    label_to_source_indexes: dict[str, list[int]],
) -> int | None:
    candidates = label_to_source_indexes.get(normalize_label_token(label), [])
    if not candidates:
        return None

    current_file = source[current_source_index].file_path
    same_file = [idx for idx in candidates if source[idx].file_path == current_file]
    pool = same_file if same_file else candidates
    return min(pool, key=lambda idx: abs(idx - current_source_index))


def derive_stack_top_addr_from_trace_stack(
    trace_stack_bytes: Sequence[int] | None,
) -> int | None:
    if trace_stack_bytes is None or len(trace_stack_bytes) < 2:
        return None

    low = trace_stack_bytes[0] & 0xFF
    high = trace_stack_bytes[1] & 0xFF
    return ((high << 8) | low) & 0xFFFF


def derive_rts_return_pc_from_trace_stack(
    trace_stack_bytes: Sequence[int] | None,
) -> int | None:
    stack_top_addr = derive_stack_top_addr_from_trace_stack(trace_stack_bytes)
    if stack_top_addr is None:
        return None
    return (stack_top_addr + 1) & 0xFFFF


def detect_rts_alignment_issue(
    trace_entry: TraceEntry,
    source_inst: SourceInstruction,
    source_pos: int,
) -> SyncIssue | None:
    if trace_entry.mnemonic != "RTS":
        return None
    if source_inst.mnemonic != "RTS":
        return None
    if trace_entry.post_pc is None:
        return None

    if trace_entry.pre_stack_bytes is None or len(trace_entry.pre_stack_bytes) < 2:
        return SyncIssue(
            "rts-insufficient-trace-stack",
            (
                f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} RTS to "
                f"${trace_entry.post_pc:04X}: trace stack bytes missing or insufficient "
                f"({source_inst.file_path}:{source_inst.line_number}, source[{source_pos}])."
            ),
        )

    trace_stack_return_pc = derive_rts_return_pc_from_trace_stack(
        trace_entry.pre_stack_bytes
    )
    # trace_stack_return_pc is non-None here; pre_stack_bytes length >= 2 is guaranteed.
    if trace_stack_return_pc == trace_entry.post_pc:
        return None

    return SyncIssue(
        "rts-return-mismatch",
        (
            f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} RTS returned to "
            f"${trace_entry.post_pc:04X}, but trace stack top implies "
            f"${trace_stack_return_pc:04X} "
            f"({source_inst.file_path}:{source_inst.line_number}, source[{source_pos}])."
        ),
    )


def format_source_location(source: list[SourceInstruction], source_index: int) -> str:
    if 0 <= source_index < len(source):
        inst = source[source_index]
        return f"{inst.file_path}:{inst.line_number}"
    return f"src[{source_index}]"


def build_rts_assoc_diagnostics(
    trace_entry: TraceEntry,
    jsr_stack_associations: dict[int, int],
    source: list[SourceInstruction],
) -> list[str]:
    if trace_entry.mnemonic != "RTS":
        return []

    stack_top_addr = derive_stack_top_addr_from_trace_stack(trace_entry.pre_stack_bytes)
    looked_for = f"${stack_top_addr:04X}" if stack_top_addr is not None else "none"
    looked_for_position = (
        jsr_stack_associations.get(stack_top_addr)
        if stack_top_addr is not None
        else None
    )
    known_positions_text = (
        ", ".join(
            f"${stack_addr:04X}->{format_source_location(source, pos)}"
            for stack_addr, pos in sorted(jsr_stack_associations.items())
        )
        if jsr_stack_associations
        else "(empty)"
    )
    assoc_entries = (
        ", ".join(
            f"{format_source_location(source, source_index)}<-${stack_addr:04X}"
            for stack_addr, source_index in sorted(
                jsr_stack_associations.items(),
                key=lambda item: (item[1], item[0]),
            )
        )
        or "(empty)"
    )

    mode = "assoc-hit" if looked_for_position is not None else "non-assoc"
    looked_for_position_text = (
        format_source_location(source, looked_for_position)
        if looked_for_position is not None
        else "none"
    )

    return [
        (
            f"  RTS @{trace_entry.index} PC=${trace_entry.pc:04X} "
            f"{mode}: looked-for={looked_for} "
            f"looked-for-pos={looked_for_position_text} "
            f"known-positions=[{known_positions_text}]"
        ),
        f"    known-assocs-by-position: {{{assoc_entries}}}",
    ]


def record_runtime_label_event(
    runtime_label_events: list[dict[str, object]] | None,
    kind: str,
    trace_entry: TraceEntry,
    runtime_label: str,
    target_source_idx: int,
    source: list[SourceInstruction],
) -> None:
    if runtime_label_events is None:
        return

    if not (0 <= target_source_idx < len(source)):
        return

    target = source[target_source_idx]
    runtime_label_events.append(
        {
            "kind": kind,
            "trace_index": trace_entry.index,
            "trace_pc": trace_entry.pc,
            "runtime_label": runtime_label,
            "target_source_index": target_source_idx,
            "target_source_file": target.file_path,
            "target_source_line": target.line_number,
        }
    )


def advance_source_position(
    trace_entry: TraceEntry,
    source_pos: int,
    source: list[SourceInstruction],
    label_to_source_indexes: dict[str, list[int]],
    return_frames: list[tuple[int, int | None]] | None = None,
    observed_next_pc: int | None = None,
    runtime_symbols_by_addr: dict[int, list[str]] | None = None,
    runtime_label_events: list[dict[str, object]] | None = None,
    jsr_stack_associations: dict[int, int] | None = None,
    rts_miss_log: list[str] | None = None,
    jsr_assoc_log: list[str] | None = None,
    indirect_jmp_log: list[str] | None = None,
    control_flow_sync_issues: list[SyncIssue] | None = None,
) -> int:
    if return_frames is None:
        return_frames = []
    if jsr_stack_associations is None:
        jsr_stack_associations = {}

    source_inst = source[source_pos]
    mnemonic = source_inst.mnemonic
    next_source_pos = source_pos + 1
    observed_post_pc = (
        trace_entry.post_pc if trace_entry.post_pc is not None else observed_next_pc
    )

    if trace_entry.mnemonic in BRANCH_MNEMONICS:
        branch_target = branch_target_from_trace(trace_entry)
        if branch_target is not None and observed_post_pc == branch_target:
            branch_label = branch_label_from_source(source_inst)
            if branch_label is not None:
                target_idx = choose_source_label_target(
                    branch_label,
                    source_pos,
                    source,
                    label_to_source_indexes,
                )
                if target_idx is not None:
                    next_source_pos = target_idx

    if mnemonic == "JMP":
        jump_label = jump_label_from_source(source_inst)
        if jump_label is not None:
            target_idx = choose_source_label_target(
                jump_label,
                source_pos,
                source,
                label_to_source_indexes,
            )
            if target_idx is not None:
                next_source_pos = target_idx
            elif control_flow_sync_issues is not None:
                control_flow_sync_issues.append(
                    SyncIssue(
                        "jmp-unresolved-target",
                        (
                            f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} JMP could not "
                            f"resolve source target label {jump_label} "
                            f"({source_inst.file_path}:{source_inst.line_number}, source[{source_pos}])."
                        ),
                    )
                )
        else:
            runtime_label_candidates: list[str] = []
            if observed_post_pc is not None:
                runtime_label_candidates.append(f"L{observed_post_pc:04X}")
                if runtime_symbols_by_addr is not None:
                    runtime_label_candidates.extend(
                        runtime_symbols_by_addr.get(observed_post_pc, [])
                    )

            seen_candidates: set[str] = set()
            for candidate_label in runtime_label_candidates:
                normalized_candidate = normalize_label_token(candidate_label)
                if normalized_candidate in seen_candidates:
                    continue
                seen_candidates.add(normalized_candidate)

                target_idx = choose_source_label_target(
                    candidate_label,
                    source_pos,
                    source,
                    label_to_source_indexes,
                )
                if target_idx is None:
                    continue

                next_source_pos = target_idx
                if indirect_jmp_log is not None:
                    indirect_jmp_log.extend(
                        build_indirect_jmp_follow_diagnostics(
                            trace_entry,
                            source,
                            source_pos,
                            observed_post_pc,
                            target_idx,
                            candidate_label,
                        )
                    )
                record_runtime_label_event(
                    runtime_label_events,
                    "jmp-indirect-runtime-fallback",
                    trace_entry,
                    candidate_label,
                    target_idx,
                    source,
                )
                break
            else:
                if indirect_jmp_log is not None:
                    indirect_jmp_log.extend(
                        build_indirect_jmp_follow_diagnostics(
                            trace_entry,
                            source,
                            source_pos,
                            observed_post_pc,
                            None,
                        )
                    )
                if control_flow_sync_issues is not None:
                    control_flow_sync_issues.append(
                        SyncIssue(
                            "indirect-jmp-unresolved-target",
                            (
                                f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} indirect JMP could not "
                                f"resolve continuation from effective target "
                                f"{f'${observed_post_pc:04X}' if observed_post_pc is not None else 'none'} "
                                f"({source_inst.file_path}:{source_inst.line_number}, source[{source_pos}])."
                            ),
                        )
                    )

    if mnemonic == "JSR":
        if trace_entry.mnemonic == "MLI":
            # Trace collapses JSR $BF00 plus inline MLI call metadata into a single
            # pseudo-instruction, so source should advance past the JSR source line.
            return next_source_pos

        return_pc = (trace_entry.pc + 3) & 0xFFFF
        call_label = call_label_from_source(source_inst)
        stepped_into_callee = False
        if call_label is not None and (
            observed_post_pc is None or observed_post_pc != return_pc
        ):
            target_idx = choose_source_label_target(
                call_label,
                source_pos,
                source,
                label_to_source_indexes,
            )
            if target_idx is not None:
                next_source_pos = target_idx
                stepped_into_callee = True
        elif (
            observed_post_pc is not None
            and observed_post_pc != return_pc
            and runtime_symbols_by_addr is not None
        ):
            for candidate_label in runtime_symbols_by_addr.get(observed_post_pc, []):
                target_idx = choose_source_label_target(
                    candidate_label,
                    source_pos,
                    source,
                    label_to_source_indexes,
                )
                if target_idx is not None:
                    next_source_pos = target_idx
                    stepped_into_callee = True
                    record_runtime_label_event(
                        runtime_label_events,
                        "jsr-runtime-fallback",
                        trace_entry,
                        candidate_label,
                        target_idx,
                        source,
                    )
                    break

        if not stepped_into_callee and control_flow_sync_issues is not None:
            if observed_post_pc == return_pc:
                control_flow_sync_issues.append(
                    SyncIssue(
                        "jsr-linear-fallback-rejected",
                        (
                            f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} JSR would fall through to "
                            f"its return PC ${return_pc:04X} instead of entering a callee "
                            f"({source_inst.file_path}:{source_inst.line_number}, source[{source_pos}])."
                        ),
                    )
                )
            else:
                control_flow_sync_issues.append(
                    SyncIssue(
                        "jsr-unresolved-target",
                        (
                            f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} JSR could not resolve callee "
                            f"from observed target {f'${observed_post_pc:04X}' if observed_post_pc is not None else 'none'} "
                            f"and source operand {call_label or 'none'} "
                            f"({source_inst.file_path}:{source_inst.line_number}, source[{source_pos}])."
                        ),
                    )
                )
            return next_source_pos

        if stepped_into_callee:
            dump_return_pc = derive_rts_return_pc_from_trace_stack(
                trace_entry.post_stack_bytes
            )
            return_frames.append((source_pos + 1, dump_return_pc or return_pc))

        pushed_return_addr = derive_stack_top_addr_from_trace_stack(
            trace_entry.post_stack_bytes
        )
        if pushed_return_addr is None:
            pushed_return_addr = (return_pc - 1) & 0xFFFF
        jsr_stack_associations[pushed_return_addr] = source_pos + 1
        if jsr_assoc_log is not None:
            jsr_assoc_log.append(
                f"  JSR @{trace_entry.index} PC=${trace_entry.pc:04X} "
                f"assoc: ${pushed_return_addr:04X}->{format_source_location(source, source_pos + 1)}"
            )

    if mnemonic == "RTS":
        stack_top_addr = derive_stack_top_addr_from_trace_stack(
            trace_entry.pre_stack_bytes
        )
        trace_derived_pc = derive_rts_return_pc_from_trace_stack(
            trace_entry.pre_stack_bytes
        )
        effective_post_pc = (
            trace_derived_pc if trace_derived_pc is not None else observed_post_pc
        )

        if rts_miss_log is not None:
            rts_miss_log.extend(
                build_rts_assoc_diagnostics(trace_entry, jsr_stack_associations, source)
            )

        if stack_top_addr is not None and stack_top_addr in jsr_stack_associations:
            assoc_target = jsr_stack_associations[stack_top_addr]
            if rts_miss_log is not None:
                rts_miss_log.append(
                    f"    RTS decision: method=assoc-map "
                    f"params={{stack-top=${stack_top_addr:04X}, target={format_source_location(source, assoc_target)}}}"
                )
            return assoc_target

        if return_frames:
            if effective_post_pc is not None:
                for i in range(len(return_frames) - 1, -1, -1):
                    return_index, return_pc = return_frames[i]
                    if return_pc == effective_post_pc:
                        if rts_miss_log is not None:
                            rts_miss_log.append(
                                f"    RTS decision: method=return-frames-pc-match "
                                f"params={{effective-post-pc=${effective_post_pc:04X}, "
                                f"matched-frame={i}, target={format_source_location(source, return_index)}}}"
                            )
                        del return_frames[i:]
                        return return_index
            else:
                return_index, _ = return_frames.pop()
                if rts_miss_log is not None:
                    rts_miss_log.append(
                        f"    RTS decision: method=return-frames-lifo-fallback "
                        f"params={{effective-post-pc=none, target={format_source_location(source, return_index)}}}"
                    )
                return return_index

        if effective_post_pc is not None:
            runtime_label_candidates = [f"L{effective_post_pc:04X}"]
            if runtime_symbols_by_addr is not None:
                runtime_label_candidates.extend(
                    runtime_symbols_by_addr.get(effective_post_pc, [])
                )

            for candidate_label in runtime_label_candidates:
                runtime_target_idx = choose_source_label_target(
                    candidate_label,
                    source_pos,
                    source,
                    label_to_source_indexes,
                )
                if runtime_target_idx is not None:
                    record_runtime_label_event(
                        runtime_label_events,
                        "rts-runtime-fallback",
                        trace_entry,
                        candidate_label,
                        runtime_target_idx,
                        source,
                    )
                    if rts_miss_log is not None:
                        rts_miss_log.append(
                            f"    RTS decision: method=runtime-label-fallback "
                            f"params={{effective-post-pc=${effective_post_pc:04X}, "
                            f"label={candidate_label}, target={format_source_location(source, runtime_target_idx)}}}"
                        )
                    return runtime_target_idx
            if rts_miss_log is not None:
                rts_miss_log.append(
                    f"    RTS decision: method=linear-fallback-rejected "
                    f"params={{effective-post-pc=${effective_post_pc:04X}, "
                    f"next-source={format_source_location(source, next_source_pos)}}}"
                )
            if control_flow_sync_issues is not None:
                control_flow_sync_issues.append(
                    SyncIssue(
                        "rts-linear-fallback-rejected",
                        (
                            f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} RTS could not resolve return "
                            f"target for effective post PC ${effective_post_pc:04X} "
                            f"({source_inst.file_path}:{source_inst.line_number}, source[{source_pos}])."
                        ),
                    )
                )
            return next_source_pos

        if rts_miss_log is not None:
            rts_miss_log.append(
                f"    RTS decision: method=linear-fallback-rejected "
                f"params={{effective-post-pc=none, next-source={format_source_location(source, next_source_pos)}}}"
            )
        if control_flow_sync_issues is not None:
            control_flow_sync_issues.append(
                SyncIssue(
                    "rts-linear-fallback-rejected",
                    (
                        f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} RTS has no resolvable return target "
                        f"({source_inst.file_path}:{source_inst.line_number}, source[{source_pos}])."
                    ),
                )
            )
        return next_source_pos

    return next_source_pos


def prompt_for_help(
    issue: SyncIssue,
    pending_trace: list[TraceEntry],
    source: list[SourceInstruction],
    current_source_index: int | None,
    label_to_source_indexes: dict[str, list[int]],
    recent_trace: list[TraceEntry] | None = None,
    last_matched_source_index: int | None = None,
    recent_source_indexes: Sequence[int | None] | None = None,
) -> tuple[str, int | None]:
    def format_trace_prompt_entry(marker: str, entry: TraceEntry) -> str:
        return (
            f"{marker} @{entry.index} PC=${entry.pc:04X} OP=${entry.opcode:02X} "
            f"{entry.mnemonic} {entry.operand}"
        ).rstrip()

    print("\n=== Alignment Needs Help ===")
    print(f"Reason: {issue.reason}")
    print(issue.detail)

    print("\nLegend:")
    print("  => current trace/source candidate")
    if recent_trace or last_matched_source_index is not None:
        print("  >> previous trace entry / last matched source instruction")

    print("\nPending trace entries:")
    if recent_trace:
        displayed = recent_trace[-6:]
        for entry in displayed:
            print(format_trace_prompt_entry(">>", entry))
    for idx, entry in enumerate(pending_trace[:5]):
        marker = "=>" if idx == 0 else "  "
        print(format_trace_prompt_entry(marker, entry))

    if current_source_index is not None:
        lo = max(0, current_source_index - 6)
        hi = min(len(source), current_source_index + 4)
        local_indexes = set(range(lo, hi))
        previous_indexes: list[int] = []
        if recent_source_indexes:
            for idx in recent_source_indexes[-6:]:
                if idx is None or not (0 <= idx < len(source)):
                    continue
                if idx not in previous_indexes:
                    previous_indexes.append(idx)
        previous_index_set = set(previous_indexes)
        display_indexes = sorted(local_indexes | previous_index_set)

        print("\nSource context:")
        previous_displayed_idx: int | None = None
        for idx in display_indexes:
            if previous_displayed_idx is not None and idx - previous_displayed_idx > 1:
                print("  ...")
            previous_displayed_idx = idx

            inst = source[idx]
            label = f"{inst.label} " if inst.label else ""
            pointer = "  "
            if idx == current_source_index:
                pointer = "=>"
            if (
                last_matched_source_index is not None
                and idx == last_matched_source_index
            ):
                pointer = "*>" if idx == current_source_index else ">>"
            elif idx in previous_index_set:
                pointer = ">>"

            extra = ""
            if idx in previous_index_set and idx not in local_indexes:
                extra = " [previous-match]"

            rendered_instruction = f"{label}{inst.mnemonic}"
            if inst.operand:
                rendered_instruction += f" {inst.operand}"
            rendered_instruction += extra
            print(
                f"{pointer} [{idx}] {inst.file_path}:{inst.line_number} "
                f"{rendered_instruction}".rstrip()
            )

    print("\nActions:")
    print("  s              skip one trace instruction and retry")
    print("  j <index>      jump source pointer to absolute instruction index")
    print("  l <label>      jump source pointer to first instruction with this label")
    print("  q              quit and finalize partial outputs")

    while True:
        try:
            raw = input("help> ").strip()
        except EOFError:
            return "quit", None

        if raw == "":
            continue
        if raw == "s":
            return "skip", None
        if raw == "q":
            return "quit", None
        if raw.startswith("j "):
            value = raw[2:].strip()
            if value.isdigit():
                idx = int(value)
                if 0 <= idx < len(source):
                    return "jump", idx
            print("Invalid source index.")
            continue
        if raw.startswith("l "):
            label = raw[2:].strip().rstrip(":")
            matches = label_to_source_indexes.get(normalize_label_token(label), [])
            if not matches:
                print("Label not found in parsed source instructions.")
                continue
            if len(matches) > 1:
                print(
                    f"Label has {len(matches)} matches, using first index {matches[0]}."
                )
            return "jump", matches[0]

        print("Unknown action. Use s, j <index>, l <label>, or q.")


def acquire_sync_window(
    pending: Deque[TraceEntry],
    source: list[SourceInstruction],
    ngram_index: dict[tuple[str, ...], list[int]],
    label_to_source_indexes: dict[str, list[int]],
    window: int,
) -> tuple[int | None, SyncIssue | None]:
    if len(pending) < window:
        return None, SyncIssue(
            "insufficient-window", f"Need {window} trace entries, have {len(pending)}"
        )

    key = tuple(entry.mnemonic for entry in list(pending)[:window])
    candidates = ngram_index.get(key, [])
    if len(candidates) == 0:
        return None, SyncIssue(
            "no-sync-candidate",
            f"No source window matches mnemonic key: {' '.join(key)}",
        )

    pending_window = list(pending)[:window]
    scored: list[tuple[tuple[int, int, int, int, int], int]] = []
    for candidate in candidates:
        scored.append(
            (
                score_sync_candidate(
                    pending_window,
                    candidate,
                    source,
                    label_to_source_indexes,
                ),
                candidate,
            )
        )

    if len(candidates) == 1:
        only_metric, only_idx = scored[0]
        if is_confident_sync_metric(only_metric, window):
            return only_idx, None
        return None, SyncIssue(
            "low-confidence-sync-candidate",
            (
                f"Unique source window for key {' '.join(key)} was rejected "
                f"(score {only_metric[0]}, exact {only_metric[1]}, "
                f"overlap {only_metric[2]}, branch {only_metric[3]}, "
                f"pcsym {only_metric[4]})."
            ),
        )

    best_metric = max(metric for metric, _ in scored)
    best_candidates = [idx for metric, idx in scored if metric == best_metric]
    if len(best_candidates) == 1:
        best_idx = best_candidates[0]
        if is_confident_sync_metric(best_metric, window):
            return best_idx, None
        return None, SyncIssue(
            "low-confidence-sync-candidate",
            (
                f"Best source window for key {' '.join(key)} was unique but weak "
                f"(score {best_metric[0]}, exact {best_metric[1]}, "
                f"overlap {best_metric[2]}, branch {best_metric[3]}, "
                f"pcsym {best_metric[4]})."
            ),
        )

    return None, SyncIssue(
        "ambiguous-sync-candidate",
        (
            f"{len(candidates)} source windows match key: {' '.join(key)} "
            f"(best score {best_metric[0]}, exact {best_metric[1]}, "
            f"overlap {best_metric[2]}, branch {best_metric[3]}, "
            f"pcsym {best_metric[4]})."
        ),
    )


def attempt_local_auto_resync(
    pending: Deque[TraceEntry],
    source: list[SourceInstruction],
    ngram_index: dict[tuple[str, ...], list[int]],
    label_to_source_indexes: dict[str, list[int]],
    current_source_index: int,
    window: int,
    search_radius: int = 64,
) -> int | None:
    if window <= 0 or len(pending) < window:
        return None

    pending_window = list(pending)[:window]
    key = tuple(entry.mnemonic for entry in pending_window)
    candidates = ngram_index.get(key, [])
    if not candidates:
        return None

    local_candidates = [
        idx for idx in candidates if abs(idx - current_source_index) <= search_radius
    ]
    if not local_candidates:
        return None

    ranked: list[tuple[tuple[int, int, int, int, int, int], int]] = []
    for idx in local_candidates:
        metric = score_sync_candidate(
            pending_window,
            idx,
            source,
            label_to_source_indexes,
        )
        distance_bias = -abs(idx - current_source_index)
        ranked.append(
            (
                (metric[0], metric[1], metric[2], metric[3], metric[4], distance_bias),
                idx,
            )
        )

    best_metric = max(metric for metric, _ in ranked)
    best_entries = [(metric, idx) for metric, idx in ranked if metric == best_metric]
    if len(best_entries) != 1:
        return None

    best_scored_metric, best_idx = best_entries[0]
    score_metric = (
        best_scored_metric[0],
        best_scored_metric[1],
        best_scored_metric[2],
        best_scored_metric[3],
        best_scored_metric[4],
    )
    if not is_confident_sync_metric(score_metric, window):
        return None

    return best_idx


def attempt_pc_symbol_resync(
    trace_entry: TraceEntry,
    source: list[SourceInstruction],
    label_to_source_indexes: dict[str, list[int]],
    current_source_index: int,
    runtime_label_events: list[dict[str, object]] | None = None,
    event_kind: str = "pc-symbol-resync",
) -> int | None:
    # Prefer exact runtime address labels (Lxxxx / Xxxxx canonicalized to Lxxxx)
    # over semantic aliases (e.g. COUT), because aliases can map to non-relocated
    # source locations while Lxxxx points to the trace address directly.
    runtime_label = f"L{trace_entry.pc:04X}"
    runtime_idx = choose_source_label_target(
        runtime_label,
        current_source_index,
        source,
        label_to_source_indexes,
    )
    if runtime_idx is not None:
        if runtime_idx != current_source_index:
            record_runtime_label_event(
                runtime_label_events,
                event_kind,
                trace_entry,
                runtime_label,
                runtime_idx,
                source,
            )
        return runtime_idx

    if trace_entry.pc_symbol is None:
        return None

    pc_symbol_idx = choose_source_label_target(
        trace_entry.pc_symbol,
        current_source_index,
        source,
        label_to_source_indexes,
    )
    if pc_symbol_idx is not None and pc_symbol_idx != current_source_index:
        record_runtime_label_event(
            runtime_label_events,
            event_kind,
            trace_entry,
            trace_entry.pc_symbol,
            pc_symbol_idx,
            source,
        )
    return pc_symbol_idx


def attempt_indirect_jmp_next_trace_resync(
    pending: Deque[TraceEntry],
    source: list[SourceInstruction],
    label_to_source_indexes: dict[str, list[int]],
    current_source_index: int,
    runtime_label_events: list[dict[str, object]] | None = None,
) -> int | None:
    if len(pending) < 2:
        return None

    current = pending[0]
    if current.mnemonic != "JMP":
        return None

    operand = normalize_operand_text(current.operand)
    if not operand.startswith("("):
        return None

    # For indirect JMP, follow observed flow using the next trace entry's target PC symbol.
    next_entry = pending[1]
    return attempt_pc_symbol_resync(
        next_entry,
        source,
        label_to_source_indexes,
        current_source_index,
        runtime_label_events=runtime_label_events,
        event_kind="indirect-jmp-next-trace-resync",
    )


def build_indirect_jmp_resync_diagnostics(
    pending: Deque[TraceEntry],
    source: list[SourceInstruction],
    current_source_index: int,
    target_source_index: int,
) -> list[str]:
    if len(pending) < 2:
        return []

    current = pending[0]
    next_entry = pending[1]
    if current.mnemonic != "JMP" or not normalize_operand_text(
        current.operand
    ).startswith("("):
        return []

    method = (
        "next-trace-runtime-label"
        if f"L{next_entry.pc:04X}" == normalize_label_token(next_entry.pc_symbol or "")
        else "next-trace-pc-symbol"
    )
    next_symbol = next_entry.pc_symbol if next_entry.pc_symbol is not None else "none"
    return [
        (
            f"  IJMP @{current.index} PC=${current.pc:04X} non-linear-follow: "
            f"current={format_source_location(source, current_source_index)}"
        ),
        (
            f"    IJMP decision: method={method} "
            f"params={{next-trace=@{next_entry.index} PC=${next_entry.pc:04X}, "
            f"next-pc-symbol={next_symbol}, target={format_source_location(source, target_source_index)}}}"
        ),
    ]


def build_indirect_jmp_conflict_diagnostics(
    pending: Deque[TraceEntry],
    source: list[SourceInstruction],
    current_source_index: int,
) -> list[str]:
    if len(pending) < 2:
        return []

    current = pending[0]
    next_entry = pending[1]
    if current.mnemonic != "JMP" or not normalize_operand_text(
        current.operand
    ).startswith("("):
        return []

    next_runtime_label = f"L{next_entry.pc:04X}"
    next_symbol = next_entry.pc_symbol if next_entry.pc_symbol is not None else "none"
    return [
        (
            f"  IJMP @{current.index} PC=${current.pc:04X} sync-conflict: "
            f"current={format_source_location(source, current_source_index)}"
        ),
        (
            f"    IJMP conflict: method=next-trace-follow-failed "
            f"params={{next-trace=@{next_entry.index} PC=${next_entry.pc:04X}, "
            f"runtime-label={next_runtime_label}, next-pc-symbol={next_symbol}}}"
        ),
    ]


def build_operand_label_mismatch_diagnostics(
    trace_entry: TraceEntry,
    source: list[SourceInstruction],
    source_index: int,
    operand_label: str,
    resolved_addr: int,
    expected_addr: int,
) -> list[str]:
    return [
        (
            f"  OP-LABEL @{trace_entry.index} PC=${trace_entry.pc:04X} sync-conflict: "
            f"current={format_source_location(source, source_index)}"
        ),
        (
            f"    OP-LABEL conflict: method=fixed-runtime-label-check "
            f"params={{label={operand_label}, resolved=${resolved_addr:04X}, "
            f"expected=${expected_addr:04X}}}"
        ),
    ]


def build_indirect_jmp_follow_diagnostics(
    trace_entry: TraceEntry,
    source: list[SourceInstruction],
    current_source_index: int,
    effective_post_pc: int | None,
    target_source_index: int | None,
    label: str | None = None,
) -> list[str]:
    effective_post_pc_text = (
        f"${effective_post_pc:04X}" if effective_post_pc is not None else "none"
    )
    if target_source_index is not None:
        return [
            (
                f"  IJMP @{trace_entry.index} PC=${trace_entry.pc:04X} non-linear-follow: "
                f"current={format_source_location(source, current_source_index)}"
            ),
            (
                f"    IJMP decision: method=observed-target-pc "
                f"params={{effective-post-pc={effective_post_pc_text}, "
                f"label={label or 'none'}, "
                f"target={format_source_location(source, target_source_index)}}}"
            ),
        ]

    return [
        (
            f"  IJMP @{trace_entry.index} PC=${trace_entry.pc:04X} sync-conflict: "
            f"current={format_source_location(source, current_source_index)}"
        ),
        (
            f"    IJMP decision: method=linear-fallback "
            f"params={{effective-post-pc={effective_post_pc_text}, "
            f"next-source={format_source_location(source, current_source_index + 1)}}}"
        ),
    ]


def build_annotated_line(
    base_line: str,
    new_names_here: list[str],
    new_operand_labels: list[str] | None = None,
) -> str:
    suffixes: list[str] = []
    if new_names_here:
        suffixes.append(f"NEW_PC_LABELS: {', '.join(new_names_here)}")
    if new_operand_labels:
        suffixes.append(f"NEW_OP_LABELS: {', '.join(new_operand_labels)}")

    if not suffixes:
        return base_line

    return f"{base_line} ; {' ; '.join(suffixes)}"


def write_discovered_labels(
    out_path: Path,
    discovered: dict[int, set[str]],
    existing: dict[int, list[str]],
    discovered_data: dict[int, set[str]] | None = None,
) -> tuple[int, int, int]:
    entries_written = 0
    aliases_written = 0
    data_entries_written = 0

    lines: list[str] = []
    for addr in sorted(discovered.keys()):
        existing_names = set(existing.get(addr, []))
        new_names = sorted(
            name for name in discovered[addr] if name not in existing_names
        )
        if not new_names:
            continue
        for name in new_names:
            lines.append(f'{{0x{addr:04X}, "{name}", MonitorSymbolPc}},//')
            entries_written += 1
            if len(new_names) > 1:
                aliases_written += 1

    if discovered_data:
        data_lines: list[str] = []
        for addr in sorted(discovered_data.keys()):
            existing_names = set(existing.get(addr, []))
            # Skip names already emitted as PC labels at the same address.
            pc_names = {n for n in discovered.get(addr, set())}
            new_names = sorted(
                name
                for name in discovered_data[addr]
                if name not in existing_names and name not in pc_names
            )
            if not new_names:
                continue
            for name in new_names:
                data_lines.append(f'{{0x{addr:04X}, "{name}" }},//')
                data_entries_written += 1
        if data_lines:
            if lines:
                lines.append("// --- operand data labels ---")
            lines.extend(data_lines)

    out_path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    return entries_written, aliases_written, data_entries_written


def runtime_label_to_addr(runtime_label: object) -> int | None:
    if not isinstance(runtime_label, str):
        return None

    normalized = normalize_label_token(runtime_label)
    match = RUNTIME_LABEL_ADDR_RE.match(normalized)
    if match is None:
        return None
    return int(match.group(1), 16)


def coerce_event_int(value: object, default: int = -1) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        text = value.strip()
        if not text:
            return default
        if text.startswith("$"):
            text = "0x" + text[1:]
        try:
            return int(text, 0)
        except ValueError:
            return default
    return default


def dedupe_runtime_label_events(
    runtime_label_events: list[dict[str, object]],
) -> list[dict[str, object]]:
    deduped: list[dict[str, object]] = []
    seen: set[tuple[object, ...]] = set()

    for event in runtime_label_events:
        kind = str(event.get("kind", ""))
        trace_index = coerce_event_int(event.get("trace_index", -1), -1)
        trace_pc = coerce_event_int(event.get("trace_pc", -1), -1)
        runtime_label = str(event.get("runtime_label", ""))
        runtime_label_token = normalize_label_token(runtime_label)
        runtime_addr = runtime_label_to_addr(runtime_label)
        target_source_index = coerce_event_int(event.get("target_source_index", -1), -1)
        target_source_file = str(event.get("target_source_file", ""))
        target_source_line = coerce_event_int(event.get("target_source_line", -1), -1)

        # Keep encounter order stable while suppressing duplicate runtime-recovery records.
        dedupe_key = (
            kind,
            trace_pc,
            runtime_label_token,
            runtime_addr,
            target_source_index,
            target_source_file,
            target_source_line,
        )
        if dedupe_key in seen:
            continue
        seen.add(dedupe_key)

        deduped.append(
            {
                "kind": kind,
                "trace_index": trace_index,
                "trace_pc": trace_pc,
                "runtime_label": runtime_label,
                "runtime_label_token": runtime_label_token,
                "runtime_addr": runtime_addr,
                "target_source_index": target_source_index,
                "target_source_file": target_source_file,
                "target_source_line": target_source_line,
            }
        )

    return deduped


def write_runtime_label_report(
    out_path: Path,
    runtime_label_events: list[dict[str, object]],
) -> tuple[int, int]:
    deduped_events = dedupe_runtime_label_events(runtime_label_events)
    duplicate_count = len(runtime_label_events) - len(deduped_events)

    rows = [
        "kind\ttrace_index\ttrace_pc\truntime_label\truntime_label_token\truntime_addr\ttarget_source_index\ttarget_source_file\ttarget_source_line"
    ]
    for event in deduped_events:
        runtime_addr = event["runtime_addr"]
        runtime_addr_text = (
            f"${runtime_addr:04X}"
            if isinstance(runtime_addr, int) and runtime_addr >= 0
            else ""
        )
        rows.append(
            "\t".join(
                [
                    str(event["kind"]),
                    str(event["trace_index"]),
                    f"${coerce_event_int(event['trace_pc'], -1) & 0xFFFF:04X}",
                    str(event["runtime_label"]),
                    str(event["runtime_label_token"]),
                    runtime_addr_text,
                    str(event["target_source_index"]),
                    str(event["target_source_file"]),
                    str(event["target_source_line"]),
                ]
            )
        )

    out_path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return len(deduped_events), duplicate_count


def run_alignment(args: argparse.Namespace) -> int:
    source = parse_source_tree(args.source_root)
    if not source:
        print(f"No source instructions parsed under: {args.source_root}")
        return 1

    existing_symbols = parse_existing_monitor_symbols(args.symbols_file)
    existing_symbol_tokens_by_addr: dict[int, set[str]] = {
        addr: {normalize_label_token(name) for name in names}
        for addr, names in existing_symbols.items()
    }
    label_to_source_indexes: dict[str, list[int]] = defaultdict(list)
    for idx, inst in enumerate(source):
        if inst.label:
            label_to_source_indexes[normalize_label_token(inst.label)].append(idx)
        for alias in inst.aliases:
            label_to_source_indexes[normalize_label_token(alias)].append(idx)

    ngram_index = build_ngram_index(source, args.sync_window)
    if not ngram_index:
        print(
            "Could not build source n-gram index. Check --sync-window and source corpus size."
        )
        return 1

    discovered: dict[int, set[str]] = defaultdict(set)
    discovered_data: dict[int, set[str]] = defaultdict(set)
    processed = 0
    source_pos: int | None = None
    pending: Deque[TraceEntry] = deque()
    recent_trace: Deque[TraceEntry] = deque(maxlen=6)
    recent_source_indexes: Deque[int | None] = deque(maxlen=6)
    last_matched_source_index: int | None = None
    source_return_frames: list[tuple[int, int | None]] = []
    jsr_stack_associations: dict[int, int] = {}
    runtime_label_events: list[dict[str, object]] = []
    synced = False
    stop_reason = "eof"

    entry_iter = trace_entries(args.log)
    annotated_out = args.annotated_log.open("w", encoding="utf-8")
    try:
        while True:
            while len(pending) < max(args.sync_window, 1):
                if args.max_lines and processed + len(pending) >= args.max_lines:
                    stop_reason = "max-lines"
                    break
                try:
                    entry = next(entry_iter)
                    pending.append(with_fallback_pc_symbol(entry, existing_symbols))
                except StopIteration:
                    stop_reason = "eof"
                    break

            if not pending:
                break

            if not synced:
                sync_idx, issue = acquire_sync_window(
                    pending,
                    source,
                    ngram_index,
                    label_to_source_indexes,
                    args.sync_window,
                )
                if issue is not None:
                    if args.non_interactive:
                        print(f"Stopping: {issue.reason}: {issue.detail}")
                        stop_reason = issue.reason
                        break

                    action, value = prompt_for_help(
                        issue,
                        list(pending),
                        source,
                        source_pos,
                        label_to_source_indexes,
                        list(recent_trace),
                        last_matched_source_index,
                        list(recent_source_indexes),
                    )
                    if action == "quit":
                        stop_reason = issue.reason
                        break
                    if action == "skip":
                        recent_trace.append(pending.popleft())
                        recent_source_indexes.append(None)
                        continue
                    if action == "jump" and value is not None:
                        source_pos = value
                        source_return_frames.clear()
                        synced = True
                        continue
                    continue

                source_pos = sync_idx
                source_return_frames.clear()
                synced = True
                print(
                    f"Synced at source index {source_pos} using window size {args.sync_window}."
                )

            # Synced mode: consume one trace instruction at a time.
            assert source_pos is not None  # nosec B101
            if source_pos >= len(source):
                issue = SyncIssue(
                    "source-exhausted", "Reached end of source instruction stream."
                )
                if args.non_interactive:
                    stop_reason = issue.reason
                    break
                action, value = prompt_for_help(
                    issue,
                    list(pending),
                    source,
                    len(source) - 1,
                    label_to_source_indexes,
                    list(recent_trace),
                    last_matched_source_index,
                    list(recent_source_indexes),
                )
                if action == "quit":
                    stop_reason = issue.reason
                    break
                if action == "skip":
                    recent_trace.append(pending.popleft())
                    recent_source_indexes.append(None)
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    source_return_frames.clear()
                    continue
                continue

            trace_entry = pending[0]
            source_inst = source[source_pos]

            rts_alignment_issue = detect_rts_alignment_issue(
                trace_entry,
                source_inst,
                source_pos,
            )
            if rts_alignment_issue is not None:
                for debug_line in build_rts_assoc_diagnostics(
                    trace_entry, jsr_stack_associations, source
                ):
                    annotated_out.write(debug_line + "\n")

                if args.non_interactive:
                    print(
                        f"Stopping: {rts_alignment_issue.reason}: {rts_alignment_issue.detail}"
                    )
                    stop_reason = rts_alignment_issue.reason
                    break

                action, value = prompt_for_help(
                    rts_alignment_issue,
                    list(pending),
                    source,
                    source_pos,
                    label_to_source_indexes,
                    list(recent_trace),
                    last_matched_source_index,
                    list(recent_source_indexes),
                )
                if action == "quit":
                    stop_reason = rts_alignment_issue.reason
                    break
                if action == "skip":
                    recent_trace.append(pending.popleft())
                    recent_source_indexes.append(None)
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    source_return_frames.clear()
                    continue
                continue

            if not mnemonics_match(trace_entry.mnemonic, source_inst.mnemonic):
                indirect_jmp_resync_idx = attempt_indirect_jmp_next_trace_resync(
                    pending,
                    source,
                    label_to_source_indexes,
                    source_pos,
                    runtime_label_events=runtime_label_events,
                )
                indirect_jmp_conflict_log = build_indirect_jmp_conflict_diagnostics(
                    pending,
                    source,
                    source_pos,
                )
                if indirect_jmp_resync_idx is not None:
                    for debug_line in build_indirect_jmp_resync_diagnostics(
                        pending,
                        source,
                        source_pos,
                        indirect_jmp_resync_idx,
                    ):
                        annotated_out.write(debug_line + "\n")
                    recent_trace.append(pending.popleft())
                    recent_source_indexes.append(None)
                    processed += 1
                    source_pos = indirect_jmp_resync_idx
                    source_return_frames.clear()
                    continue

                pc_symbol_resync_idx = attempt_pc_symbol_resync(
                    trace_entry,
                    source,
                    label_to_source_indexes,
                    source_pos,
                    runtime_label_events=runtime_label_events,
                )
                if (
                    pc_symbol_resync_idx is not None
                    and pc_symbol_resync_idx != source_pos
                ):
                    source_pos = pc_symbol_resync_idx
                    source_return_frames.clear()
                    continue

                local_resync_idx = attempt_local_auto_resync(
                    pending,
                    source,
                    ngram_index,
                    label_to_source_indexes,
                    source_pos,
                    args.sync_window,
                )
                if local_resync_idx is not None and local_resync_idx != source_pos:
                    source_pos = local_resync_idx
                    source_return_frames.clear()
                    continue

                issue = SyncIssue(
                    "mnemonic-mismatch",
                    (
                        f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} has {trace_entry.mnemonic}, "
                        f"source[{source_pos}] has {source_inst.mnemonic} "
                        f"({source_inst.file_path}:{source_inst.line_number})."
                    ),
                )
                for debug_line in indirect_jmp_conflict_log:
                    annotated_out.write(debug_line + "\n")
                if args.non_interactive:
                    print(f"Stopping: {issue.reason}: {issue.detail}")
                    stop_reason = issue.reason
                    break

                action, value = prompt_for_help(
                    issue,
                    list(pending),
                    source,
                    source_pos,
                    label_to_source_indexes,
                    list(recent_trace),
                    last_matched_source_index,
                    list(recent_source_indexes),
                )
                if action == "quit":
                    stop_reason = issue.reason
                    break
                if action == "skip":
                    recent_trace.append(pending.popleft())
                    recent_source_indexes.append(None)
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    source_return_frames.clear()
                    continue
                continue

            new_names_here: list[str] = []
            if source_inst.label:
                source_label_token = normalize_label_token(source_inst.label)
                discovered_tokens_for_pc = {
                    normalize_label_token(name) for name in discovered[trace_entry.pc]
                }
                if (
                    source_label_token not in discovered_tokens_for_pc
                    and not is_pc_label_already_known(
                        source_inst.label,
                        trace_entry,
                        existing_symbol_tokens_by_addr.get(trace_entry.pc, set()),
                    )
                ):
                    discovered[trace_entry.pc].add(source_inst.label)
                    new_names_here = [source_inst.label]

            # Operand label discovery: extract a label from the source operand and
            # map it to the resolved numeric address in the trace operand.
            new_operand_label_strs: list[str] = []
            op_pairs = extract_operand_label_pairs(source_inst, trace_entry)
            operand_sync_issue: SyncIssue | None = None
            operand_sync_log: list[str] = []
            for op_label, op_addr, op_flag in op_pairs:
                fixed_addr = runtime_label_fixed_address(op_label)
                if fixed_addr is not None and fixed_addr != op_addr:
                    operand_sync_issue = SyncIssue(
                        "operand-label-address-mismatch",
                        (
                            f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} matched "
                            f"source[{source_pos}] {source_inst.file_path}:{source_inst.line_number}, "
                            f"but NEW_OP_LABELS candidate {op_label} resolved to ${op_addr:04X} "
                            f"(expected ${fixed_addr:04X} from label token)."
                        ),
                    )
                    operand_sync_log = build_operand_label_mismatch_diagnostics(
                        trace_entry,
                        source,
                        source_pos,
                        op_label,
                        op_addr,
                        fixed_addr,
                    )
                    break

                op_token = normalize_label_token(op_label)
                existing_at = existing_symbol_tokens_by_addr.get(op_addr, set())
                target_discovered = (
                    discovered if op_flag == "MonitorSymbolPc" else discovered_data
                )
                discovered_at = {
                    normalize_label_token(n)
                    for n in target_discovered.get(op_addr, set())
                }
                if op_token not in existing_at and op_token not in discovered_at:
                    target_discovered[op_addr].add(op_label)
                    new_operand_label_strs.append(f"{op_label}=${op_addr:04X}")

            if operand_sync_issue is not None:
                for debug_line in operand_sync_log:
                    annotated_out.write(debug_line + "\n")
                if args.non_interactive:
                    print(
                        f"Stopping: {operand_sync_issue.reason}: {operand_sync_issue.detail}"
                    )
                    stop_reason = operand_sync_issue.reason
                    break

                action, value = prompt_for_help(
                    operand_sync_issue,
                    list(pending),
                    source,
                    source_pos,
                    label_to_source_indexes,
                    list(recent_trace),
                    last_matched_source_index,
                    list(recent_source_indexes),
                )
                if action == "quit":
                    stop_reason = operand_sync_issue.reason
                    break
                if action == "skip":
                    recent_trace.append(pending.popleft())
                    recent_source_indexes.append(None)
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    source_return_frames.clear()
                    continue
                continue

            observed_next_pc = pending[1].pc if len(pending) > 1 else None
            rts_miss_log: list[str] = []
            jsr_assoc_log: list[str] = []
            indirect_jmp_log: list[str] = []
            control_flow_sync_issues: list[SyncIssue] = []
            next_source_pos = advance_source_position(
                trace_entry,
                source_pos,
                source,
                label_to_source_indexes,
                source_return_frames,
                observed_next_pc,
                existing_symbols,
                runtime_label_events,
                jsr_stack_associations,
                rts_miss_log,
                jsr_assoc_log,
                indirect_jmp_log,
                control_flow_sync_issues,
            )

            if control_flow_sync_issues:
                for debug_line in jsr_assoc_log:
                    annotated_out.write(debug_line + "\n")
                for debug_line in indirect_jmp_log:
                    annotated_out.write(debug_line + "\n")
                for debug_line in rts_miss_log:
                    annotated_out.write(debug_line + "\n")

                issue = control_flow_sync_issues[0]
                if args.non_interactive:
                    print(f"Stopping: {issue.reason}: {issue.detail}")
                    stop_reason = issue.reason
                    break

                action, value = prompt_for_help(
                    issue,
                    list(pending),
                    source,
                    source_pos,
                    label_to_source_indexes,
                    list(recent_trace),
                    last_matched_source_index,
                    list(recent_source_indexes),
                )
                if action == "quit":
                    stop_reason = issue.reason
                    break
                if action == "skip":
                    recent_trace.append(pending.popleft())
                    recent_source_indexes.append(None)
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    source_return_frames.clear()
                    continue
                continue

            last_matched_source_index = source_pos
            recent_trace.append(trace_entry)
            recent_source_indexes.append(source_pos)
            pending.popleft()
            source_pos = next_source_pos

            line_out = build_annotated_line(
                trace_entry.full_line,
                new_names_here,
                new_operand_label_strs,
            )
            for stack_line in trace_entry.pre_stack_lines:
                annotated_out.write(stack_line + "\n")
            for debug_line in jsr_assoc_log:
                annotated_out.write(debug_line + "\n")
            for debug_line in indirect_jmp_log:
                annotated_out.write(debug_line + "\n")
            for debug_line in rts_miss_log:
                annotated_out.write(debug_line + "\n")
            annotated_out.write(line_out + "\n")
            for stack_line in trace_entry.post_stack_lines:
                annotated_out.write(stack_line + "\n")
            processed += 1

            if args.max_lines and processed >= args.max_lines:
                stop_reason = "max-lines"
                break

    finally:
        annotated_out.close()

    entries_written, aliases_written, data_entries_written = write_discovered_labels(
        args.new_labels,
        discovered,
        existing_symbols,
        discovered_data,
    )
    runtime_events_written, runtime_event_duplicates = write_runtime_label_report(
        args.runtime_label_report,
        runtime_label_events,
    )

    print("\nAlignment summary")
    print(f"  processed trace instructions: {processed}")
    print(f"  stop reason: {stop_reason}")
    print(f"  annotated log: {args.annotated_log}")
    print(f"  new labels file: {args.new_labels}")
    print(f"  runtime-label report (TSV): {args.runtime_label_report}")
    print(f"  inserted-ready entries: {entries_written}")
    print(f"  alias entries (same address, additional names): {aliases_written}")
    print(f"  operand data label entries: {data_entries_written}")
    print(f"  runtime-label entries: {runtime_events_written}")
    print(f"  runtime-label duplicates removed: {runtime_event_duplicates}")

    if stop_reason not in {"eof", "max-lines"}:
        print("  NOTE: stopped at unresolved sync point (help required).")

    return 0


def run_self_check() -> None:
    sample_trace = (
        "@123 PC=$2000 OP=$A9 LDA #$01 ; PRE PC=$2000 A=$00 X=$00 Y=$00 "
        "SP=$FF P=$24 POST PC=$2002 A=$01 X=$00 Y=$00 SP=$FF P=$24"
    )
    parsed_trace = parse_log_line(sample_trace)
    assert parsed_trace is not None  # nosec B101
    assert parsed_trace.index == 123  # nosec B101
    assert parsed_trace.pc == 0x2000  # nosec B101
    assert parsed_trace.opcode == 0xA9  # nosec B101
    assert parsed_trace.mnemonic == "LDA"  # nosec B101
    assert parsed_trace.operand == "#$01"  # nosec B101
    assert parsed_trace.pre_pc == 0x2000  # nosec B101
    assert parsed_trace.post_pc == 0x2002  # nosec B101
    assert parsed_trace.pre_a == 0x00  # nosec B101
    assert parsed_trace.pre_x == 0x00  # nosec B101
    assert parsed_trace.pre_y == 0x00  # nosec B101
    assert parsed_trace.pre_sp == 0xFF  # nosec B101
    assert parsed_trace.pre_p == 0x24  # nosec B101

    mli_trace = parse_log_line(
        "@1 PC=$1000 OP=$20 MLI .byte $C8 .word $1234 (OPEN) ; PRE X POST X"
    )
    assert mli_trace is not None  # nosec B101
    assert mli_trace.mnemonic == "MLI"  # nosec B101

    no_operand_trace = parse_log_line(
        "@7 PC=$2002 OP=$9A TXS ; PRE PC=$2002 POST PC=$2003"
    )
    assert no_operand_trace is not None  # nosec B101
    assert no_operand_trace.operand == ""  # nosec B101

    pc_symbol_trace = parse_log_line(
        "@8 PC=$FC22 (LFC22) OP=$A5 LDA $25 ; PRE PC=$FC22 POST PC=$FC24"
    )
    assert pc_symbol_trace is not None  # nosec B101
    assert pc_symbol_trace.pc == 0xFC22  # nosec B101
    assert pc_symbol_trace.mnemonic == "LDA"  # nosec B101
    assert pc_symbol_trace.pc_symbol == "LFC22"  # nosec B101

    assert is_pc_label_already_known("LFC22", pc_symbol_trace, set())  # nosec B101
    assert is_pc_label_already_known("XFC22", pc_symbol_trace, set())  # nosec B101
    assert not is_pc_label_already_known("LFC23", pc_symbol_trace, set())  # nosec B101
    assert is_pc_label_already_known(  # nosec B101
        "DelayLup",
        TraceEntry(
            index=9,
            pc=0x7A25,
            opcode=0xA9,
            mnemonic="LDA",
            operand="#$00",
            full_line="@9 PC=$7A25 OP=$A9 LDA #$00",
            pre_pc=0x7A25,
            post_pc=0x7A27,
        ),
        {"DELAYLUP"},
    )

    parsed_stack_dump = parse_stack_dump_line(
        "  STACK SP=$FD USED=2: $01FE=$12 $01FF=$25"
    )
    assert parsed_stack_dump is not None  # nosec B101
    assert parsed_stack_dump.stack_bytes == (0x12, 0x25)  # nosec B101
    assert parsed_stack_dump.phase is None  # nosec B101
    empty_stack_dump = parse_stack_dump_line("  STACK SP=$FF EMPTY")
    assert empty_stack_dump is not None  # nosec B101
    assert empty_stack_dump.stack_bytes == ()  # nosec B101

    tagged_stack_dump = parse_stack_dump_line(
        "  STACK META[INSN=3 PHASE=PRE OP=$60 PC=$4001 SP=$FD] SP=$FD USED=2: "
        "$01FE=$02 $01FF=$30"
    )
    assert tagged_stack_dump is not None  # nosec B101
    assert tagged_stack_dump.instruction_index == 3  # nosec B101
    assert tagged_stack_dump.phase == "PRE"  # nosec B101
    assert tagged_stack_dump.opcode == 0x60  # nosec B101
    assert tagged_stack_dump.pc == 0x4001  # nosec B101
    assert tagged_stack_dump.sp == 0xFD  # nosec B101
    assert tagged_stack_dump.stack_bytes == (0x02, 0x30)  # nosec B101

    with tempfile.TemporaryDirectory() as temp_dir:
        log_file = Path(temp_dir) / "trace.log"
        log_file.write_text(
            "\n".join(
                [
                    "@1 PC=$3000 OP=$20 JSR $4000 ; PRE PC=$3000 POST PC=$4000",
                    "  STACK META[INSN=1 PHASE=POST OP=$20 PC=$3000 SP=$FD] SP=$FD USED=2: "
                    "$01FE=$02 $01FF=$30",
                    "@2 PC=$4000 OP=$EA NOP ; PRE PC=$4000 POST PC=$4001",
                    "  STACK META[INSN=3 PHASE=PRE OP=$60 PC=$4001 SP=$FD] SP=$FD USED=2: "
                    "$01FE=$02 $01FF=$30",
                    "@3 PC=$4001 OP=$60 RTS ; PRE PC=$4001 POST PC=$3003",
                ]
            )
            + "\n",
            encoding="utf-8",
        )
        parsed_entries = list(trace_entries(log_file))
        assert len(parsed_entries) == 3  # nosec B101
        assert parsed_entries[0].mnemonic == "JSR"  # nosec B101
        assert parsed_entries[0].pre_stack_bytes is None  # nosec B101
        assert parsed_entries[0].post_stack_bytes == (0x02, 0x30)  # nosec B101
        assert parsed_entries[0].pre_stack_lines == ()  # nosec B101
        assert parsed_entries[0].post_stack_lines == (  # nosec B101
            "  STACK META[INSN=1 PHASE=POST OP=$20 PC=$3000 SP=$FD] SP=$FD USED=2: "
            "$01FE=$02 $01FF=$30",
        )
        assert parsed_entries[1].mnemonic == "NOP"  # nosec B101
        assert parsed_entries[1].pre_stack_bytes is None  # nosec B101
        assert parsed_entries[1].pre_stack_lines == ()  # nosec B101
        assert parsed_entries[1].post_stack_lines == ()  # nosec B101
        assert parsed_entries[2].mnemonic == "RTS"  # nosec B101
        assert parsed_entries[2].pre_stack_bytes == (0x02, 0x30)  # nosec B101
        assert parsed_entries[2].pre_stack_lines == (  # nosec B101
            "  STACK META[INSN=3 PHASE=PRE OP=$60 PC=$4001 SP=$FD] SP=$FD USED=2: "
            "$01FE=$02 $01FF=$30",
        )
        assert parsed_entries[2].post_stack_lines == ()  # nosec B101

    fallback_entry = TraceEntry(
        index=9,
        pc=0xFC22,
        opcode=0xA5,
        mnemonic="LDA",
        operand="$25",
        full_line="@9 PC=$FC22 OP=$A5 LDA $25",
        pre_pc=None,
        post_pc=None,
    )
    fallback_symbols = {0xFC22: ["LFC22", "ALIAS"]}
    enriched = with_fallback_pc_symbol(fallback_entry, fallback_symbols)
    assert enriched.pc_symbol == "LFC22"  # nosec B101

    help_issue = SyncIssue("mnemonic-mismatch", "Need operator review")
    help_recent_trace = [
        TraceEntry(100, 0x1FF6, 0xA9, "LDA", "#$00", "", None, None),
        TraceEntry(101, 0x1FF8, 0x8D, "STA", "$3000", "", None, None),
        TraceEntry(102, 0x1FFB, 0xEA, "NOP", "", "", None, None),
        TraceEntry(103, 0x1FFC, 0xA2, "LDX", "#$04", "", None, None),
        TraceEntry(104, 0x1FFE, 0x86, "STX", "$20", "", None, None),
        TraceEntry(105, 0x2000, 0xA0, "LDY", "#$01", "", None, None),
    ]
    help_pending_trace = [
        TraceEntry(106, 0x2006, 0xC9, "CMP", "#$10", "", None, None),
        TraceEntry(107, 0x2008, 0xD0, "BNE", "Next", "", None, None),
    ]
    help_source = [
        SourceInstruction("Monitor.S", 905, "Older0", "CLC", ""),
        SourceInstruction("Monitor.S", 906, None, "LDA", "#$00"),
        SourceInstruction("Monitor.S", 907, None, "STA", "$3000"),
        SourceInstruction("Monitor.S", 908, None, "LDX", "#$04"),
        SourceInstruction("Monitor.S", 909, None, "STX", "$20"),
        SourceInstruction("Monitor.S", 910, None, "LDY", "#$01"),
        SourceInstruction("Monitor.S", 911, "LastGood", "CMP", "#$08"),
        SourceInstruction("Monitor.S", 912, None, "CMP", "#$10"),
        SourceInstruction("Monitor.S", 913, "Next", "STY", "$20"),
    ]
    help_labels: dict[str, list[int]] = defaultdict(list)
    for idx, inst in enumerate(help_source):
        if inst.label:
            help_labels[inst.label.upper()].append(idx)

    prompt_output = io.StringIO()
    with patch("builtins.input", side_effect=["q"]), redirect_stdout(prompt_output):
        action, value = prompt_for_help(
            help_issue,
            help_pending_trace,
            help_source,
            8,
            help_labels,
            help_recent_trace,
            6,
        )
    assert action == "quit"  # nosec B101
    assert value is None  # nosec B101

    prompt_text = prompt_output.getvalue()
    assert prompt_text.count("\nLegend:\n") == 1  # nosec B101
    legend_pos = prompt_text.index("=> current trace/source candidate")
    pending_pos = prompt_text.index("Pending trace entries:")
    assert legend_pos < pending_pos  # nosec B101
    assert (  # nosec B101
        prompt_text.count(">> previous trace entry / last matched source instruction")
        == 1
    )
    assert prompt_text.count(">> @100 PC=$1FF6 OP=$A9 LDA #$00") == 1  # nosec B101
    assert prompt_text.count(">> @101 PC=$1FF8 OP=$8D STA $3000") == 1  # nosec B101
    assert prompt_text.count(">> @102 PC=$1FFB OP=$EA NOP") == 1  # nosec B101
    assert prompt_text.count(">> @103 PC=$1FFC OP=$A2 LDX #$04") == 1  # nosec B101
    assert prompt_text.count(">> @104 PC=$1FFE OP=$86 STX $20") == 1  # nosec B101
    assert prompt_text.count(">> @105 PC=$2000 OP=$A0 LDY #$01") == 1  # nosec B101
    assert prompt_text.count("=> @106 PC=$2006 OP=$C9 CMP #$10") == 1  # nosec B101
    assert "[0] Monitor.S:905 Older0 CLC" not in prompt_text  # nosec B101
    assert "[1] Monitor.S:906 LDA #$00" not in prompt_text  # nosec B101
    assert "[2] Monitor.S:907 STA $3000" in prompt_text  # nosec B101
    assert "[5] Monitor.S:910 LDY #$01" in prompt_text  # nosec B101
    assert ">> [6] Monitor.S:911 LastGood CMP #$08" in prompt_text  # nosec B101
    assert "=> [8] Monitor.S:913 Next STY $20" in prompt_text  # nosec B101
    assert "(prev)" not in prompt_text  # nosec B101
    assert "=> current source candidate" not in prompt_text  # nosec B101
    assert ">> last matched source instruction" not in prompt_text  # nosec B101

    prompt_output_with_prev = io.StringIO()
    with patch("builtins.input", side_effect=["q"]), redirect_stdout(
        prompt_output_with_prev
    ):
        prompt_for_help(
            help_issue,
            help_pending_trace,
            help_source,
            8,
            help_labels,
            help_recent_trace,
            6,
            recent_source_indexes=[0, 1, 2, 3, 4, 5],
        )
    prompt_text_with_prev = prompt_output_with_prev.getvalue()
    assert (
        ">> [0] Monitor.S:905 Older0 CLC [previous-match]" in prompt_text_with_prev
    )  # nosec B101
    assert (
        ">> [1] Monitor.S:906 LDA #$00 [previous-match]" in prompt_text_with_prev
    )  # nosec B101

    retained_recent_trace: Deque[TraceEntry] = deque(maxlen=6)
    for index in range(7):
        retained_recent_trace.append(
            TraceEntry(index, 0x2000 + index, 0xEA, "NOP", "", "", None, None)
        )
    assert [entry.index for entry in retained_recent_trace] == [
        1,
        2,
        3,
        4,
        5,
        6,
    ]  # nosec B101

    assert derive_rts_return_pc_from_trace_stack(None) is None  # nosec B101
    assert derive_rts_return_pc_from_trace_stack([0x99]) is None  # nosec B101
    assert derive_rts_return_pc_from_trace_stack([0xD5, 0x99]) == 0x99D6  # nosec B101

    annotated_plain = build_annotated_line(
        "@1 PC=$3000 OP=$EA NOP",
        [],
    )
    assert annotated_plain == "@1 PC=$3000 OP=$EA NOP"  # nosec B101

    annotated_rts = build_annotated_line(
        "@2 PC=$3001 OP=$60 RTS",
        ["LB001"],
    )
    assert "NEW_PC_LABELS: LB001" in annotated_rts  # nosec B101

    annotated_push = build_annotated_line(
        "@3 PC=$3002 OP=$20 JSR $4000",
        [],
    )
    assert annotated_push == "@3 PC=$3002 OP=$20 JSR $4000"  # nosec B101

    # NEW_PC_LABELS should only include labels discovered on this instruction,
    # not all labels ever discovered for this PC (avoids loop spam).
    annotated_repeat_pc = build_annotated_line(
        "@4 PC=$FCA9 OP=$48 PHA",
        [],
    )
    assert "NEW_PC_LABELS:" not in annotated_repeat_pc  # nosec B101

    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        src_dir = root / "EDASM.SRC"
        src_dir.mkdir()
        source_file = src_dir / "ASM.S"
        source_file.write_text(
            """ ORG $7800
Start LDA #$00
 STA $2000
Loop BBR0 Next
Next RTS
""",
            encoding="utf-8",
        )

        parsed_source = parse_source_tree(src_dir)
        assert len(parsed_source) == 4  # nosec B101
        assert parsed_source[0].label == "Start"  # nosec B101
        assert parsed_source[2].mnemonic == "BBR"  # nosec B101

        branch_label = branch_label_from_source(parsed_source[2])
        assert branch_label == "Next"  # nosec B101

        equ_file = src_dir / "EQUSTAR.S"
        equ_file.write_text(
            """Alias EQU *
Labeled LDA #$01
""",
            encoding="utf-8",
        )
        parsed_source = parse_source_tree(src_dir)
        equ_inst = next(inst for inst in parsed_source if inst.label == "Labeled")
        assert "Alias" in equ_inst.aliases  # nosec B101

        call_source = [
            SourceInstruction("main.s", 1, "Main", "JSR", "Worker"),
            SourceInstruction("main.s", 2, "L2003", "LDA", "#$01"),
            SourceInstruction("main.s", 3, None, "STA", "$2000"),
            SourceInstruction("worker.s", 1, "Worker", "PHA", ""),
            SourceInstruction("worker.s", 2, None, "RTS", ""),
        ]
        call_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(call_source):
            if inst.label:
                call_labels[inst.label.upper()].append(idx)

        jsr_trace = TraceEntry(
            index=10,
            pc=0x2000,
            opcode=0x20,
            mnemonic="JSR",
            operand="$3000",
            full_line="@10 PC=$2000 OP=$20 JSR $3000",
            pre_pc=0x2000,
            post_pc=0x3000,
        )
        next_pos = advance_source_position(
            jsr_trace,
            0,
            call_source,
            call_labels,
            runtime_label_events=[],
        )
        assert next_pos == 3  # nosec B101

        # Do not step into callee when JSR appears to return immediately.
        jsr_no_step_trace = TraceEntry(
            index=10,
            pc=0x2000,
            opcode=0x20,
            mnemonic="JSR",
            operand="$3000",
            full_line="@10 PC=$2000 OP=$20 JSR $3000",
            pre_pc=0x2000,
            post_pc=0x2003,
        )
        jsr_no_step_issues: list[SyncIssue] = []
        no_step_pos = advance_source_position(
            jsr_no_step_trace,
            0,
            call_source,
            call_labels,
            runtime_label_events=[],
            control_flow_sync_issues=jsr_no_step_issues,
        )
        assert no_step_pos == 1  # nosec B101
        assert len(jsr_no_step_issues) == 1  # nosec B101
        assert (
            jsr_no_step_issues[0].reason == "jsr-linear-fallback-rejected"
        )  # nosec B101

        # Non-runtime JSR should not emit any runtime label events or push a return frame.
        jsr_no_step_frames: list[tuple[int, int | None]] = []
        jsr_no_step_events: list[dict[str, object]] = []
        jsr_no_step_issue_with_events: list[SyncIssue] = []
        no_step_pos_with_events = advance_source_position(
            jsr_no_step_trace,
            0,
            call_source,
            call_labels,
            return_frames=jsr_no_step_frames,
            runtime_label_events=jsr_no_step_events,
            control_flow_sync_issues=jsr_no_step_issue_with_events,
        )
        assert no_step_pos_with_events == 1  # nosec B101
        assert jsr_no_step_frames == []  # nosec B101
        assert jsr_no_step_events == []  # nosec B101
        assert len(jsr_no_step_issue_with_events) == 1  # nosec B101

        rts_trace = TraceEntry(
            index=11,
            pc=0x3001,
            opcode=0x60,
            mnemonic="RTS",
            operand="",
            full_line="@11 PC=$3001 OP=$60 RTS",
            pre_pc=0x3001,
            post_pc=0x2003,
        )
        return_frames: list[tuple[int, int | None]] = []
        jsr_non_runtime_events: list[dict[str, object]] = []
        next_pos = advance_source_position(
            jsr_trace,
            0,
            call_source,
            call_labels,
            return_frames=return_frames,
            runtime_label_events=jsr_non_runtime_events,
        )
        assert next_pos == 3  # nosec B101
        assert return_frames == [(1, 0x2003)]  # nosec B101
        assert jsr_non_runtime_events == []  # nosec B101

        # JSR should record a stack-top association and assoc diagnostic line.
        jsr_assoc_map: dict[int, int] = {}
        jsr_assoc_lines: list[str] = []
        _ = advance_source_position(
            jsr_trace,
            0,
            call_source,
            call_labels,
            return_frames=[],
            runtime_label_events=[],
            jsr_stack_associations=jsr_assoc_map,
            jsr_assoc_log=jsr_assoc_lines,
        )
        assert jsr_assoc_map.get(0x2002) == 1  # nosec B101
        assert any("assoc:" in line for line in jsr_assoc_lines)  # nosec B101

        rts_non_runtime_events: list[dict[str, object]] = []
        next_pos = advance_source_position(
            rts_trace,
            4,
            call_source,
            call_labels,
            return_frames=return_frames,
            runtime_label_events=rts_non_runtime_events,
        )
        assert next_pos == 1  # nosec B101
        assert return_frames == []  # nosec B101
        assert rts_non_runtime_events == []  # nosec B101

        # RTS should use stack-top association before fallback label logic.
        persistent_assoc_map = {0x2002: 1}
        rts_assoc_pos = advance_source_position(
            TraceEntry(
                index=18,
                pc=0x3001,
                opcode=0x60,
                mnemonic="RTS",
                operand="",
                full_line="@18 PC=$3001 OP=$60 RTS",
                pre_pc=0x3001,
                post_pc=0x9999,
                pre_stack_bytes=(0x02, 0x20),
            ),
            4,
            call_source,
            call_labels,
            jsr_stack_associations=persistent_assoc_map,
        )
        assert rts_assoc_pos == 1  # nosec B101
        assert persistent_assoc_map.get(0x2002) == 1  # nosec B101

        # RTS should emit non-assoc diagnostics when association lookup misses.
        rts_non_assoc_lines: list[str] = []
        _ = advance_source_position(
            TraceEntry(
                index=19,
                pc=0x3001,
                opcode=0x60,
                mnemonic="RTS",
                operand="",
                full_line="@19 PC=$3001 OP=$60 RTS",
                pre_pc=0x3001,
                post_pc=0x2003,
                pre_stack_bytes=(0x03, 0x20),
            ),
            4,
            call_source,
            call_labels,
            jsr_stack_associations={0x2002: 1, 0x2005: 3, 0x2001: 3},
            rts_miss_log=rts_non_assoc_lines,
        )
        assert any("non-assoc:" in line for line in rts_non_assoc_lines)  # nosec B101
        assert any(
            "looked-for=$2003" in line for line in rts_non_assoc_lines
        )  # nosec B101
        assert any(  # nosec B101
            "known-positions=[$2001->worker.s:1, $2002->main.s:2, $2005->worker.s:1]"
            in line
            for line in rts_non_assoc_lines
        )
        assert any(  # nosec B101
            "known-assocs-by-position: {main.s:2<-$2002, worker.s:1<-$2001, worker.s:1<-$2005}"
            in line
            for line in rts_non_assoc_lines
        )

        # If post_pc is present and cannot be resolved to a source label, keep linear advance.
        rts_unmatched_trace = TraceEntry(
            index=12,
            pc=0x3001,
            opcode=0x60,
            mnemonic="RTS",
            operand="",
            full_line="@12 PC=$3001 OP=$60 RTS",
            pre_pc=0x3001,
            post_pc=0x2222,
        )
        rts_unmatched_issues: list[SyncIssue] = []
        unmatched_pos = advance_source_position(
            rts_unmatched_trace,
            4,
            call_source,
            call_labels,
            control_flow_sync_issues=rts_unmatched_issues,
        )
        assert unmatched_pos == 5  # nosec B101
        assert len(rts_unmatched_issues) == 1  # nosec B101
        assert (
            rts_unmatched_issues[0].reason == "rts-linear-fallback-rejected"
        )  # nosec B101

        # Phase 3: branch fallback should use observed next PC when post_pc is missing.
        loop_source = [
            SourceInstruction("loop.s", 1, "Loop", "STA", "$0100,X"),
            SourceInstruction("loop.s", 2, None, "DEX", ""),
            SourceInstruction("loop.s", 3, None, "BNE", "Loop"),
            SourceInstruction("loop.s", 4, None, "LDY", "#$19"),
        ]
        loop_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(loop_source):
            if inst.label:
                loop_labels[inst.label.upper()].append(idx)

        branch_taken_missing_post = TraceEntry(
            index=13,
            pc=0x200B,
            opcode=0xD0,
            mnemonic="BNE",
            operand="$2007",
            full_line="@13 PC=$200B OP=$D0 BNE $2007",
            pre_pc=0x200B,
            post_pc=None,
        )
        taken_pos = advance_source_position(
            branch_taken_missing_post,
            2,
            loop_source,
            loop_labels,
            observed_next_pc=0x2007,
        )
        assert taken_pos == 0  # nosec B101

        branch_not_taken_missing_post = TraceEntry(
            index=14,
            pc=0x200B,
            opcode=0xD0,
            mnemonic="BNE",
            operand="$2007",
            full_line="@14 PC=$200B OP=$D0 BNE $2007",
            pre_pc=0x200B,
            post_pc=None,
        )
        not_taken_pos = advance_source_position(
            branch_not_taken_missing_post,
            2,
            loop_source,
            loop_labels,
            observed_next_pc=0x200D,
        )
        assert not_taken_pos == 3  # nosec B101

        # Phase 3: JSR fallback should use observed next PC when post_pc is missing.
        jsr_missing_post_return = TraceEntry(
            index=15,
            pc=0x2000,
            opcode=0x20,
            mnemonic="JSR",
            operand="$3000",
            full_line="@15 PC=$2000 OP=$20 JSR $3000",
            pre_pc=0x2000,
            post_pc=None,
        )
        jsr_missing_post_return_issues: list[SyncIssue] = []
        jsr_missing_post_pos = advance_source_position(
            jsr_missing_post_return,
            0,
            call_source,
            call_labels,
            observed_next_pc=0x2003,
            control_flow_sync_issues=jsr_missing_post_return_issues,
        )
        assert jsr_missing_post_pos == 1  # nosec B101
        assert len(jsr_missing_post_return_issues) == 1  # nosec B101
        assert (
            jsr_missing_post_return_issues[0].reason == "jsr-linear-fallback-rejected"
        )  # nosec B101

        jsr_missing_post_step = TraceEntry(
            index=16,
            pc=0x2000,
            opcode=0x20,
            mnemonic="JSR",
            operand="$3000",
            full_line="@16 PC=$2000 OP=$20 JSR $3000",
            pre_pc=0x2000,
            post_pc=None,
        )
        jsr_missing_post_step_pos = advance_source_position(
            jsr_missing_post_step,
            0,
            call_source,
            call_labels,
            observed_next_pc=0x3000,
        )
        assert jsr_missing_post_step_pos == 3  # nosec B101

        # Phase 1: capture runtime-required JSR fallback success via runtime symbols.
        jsr_runtime_fallback_trace = TraceEntry(
            index=160,
            pc=0x2000,
            opcode=0x20,
            mnemonic="JSR",
            operand="$3000",
            full_line="@160 PC=$2000 OP=$20 JSR $3000",
            pre_pc=0x2000,
            post_pc=0x3000,
        )
        jsr_runtime_fallback_source = [
            SourceInstruction("jsr_runtime.s", 1, "Main", "JSR", "$3000"),
            SourceInstruction("jsr_runtime.s", 2, "AfterMain", "NOP", ""),
            SourceInstruction("jsr_runtime.s", 3, "Worker", "RTS", ""),
        ]
        jsr_runtime_fallback_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(jsr_runtime_fallback_source):
            if inst.label:
                jsr_runtime_fallback_labels[inst.label.upper()].append(idx)
        jsr_runtime_events: list[dict[str, object]] = []
        jsr_runtime_fallback_pos = advance_source_position(
            jsr_runtime_fallback_trace,
            0,
            jsr_runtime_fallback_source,
            jsr_runtime_fallback_labels,
            runtime_symbols_by_addr={0x3000: ["Worker"]},
            runtime_label_events=jsr_runtime_events,
        )
        assert jsr_runtime_fallback_pos == 2  # nosec B101
        assert len(jsr_runtime_events) == 1  # nosec B101
        assert jsr_runtime_events[0]["kind"] == "jsr-runtime-fallback"  # nosec B101
        assert jsr_runtime_events[0]["runtime_label"] == "Worker"  # nosec B101
        assert jsr_runtime_events[0]["target_source_index"] == 2  # nosec B101

        mli_jsr_source = [
            SourceInstruction("mli.s", 42, None, "JSR", "PRODOS8"),
            SourceInstruction("mli.s", 46, None, "LDA", "ROMIN2"),
        ]
        mli_jsr_labels: dict[str, list[int]] = defaultdict(list)
        mli_jsr_issues: list[SyncIssue] = []
        mli_jsr_events: list[dict[str, object]] = []
        mli_jsr_pos = advance_source_position(
            TraceEntry(
                index=969,
                pc=0x2031,
                opcode=0x20,
                mnemonic="MLI",
                operand=".byte $82 .word $2035 (GET_TIME)",
                full_line="@969 PC=$2031 OP=$20 MLI .byte $82 .word $2035 (GET_TIME)",
                pre_pc=0x2031,
                post_pc=0x2037,
            ),
            0,
            mli_jsr_source,
            mli_jsr_labels,
            runtime_label_events=mli_jsr_events,
            control_flow_sync_issues=mli_jsr_issues,
        )
        assert mli_jsr_pos == 1  # nosec B101
        assert mli_jsr_issues == []  # nosec B101
        assert mli_jsr_events == []  # nosec B101

        # Phase 3: RTS fallback should match runtime label via observed next PC.
        rts_missing_post_trace = TraceEntry(
            index=17,
            pc=0x3001,
            opcode=0x60,
            mnemonic="RTS",
            operand="",
            full_line="@17 PC=$3001 OP=$60 RTS",
            pre_pc=0x3001,
            post_pc=None,
        )
        rts_missing_post_pos = advance_source_position(
            rts_missing_post_trace,
            4,
            call_source,
            call_labels,
            observed_next_pc=0x2003,
        )
        assert rts_missing_post_pos == 1  # nosec B101

        # Phase 1: capture runtime-required RTS fallback success when stack assoc miss.
        rts_runtime_events: list[dict[str, object]] = []
        rts_runtime_pos = advance_source_position(
            rts_trace,
            4,
            call_source,
            call_labels,
            runtime_label_events=rts_runtime_events,
        )
        assert rts_runtime_pos == 1  # nosec B101
        assert len(rts_runtime_events) == 1  # nosec B101
        assert rts_runtime_events[0]["kind"] == "rts-runtime-fallback"  # nosec B101
        assert rts_runtime_events[0]["runtime_label"] == "L2003"  # nosec B101
        assert rts_runtime_events[0]["target_source_index"] == 1  # nosec B101

        # Phase 2: advance_source_position RTS path uses trace stack bytes over observed
        # post_pc when pre_stack_bytes is available.
        rts_trace_stack_priority_trace = TraceEntry(
            index=18,
            pc=0x3001,
            opcode=0x60,
            mnemonic="RTS",
            operand="",
            full_line="@18 PC=$3001 OP=$60 RTS",
            pre_pc=0x3001,
            post_pc=0x9999,  # observed post_pc would not match any frame
            pre_stack_bytes=(0x02, 0x20),  # derives to 0x2003
        )
        rts_stack_priority_pos = advance_source_position(
            rts_trace_stack_priority_trace,
            4,
            call_source,
            call_labels,
        )
        assert rts_stack_priority_pos == 1  # nosec B101

        # Phase 2: detect_rts_alignment_issue returns mismatch-significant issue when
        # trace stack bytes are missing or insufficient.
        rts_source = [SourceInstruction("dispatch.s", 1, None, "RTS", "")]

        rts_stack_mismatch_issue = detect_rts_alignment_issue(
            TraceEntry(
                index=103,
                pc=0xB262,
                opcode=0x60,
                mnemonic="RTS",
                operand="",
                full_line="@103 PC=$B262 OP=$60 RTS",
                pre_pc=0xB262,
                post_pc=0x7A2D,
            ),
            rts_source[0],
            0,
        )
        assert rts_stack_mismatch_issue is not None  # nosec B101
        assert (
            rts_stack_mismatch_issue.reason == "rts-insufficient-trace-stack"
        )  # nosec B101

        rts_trace_stack_match_issue = detect_rts_alignment_issue(
            TraceEntry(
                index=103,
                pc=0xB262,
                opcode=0x60,
                mnemonic="RTS",
                operand="",
                full_line="@103 PC=$B262 OP=$60 RTS",
                pre_pc=0xB262,
                post_pc=0x7A2D,
                pre_stack_bytes=(0x2C, 0x7A),
            ),
            rts_source[0],
            0,
        )
        assert rts_trace_stack_match_issue is None  # nosec B101

        rts_trace_stack_mismatch_issue = detect_rts_alignment_issue(
            TraceEntry(
                index=103,
                pc=0xB262,
                opcode=0x60,
                mnemonic="RTS",
                operand="",
                full_line="@103 PC=$B262 OP=$60 RTS",
                pre_pc=0xB262,
                post_pc=0x7A2D,
                pre_stack_bytes=(0xE9, 0x7E),
            ),
            rts_source[0],
            0,
        )
        assert rts_trace_stack_mismatch_issue is not None  # nosec B101
        assert (
            rts_trace_stack_mismatch_issue.reason == "rts-return-mismatch"
        )  # nosec B101
        assert "$7EEA" in rts_trace_stack_mismatch_issue.detail  # nosec B101

        pull_source = [SourceInstruction("pull.s", 1, None, "PLA", "")]
        pull_labels: dict[str, list[int]] = defaultdict(list)
        pla_trace = TraceEntry(
            index=103,
            pc=0x3000,
            opcode=0x68,
            mnemonic="PLA",
            operand="",
            full_line="@103 PC=$3000 OP=$68 PLA",
            pre_pc=0x3000,
            post_pc=0x3001,
        )
        pla_pos = advance_source_position(
            pla_trace,
            0,
            pull_source,
            pull_labels,
        )
        assert pla_pos == 1  # nosec B101

        # Follow unconditional JMP labels.
        jmp_source = [
            SourceInstruction("jmp.s", 1, "L1000", "JMP", "L2000"),
            SourceInstruction("jmp.s", 2, None, "NOP", ""),
            SourceInstruction("jmp.s", 3, "L2000", "LDA", "#$01"),
        ]
        jmp_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(jmp_source):
            if inst.label:
                jmp_labels[inst.label.upper()].append(idx)

        jmp_trace = TraceEntry(
            index=18,
            pc=0x1000,
            opcode=0x4C,
            mnemonic="JMP",
            operand="$2000",
            full_line="@18 PC=$1000 OP=$4C JMP $2000",
            pre_pc=0x1000,
            post_pc=0x2000,
        )
        jmp_pos = advance_source_position(
            jmp_trace,
            0,
            jmp_source,
            jmp_labels,
            observed_next_pc=0x2000,
        )
        assert jmp_pos == 2  # nosec B101

        indirect_follow_source = [
            SourceInstruction("ijmp_follow.s", 1, "COUT", "JMP", "(CSWL)"),
            SourceInstruction("ijmp_follow.s", 2, "COUT1", "CMP", "#$A0"),
            SourceInstruction("ijmp_follow.s", 3, "LB3D9", "CMP", "#$E0"),
        ]
        indirect_follow_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(indirect_follow_source):
            if inst.label:
                indirect_follow_labels[inst.label.upper()].append(idx)

        indirect_follow_log: list[str] = []
        indirect_follow_events: list[dict[str, object]] = []
        indirect_follow_pos = advance_source_position(
            TraceEntry(
                index=109701,
                pc=0xFDED,
                opcode=0x6C,
                mnemonic="JMP",
                operand="(0036)",
                full_line="@109701 PC=$FDED OP=$6C JMP (0036)",
                pre_pc=0xFDED,
                post_pc=0xB3D9,
            ),
            0,
            indirect_follow_source,
            indirect_follow_labels,
            runtime_symbols_by_addr={0xB3D9: ["LB3D9"]},
            runtime_label_events=indirect_follow_events,
            indirect_jmp_log=indirect_follow_log,
        )
        assert indirect_follow_pos == 2  # nosec B101
        assert any(
            "IJMP decision: method=observed-target-pc" in line
            for line in indirect_follow_log
        )  # nosec B101
        assert any("label=LB3D9" in line for line in indirect_follow_log)  # nosec B101
        assert len(indirect_follow_events) == 1  # nosec B101
        assert (
            indirect_follow_events[0]["kind"] == "jmp-indirect-runtime-fallback"
        )  # nosec B101
        assert indirect_follow_events[0]["target_source_index"] == 2  # nosec B101

        unresolved_indirect_log: list[str] = []
        unresolved_indirect_issues: list[SyncIssue] = []
        unresolved_indirect_pos = advance_source_position(
            TraceEntry(
                index=109702,
                pc=0xFDED,
                opcode=0x6C,
                mnemonic="JMP",
                operand="(0036)",
                full_line="@109702 PC=$FDED OP=$6C JMP (0036)",
                pre_pc=0xFDED,
                post_pc=0xB3DA,
            ),
            0,
            indirect_follow_source,
            indirect_follow_labels,
            indirect_jmp_log=unresolved_indirect_log,
            control_flow_sync_issues=unresolved_indirect_issues,
        )
        assert unresolved_indirect_pos == 1  # nosec B101
        assert len(unresolved_indirect_issues) == 1  # nosec B101
        assert (
            unresolved_indirect_issues[0].reason == "indirect-jmp-unresolved-target"
        )  # nosec B101
        assert any(
            "IJMP decision: method=linear-fallback" in line
            for line in unresolved_indirect_log
        )  # nosec B101

        unresolved_direct_jmp_issues: list[SyncIssue] = []
        unresolved_direct_jmp_pos = advance_source_position(
            TraceEntry(
                index=19,
                pc=0x1000,
                opcode=0x4C,
                mnemonic="JMP",
                operand="$2000",
                full_line="@19 PC=$1000 OP=$4C JMP $2000",
                pre_pc=0x1000,
                post_pc=0x2000,
            ),
            0,
            [SourceInstruction("jmp_fail.s", 1, "L1000", "JMP", "MissingTarget")],
            defaultdict(list, {"L1000": [0]}),
            control_flow_sync_issues=unresolved_direct_jmp_issues,
        )
        assert unresolved_direct_jmp_pos == 1  # nosec B101
        assert len(unresolved_direct_jmp_issues) == 1  # nosec B101
        assert (
            unresolved_direct_jmp_issues[0].reason == "jmp-unresolved-target"
        )  # nosec B101

        symbols_file = root / "cpu65c02.cpp"
        symbols_file.write_text(
            """static const MonitorSymbol kMonitorSymbols[] = {
    {0x0079, "SrcP"},
    {0x0079, "UnsortedP"},
    {0x00AF, "Accum", MonitorSymbolRead | MonitorSymbolWrite},
};
""",
            encoding="utf-8",
        )

        symbols = parse_existing_monitor_symbols(symbols_file)
        assert symbols[0x0079] == ["SrcP", "UnsortedP"]  # nosec B101
        assert symbols[0x00AF] == ["Accum"]  # nosec B101

        symbols_if0_file = root / "cpu65c02_if0.cpp"
        symbols_if0_file.write_text(
            """static const MonitorSymbol kMonitorSymbols[] = {
#if 0
    {0x1234, "DisabledSymbol"},
#else
    {0x2345, "EnabledSymbol"},
#endif
};
""",
            encoding="utf-8",
        )
        symbols_if0 = parse_existing_monitor_symbols(symbols_if0_file)
        assert symbols_if0.get(0x1234) is None  # nosec B101
        assert symbols_if0[0x2345] == ["EnabledSymbol"]  # nosec B101

        ng = build_ngram_index(parsed_source, window=2)
        key = tuple(inst.mnemonic for inst in parsed_source[:2])
        assert key in ng  # nosec B101

        # Phase 2: resolve ambiguous mnemonic windows with operand/address hints.
        assert runtime_label_fixed_address("L1234") == 0x1234  # nosec B101
        assert runtime_label_fixed_address("x00FF") == 0x00FF  # nosec B101
        assert runtime_label_fixed_address("Main") is None  # nosec B101

        ambiguous_source = [
            SourceInstruction("amb.s", 1, "PathA", "LDA", "#$11"),
            SourceInstruction("amb.s", 2, None, "STA", "$0200"),
            SourceInstruction("amb.s", 3, None, "BNE", "PathA"),
            SourceInstruction("amb.s", 4, "PathB", "LDA", "#$22"),
            SourceInstruction("amb.s", 5, None, "STA", "$0300"),
            SourceInstruction("amb.s", 6, None, "BNE", "PathB"),
        ]
        ambiguous_ngram = build_ngram_index(ambiguous_source, window=3)
        pending_ambiguous: Deque[TraceEntry] = deque(
            [
                TraceEntry(
                    index=0,
                    pc=0x2000,
                    opcode=0xA9,
                    mnemonic="LDA",
                    operand="#$22",
                    full_line="@0 PC=$2000 OP=$A9 LDA #$22",
                    pre_pc=0x2000,
                    post_pc=0x2002,
                ),
                TraceEntry(
                    index=1,
                    pc=0x2002,
                    opcode=0x8D,
                    mnemonic="STA",
                    operand="$0300",
                    full_line="@1 PC=$2002 OP=$8D STA $0300",
                    pre_pc=0x2002,
                    post_pc=0x2005,
                ),
                TraceEntry(
                    index=2,
                    pc=0x2005,
                    opcode=0xD0,
                    mnemonic="BNE",
                    operand="$2000",
                    full_line="@2 PC=$2005 OP=$D0 BNE $2000",
                    pre_pc=0x2005,
                    post_pc=0x2000,
                ),
            ]
        )
        ambiguous_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(ambiguous_source):
            if inst.label:
                ambiguous_labels[inst.label.upper()].append(idx)

        sync_idx, sync_issue = acquire_sync_window(
            pending_ambiguous,
            ambiguous_source,
            ambiguous_ngram,
            ambiguous_labels,
            window=3,
        )
        assert sync_issue is None  # nosec B101
        assert sync_idx == 3  # nosec B101

        # Phase 2: when already synced, try local auto-resync before prompting.
        local_source = [
            SourceInstruction("loop.s", 1, "Loop", "STA", "$0100,X"),
            SourceInstruction("loop.s", 2, None, "DEX", ""),
            SourceInstruction("loop.s", 3, None, "BNE", "Loop"),
            SourceInstruction("loop.s", 4, None, "LDY", "#$19"),
            SourceInstruction("loop.s", 5, None, "LDA", "#$00"),
        ]
        local_ngram = build_ngram_index(local_source, window=3)
        pending_local: Deque[TraceEntry] = deque(
            [
                TraceEntry(
                    index=8,
                    pc=0x2007,
                    opcode=0x9D,
                    mnemonic="STA",
                    operand="$0100,X",
                    full_line="@8 PC=$2007 OP=$9D STA $0100,X",
                    pre_pc=0x2007,
                    post_pc=0x200A,
                ),
                TraceEntry(
                    index=9,
                    pc=0x200A,
                    opcode=0xCA,
                    mnemonic="DEX",
                    operand="",
                    full_line="@9 PC=$200A OP=$CA DEX",
                    pre_pc=0x200A,
                    post_pc=0x200B,
                ),
                TraceEntry(
                    index=10,
                    pc=0x200B,
                    opcode=0xD0,
                    mnemonic="BNE",
                    operand="$2007",
                    full_line="@10 PC=$200B OP=$D0 BNE $2007",
                    pre_pc=0x200B,
                    post_pc=0x2007,
                ),
            ]
        )
        local_resync_idx = attempt_local_auto_resync(
            pending_local,
            local_source,
            local_ngram,
            defaultdict(list),
            current_source_index=3,
            window=3,
            search_radius=4,
        )
        assert local_resync_idx == 0  # nosec B101

        # Phase 2: do not auto-accept unique low-confidence sync candidates.
        weak_source = [
            SourceInstruction("weak.s", 1, "Target", "LDA", "#$10"),
            SourceInstruction("weak.s", 2, None, "STA", "$1234"),
            SourceInstruction("weak.s", 3, None, "BEQ", "Target"),
            SourceInstruction("weak.s", 4, None, "RTS", ""),
        ]
        weak_ngram = build_ngram_index(weak_source, window=3)
        weak_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(weak_source):
            if inst.label:
                weak_labels[inst.label.upper()].append(idx)

        pending_weak_sync: Deque[TraceEntry] = deque(
            [
                TraceEntry(
                    index=20,
                    pc=0x2100,
                    opcode=0xA5,
                    mnemonic="LDA",
                    operand="$44",
                    full_line="@20 PC=$2100 OP=$A5 LDA $44",
                    pre_pc=0x2100,
                    post_pc=0x2102,
                ),
                TraceEntry(
                    index=21,
                    pc=0x2102,
                    opcode=0x9D,
                    mnemonic="STA",
                    operand="$5678,X",
                    full_line="@21 PC=$2102 OP=$9D STA $5678,X",
                    pre_pc=0x2102,
                    post_pc=0x2105,
                ),
                TraceEntry(
                    index=22,
                    pc=0x2105,
                    opcode=0xF0,
                    mnemonic="BEQ",
                    operand="$9ABC",
                    full_line="@22 PC=$2105 OP=$F0 BEQ $9ABC",
                    pre_pc=0x2105,
                    post_pc=0x2107,
                ),
            ]
        )
        weak_sync_idx, weak_sync_issue = acquire_sync_window(
            pending_weak_sync,
            weak_source,
            weak_ngram,
            weak_labels,
            window=3,
        )
        assert weak_sync_idx is None  # nosec B101
        assert weak_sync_issue is not None  # nosec B101

        pending_weak_local: Deque[TraceEntry] = deque(
            [
                TraceEntry(
                    index=30,
                    pc=0x2200,
                    opcode=0xA5,
                    mnemonic="LDA",
                    operand="$55",
                    full_line="@30 PC=$2200 OP=$A5 LDA $55",
                    pre_pc=0x2200,
                    post_pc=0x2202,
                ),
                TraceEntry(
                    index=31,
                    pc=0x2202,
                    opcode=0x9D,
                    mnemonic="STA",
                    operand="$6789,X",
                    full_line="@31 PC=$2202 OP=$9D STA $6789,X",
                    pre_pc=0x2202,
                    post_pc=0x2205,
                ),
                TraceEntry(
                    index=32,
                    pc=0x2205,
                    opcode=0xF0,
                    mnemonic="BEQ",
                    operand="$8000",
                    full_line="@32 PC=$2205 OP=$F0 BEQ $8000",
                    pre_pc=0x2205,
                    post_pc=0x2207,
                ),
            ]
        )
        weak_local_idx = attempt_local_auto_resync(
            pending_weak_local,
            weak_source,
            weak_ngram,
            weak_labels,
            current_source_index=2,
            window=3,
            search_radius=8,
        )
        assert weak_local_idx is None  # nosec B101

        pc_symbol_source = [
            SourceInstruction("s.s", 1, "Near", "NOP", ""),
            SourceInstruction("s.s", 2, "Far", "LDX", "#$03"),
        ]
        pc_symbol_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(pc_symbol_source):
            if inst.label:
                pc_symbol_labels[inst.label.upper()].append(idx)

        pc_symbol_entry = TraceEntry(
            index=40,
            pc=0xB100,
            opcode=0xA2,
            mnemonic="LDX",
            operand="#$03",
            full_line="@40 PC=$B100 OP=$A2 LDX #$03",
            pre_pc=None,
            post_pc=None,
            pc_symbol="Far",
        )
        pc_symbol_idx = attempt_pc_symbol_resync(
            pc_symbol_entry,
            pc_symbol_source,
            pc_symbol_labels,
            current_source_index=0,
            runtime_label_events=[],
        )
        assert pc_symbol_idx == 1  # nosec B101

        # Prefer exact PC address label over ambiguous semantic pc symbol alias.
        pc_addr_source = [
            SourceInstruction("addr.s", 1, "COUT", "NOP", ""),
            SourceInstruction("addr.s", 2, "LB3D9", "CMP", "#$E0"),
        ]
        pc_addr_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(pc_addr_source):
            if inst.label:
                pc_addr_labels[normalize_label_token(inst.label)].append(idx)

        pc_addr_entry = TraceEntry(
            index=41,
            pc=0xB3D9,
            opcode=0xC9,
            mnemonic="CMP",
            operand="#$E0",
            full_line="@41 PC=$B3D9 OP=$C9 CMP #$E0",
            pre_pc=None,
            post_pc=None,
            pc_symbol="COUT",
        )
        pc_addr_idx = attempt_pc_symbol_resync(
            pc_addr_entry,
            pc_addr_source,
            pc_addr_labels,
            current_source_index=0,
            runtime_label_events=[],
        )
        assert pc_addr_idx == 1  # nosec B101

        # Phase 1: capture runtime-required mismatch PC-symbol resync.
        pc_symbol_runtime_events: list[dict[str, object]] = []
        pc_symbol_runtime_idx = attempt_pc_symbol_resync(
            pc_symbol_entry,
            pc_symbol_source,
            pc_symbol_labels,
            current_source_index=0,
            runtime_label_events=pc_symbol_runtime_events,
        )
        assert pc_symbol_runtime_idx == 1  # nosec B101
        assert len(pc_symbol_runtime_events) == 1  # nosec B101
        assert pc_symbol_runtime_events[0]["kind"] == "pc-symbol-resync"  # nosec B101
        assert pc_symbol_runtime_events[0]["runtime_label"] == "Far"  # nosec B101
        assert pc_symbol_runtime_events[0]["target_source_index"] == 1  # nosec B101

        # No event should be recorded if candidate equals current source index.
        pc_symbol_same_events: list[dict[str, object]] = []
        pc_symbol_same_idx = attempt_pc_symbol_resync(
            pc_symbol_entry,
            pc_symbol_source,
            pc_symbol_labels,
            current_source_index=1,
            runtime_label_events=pc_symbol_same_events,
        )
        assert pc_symbol_same_idx == 1  # nosec B101
        assert pc_symbol_same_events == []  # nosec B101

        # No event should be recorded when runtime and pc_symbol lookups fail.
        pc_symbol_miss_events: list[dict[str, object]] = []
        pc_symbol_miss_entry = TraceEntry(
            index=42,
            pc=0xB222,
            opcode=0xEA,
            mnemonic="NOP",
            operand="",
            full_line="@42 PC=$B222 OP=$EA NOP",
            pre_pc=None,
            post_pc=None,
            pc_symbol="NoMatch",
        )
        pc_symbol_miss_idx = attempt_pc_symbol_resync(
            pc_symbol_miss_entry,
            pc_symbol_source,
            pc_symbol_labels,
            current_source_index=0,
            runtime_label_events=pc_symbol_miss_events,
        )
        assert pc_symbol_miss_idx is None  # nosec B101
        assert pc_symbol_miss_events == []  # nosec B101

        indirect_jmp_source = [
            SourceInstruction("ijmp.s", 1, "LB3D9", "CMP", "#$E0"),
            SourceInstruction("ijmp.s", 2, None, "BCC", "LB3E8"),
        ]
        indirect_jmp_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(indirect_jmp_source):
            if inst.label:
                indirect_jmp_labels[normalize_label_token(inst.label)].append(idx)

        pending_indirect_jmp: Deque[TraceEntry] = deque(
            [
                TraceEntry(
                    index=50,
                    pc=0xFDED,
                    opcode=0x6C,
                    mnemonic="JMP",
                    operand="(0036)",
                    full_line="@50 PC=$FDED OP=$6C JMP (0036)",
                    pre_pc=None,
                    post_pc=None,
                ),
                TraceEntry(
                    index=51,
                    pc=0xB3D9,
                    opcode=0xC9,
                    mnemonic="CMP",
                    operand="#$E0",
                    full_line="@51 PC=$B3D9 OP=$C9 CMP #$E0",
                    pre_pc=None,
                    post_pc=None,
                    pc_symbol="LB3D9",
                ),
            ]
        )
        indirect_jmp_idx = attempt_indirect_jmp_next_trace_resync(
            pending_indirect_jmp,
            indirect_jmp_source,
            indirect_jmp_labels,
            current_source_index=0,
            runtime_label_events=[],
        )
        assert indirect_jmp_idx == 0  # nosec B101

        # No event should be recorded if indirect-JMP candidate equals current index.
        indirect_jmp_same_events: list[dict[str, object]] = []
        indirect_jmp_same_idx = attempt_indirect_jmp_next_trace_resync(
            pending_indirect_jmp,
            indirect_jmp_source,
            indirect_jmp_labels,
            current_source_index=0,
            runtime_label_events=indirect_jmp_same_events,
        )
        assert indirect_jmp_same_idx == 0  # nosec B101
        assert indirect_jmp_same_events == []  # nosec B101

        # Phase 1: capture runtime-required mismatch indirect-JMP resync.
        indirect_jmp_runtime_events: list[dict[str, object]] = []
        indirect_jmp_runtime_idx = attempt_indirect_jmp_next_trace_resync(
            pending_indirect_jmp,
            indirect_jmp_source,
            indirect_jmp_labels,
            current_source_index=1,
            runtime_label_events=indirect_jmp_runtime_events,
        )
        assert indirect_jmp_runtime_idx == 0  # nosec B101
        assert len(indirect_jmp_runtime_events) == 1  # nosec B101
        assert (  # nosec B101
            indirect_jmp_runtime_events[0]["kind"] == "indirect-jmp-next-trace-resync"
        )
        assert indirect_jmp_runtime_events[0]["runtime_label"] == "LB3D9"  # nosec B101
        assert indirect_jmp_runtime_events[0]["target_source_index"] == 0  # nosec B101

        indirect_jmp_diag = build_indirect_jmp_resync_diagnostics(
            pending_indirect_jmp,
            indirect_jmp_source,
            current_source_index=1,
            target_source_index=0,
        )
        assert len(indirect_jmp_diag) == 2  # nosec B101
        assert "IJMP decision: method=next-trace-runtime-label" in indirect_jmp_diag[1]
        assert "target=ijmp.s:1" in indirect_jmp_diag[1]

        indirect_jmp_conflict_diag = build_indirect_jmp_conflict_diagnostics(
            pending_indirect_jmp,
            indirect_jmp_source,
            current_source_index=1,
        )
        assert len(indirect_jmp_conflict_diag) == 2  # nosec B101
        assert (
            "IJMP conflict: method=next-trace-follow-failed"
            in indirect_jmp_conflict_diag[1]
        )
        assert "runtime-label=LB3D9" in indirect_jmp_conflict_diag[1]

        operand_label_diag = build_operand_label_mismatch_diagnostics(
            TraceEntry(
                index=77,
                pc=0xB3DB,
                opcode=0x90,
                mnemonic="BCC",
                operand="$B3E8",
                full_line="@77 PC=$B3DB OP=$90 BCC $B3E8",
                pre_pc=None,
                post_pc=None,
            ),
            [
                SourceInstruction("monitor.s", 911, None, "BCC", "LFDF6"),
                SourceInstruction("monitor.s", 913, "LFDF6", "STY", "YSAV1"),
            ],
            source_index=0,
            operand_label="LFDF6",
            resolved_addr=0xB3E8,
            expected_addr=0xFDF6,
        )
        assert len(operand_label_diag) == 2  # nosec B101
        assert (
            "OP-LABEL conflict: method=fixed-runtime-label-check"
            in operand_label_diag[1]
        )
        assert "label=LFDF6, resolved=$B3E8, expected=$FDF6" in operand_label_diag[1]

        # Xhhhh and Lhhhh labels should resolve to the same source target.
        xmap_source = [
            SourceInstruction("xmap.s", 1, "LA75B", "LDA", "#$00"),
            SourceInstruction("xmap.s", 2, "LB000", "RTS", ""),
        ]
        xmap_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(xmap_source):
            if inst.label:
                xmap_labels[normalize_label_token(inst.label)].append(idx)

        assert (
            choose_source_label_target("XA75B", 1, xmap_source, xmap_labels) == 0
        )  # nosec B101
        assert (
            choose_source_label_target("LA75B", 1, xmap_source, xmap_labels) == 0
        )  # nosec B101

        # Runtime-label report should be TSV and dedupe duplicate runtime events.
        runtime_report = root / "runtime_labels_needed.tsv"
        report_events = [
            {
                "kind": "rts-runtime-fallback",
                "trace_index": 11,
                "trace_pc": 0x3001,
                "runtime_label": "L2003",
                "target_source_index": 1,
                "target_source_file": "main.s",
                "target_source_line": 2,
            },
            {
                "kind": "rts-runtime-fallback",
                "trace_index": 11,
                "trace_pc": 0x3001,
                "runtime_label": "L2003",
                "target_source_index": 1,
                "target_source_file": "main.s",
                "target_source_line": 2,
            },
            {
                "kind": "jsr-runtime-fallback",
                "trace_index": 160,
                "trace_pc": 0x2000,
                "runtime_label": "Worker",
                "target_source_index": 2,
                "target_source_file": "jsr_runtime.s",
                "target_source_line": 3,
            },
        ]
        report_written, report_duplicates = write_runtime_label_report(
            runtime_report,
            report_events,
        )
        report_lines = runtime_report.read_text(encoding="utf-8").splitlines()
        assert report_written == 2  # nosec B101
        assert report_duplicates == 1  # nosec B101
        assert report_lines[0].startswith("kind\ttrace_index\ttrace_pc\t")  # nosec B101
        assert len(report_lines) == 3  # nosec B101
        assert (  # nosec B101
            "rts-runtime-fallback\t11\t$3001\tL2003\tL2003\t$2003\t1\tmain.s\t2"
            in report_lines
        )
        assert (  # nosec B101
            "jsr-runtime-fallback\t160\t$2000\tWorker\tWORKER\t\t2\tjsr_runtime.s\t3"
            in report_lines
        )

        empty_runtime_report = root / "runtime_labels_needed.empty.tsv"
        empty_written, empty_duplicates = write_runtime_label_report(
            empty_runtime_report,
            [],
        )
        empty_lines = empty_runtime_report.read_text(encoding="utf-8").splitlines()
        assert empty_written == 0  # nosec B101
        assert empty_duplicates == 0  # nosec B101
        assert len(empty_lines) == 1  # nosec B101
        assert empty_lines[0].startswith("kind\ttrace_index\ttrace_pc\t")  # nosec B101

    rts_recent = [
        TraceEntry(200, 0x3000, 0x20, "JSR", "$4000", "@200", None, None),
        TraceEntry(201, 0x4000, 0xA9, "LDA", "#$01", "@201", None, None),
        TraceEntry(202, 0x4002, 0x60, "RTS", "", "@202", None, None),
    ]
    rts_out = io.StringIO()
    with patch("builtins.input", side_effect=["q"]), redirect_stdout(rts_out):
        prompt_for_help(
            SyncIssue("mnemonic-mismatch", "test"),
            help_pending_trace,
            help_source,
            8,
            help_labels,
            rts_recent,
            6,
        )
    assert (
        "Emulated stack before last shown RTS" not in rts_out.getvalue()
    )  # nosec B101

    with patch("sys.argv", ["disassembly_log_analyzer.py"]):
        parsed_defaults = parse_args()
    assert parsed_defaults.runtime_label_report == Path(
        "runtime_labels_needed.tsv"
    )  # nosec B101

    # operand_symbolic_token
    assert operand_symbolic_token("HOME") == "HOME"  # nosec B101
    assert operand_symbolic_token("HOME,X") == "HOME"  # nosec B101
    assert operand_symbolic_token("(HOME),Y") == "HOME"  # nosec B101
    assert operand_symbolic_token("(HOME,X)") == "HOME"  # nosec B101
    assert operand_symbolic_token("#SPACE") is None  # nosec B101
    assert operand_symbolic_token("$FC58") is None  # nosec B101
    assert operand_symbolic_token("#$20") is None  # nosec B101
    assert operand_symbolic_token("SomeLbl-1,X") == "SomeLbl"  # nosec B101
    assert operand_symbolic_token("SomeLbl+Other,X") is None  # nosec B101
    assert operand_symbolic_token("") is None  # nosec B101

    # operand_numeric_address
    assert operand_numeric_address("$FC58") == 0xFC58  # nosec B101
    assert operand_numeric_address("$FC58,X") == 0xFC58  # nosec B101
    assert operand_numeric_address("($FC58),Y") == 0xFC58  # nosec B101
    assert operand_numeric_address("$20") == 0x20  # nosec B101
    assert operand_numeric_address("$20,X") == 0x20  # nosec B101
    assert operand_numeric_address("#$20") is None  # nosec B101
    assert operand_numeric_address("#$FC58") is None  # nosec B101

    # extract_operand_label_pair: JSR symbolic target → MonitorSymbolPc
    _jsr_src = SourceInstruction("f.s", 1, None, "JSR", "HOME")
    _jsr_tr = TraceEntry(10, 0x2047, 0x20, "JSR", "$FC58", "@10", None, None)
    _ops = extract_operand_label_pairs(_jsr_src, _jsr_tr)
    assert _ops == [("HOME", 0xFC58, "MonitorSymbolPc")]  # nosec B101
    _op = extract_operand_label_pair(_jsr_src, _jsr_tr)
    assert _op == ("HOME", 0xFC58, "MonitorSymbolPc")  # nosec B101

    # extract_operand_label_pair: JSR target with +offset infers base label address
    _jsr_off_src = SourceInstruction("f.s", 1, None, "JSR", "HOME+$03")
    _jsr_off_tr = TraceEntry(10, 0x2047, 0x20, "JSR", "$FC5B", "@10", None, None)
    _op_off = extract_operand_label_pair(_jsr_off_src, _jsr_off_tr)
    assert _op_off == ("HOME", 0xFC58, "MonitorSymbolPc")  # nosec B101

    # extract_operand_label_pairs: MLI source emits both JSR target (PC) and param block (data)
    _mli_src = SourceInstruction("f.s", 1, None, "MLI", "PRODOS8,PBLOCK")
    _mli_tr = TraceEntry(
        10,
        0x2047,
        0x20,
        "MLI",
        ".byte $C8 .word $1234 (OPEN)",
        "@10",
        None,
        None,
    )
    _mli_ops = extract_operand_label_pairs(_mli_src, _mli_tr)
    assert _mli_ops == [  # nosec B101
        ("PRODOS8", 0xBF00, "MonitorSymbolPc"),
        ("PBLOCK", 0x1234, ""),
    ]

    # extract_operand_label_pairs: data half of MLI is symbolic-only (no synthetic from trace)
    _mli_num_src = SourceInstruction("f.s", 1, None, "MLI", "PRODOS8,$1234")
    _mli_num_ops = extract_operand_label_pairs(_mli_num_src, _mli_tr)
    assert _mli_num_ops == [("PRODOS8", 0xBF00, "MonitorSymbolPc")]  # nosec B101

    # extract_operand_label_pairs: MLI trace matched with JSR source keeps target as PC label
    _mli_jsr_src = SourceInstruction("f.s", 1, None, "JSR", "PRODOS8")
    _mli_jsr_ops = extract_operand_label_pairs(_mli_jsr_src, _mli_tr)
    assert _mli_jsr_ops == [("PRODOS8", 0xBF00, "MonitorSymbolPc")]  # nosec B101
    assert extract_operand_label_pair(_mli_jsr_src, _mli_tr) == (  # nosec B101
        "PRODOS8",
        0xBF00,
        "MonitorSymbolPc",
    )

    # extract_operand_label_pair: branch symbolic target → MonitorSymbolPc
    _bne_src = SourceInstruction("f.s", 2, None, "BNE", "Loop")
    _bne_tr = TraceEntry(11, 0x2005, 0xD0, "BNE", "$2000", "@11", 0x2005, 0x2000)
    _op2 = extract_operand_label_pair(_bne_src, _bne_tr)
    assert _op2 == ("Loop", 0x2000, "MonitorSymbolPc")  # nosec B101

    # extract_operand_label_pair: data operand → no flags
    _lda_src = SourceInstruction("f.s", 3, None, "LDA", "SomeVar")
    _lda_tr = TraceEntry(12, 0x2000, 0xAD, "LDA", "$3000", "@12", 0x2000, 0x2003)
    _op3 = extract_operand_label_pair(_lda_src, _lda_tr)
    assert _op3 == ("SomeVar", 0x3000, "")  # nosec B101

    # extract_operand_label_pair: data operand with +offset infers base label address
    _lda_off_src = SourceInstruction("f.s", 3, None, "LDA", "SomeVar+1")
    _lda_off_tr = TraceEntry(12, 0x2000, 0xAD, "LDA", "$3001", "@12", 0x2000, 0x2003)
    _op4 = extract_operand_label_pair(_lda_off_src, _lda_off_tr)
    assert _op4 == ("SomeVar", 0x3000, "")  # nosec B101

    # Zero-page width should wrap/derive in 8-bit space.
    _zp_off_src = SourceInstruction("f.s", 3, None, "LDA", "ZPVAR+1")
    _zp_off_tr = TraceEntry(12, 0x2000, 0xA5, "LDA", "$20", "@12", 0x2000, 0x2002)
    _op5 = extract_operand_label_pair(_zp_off_src, _zp_off_tr)
    assert _op5 == ("ZPVAR", 0x1F, "")  # nosec B101

    # extract_operand_label_pair: immediate source → None
    _imm_src = SourceInstruction("f.s", 4, None, "LDA", "#SPACE")
    _imm_tr = TraceEntry(13, 0x2003, 0xA9, "LDA", "#$20", "@13", 0x2003, 0x2005)
    assert extract_operand_label_pair(_imm_src, _imm_tr) is None  # nosec B101

    # extract_operand_label_pair: already-numeric source operand → None
    _num_src = SourceInstruction("f.s", 5, None, "LDA", "$3000")
    assert extract_operand_label_pair(_num_src, _lda_tr) is None  # nosec B101

    # extract_operand_label_pair: complex expressions are intentionally ignored
    _complex_src = SourceInstruction("f.s", 6, None, "LDA", "SomeVar+Other")
    assert extract_operand_label_pair(_complex_src, _lda_tr) is None  # nosec B101

    print("self-check passed")


def print_dry_run_summary(args: argparse.Namespace) -> None:
    trace_valid, trace_invalid, trace_samples = parse_trace_log(args.log)
    source_instructions = parse_source_tree(args.source_root)
    symbols = parse_existing_monitor_symbols(args.symbols_file)

    print("Dry-run summary")
    print(f"  log: {args.log}")
    print(f"    parsed entries: {trace_valid}")
    print(f"    skipped lines: {trace_invalid}")
    print(f"  source root: {args.source_root}")
    print(f"    instruction lines: {len(source_instructions)}")
    print(f"  symbols file: {args.symbols_file}")
    print(f"    symbol addresses: {len(symbols)}")
    print(
        f"    total symbol names (incl aliases): {sum(len(v) for v in symbols.values())}"
    )

    print("\nTrace sample")
    for entry in _preview(trace_samples, limit=3):
        print(
            f"  @{entry.index} PC=${entry.pc:04X} OP=${entry.opcode:02X} "
            f"{entry.mnemonic} {entry.operand}".rstrip()
        )

    print("\nSource sample")
    for inst in _preview(source_instructions, limit=3):
        label = f"{inst.label} " if inst.label else ""
        print(
            f"  {inst.file_path}:{inst.line_number} "
            f"{label}{inst.mnemonic} {inst.operand}".rstrip()
        )

    print("\nSymbol sample")
    for addr in _preview(sorted(symbols.keys()), limit=3):
        names = symbols[addr]
        print(f"  ${addr:04X} -> {', '.join(names)}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--log",
        type=Path,
        default=Path("prodos8emu_disassembly_trace.log"),
        help="Path to disassembly trace log.",
    )
    parser.add_argument(
        "--source-root",
        type=Path,
        default=Path("EDASM.SRC"),
        help="Path to EDASM source tree root.",
    )
    parser.add_argument(
        "--symbols-file",
        type=Path,
        default=Path("src/cpu65c02.cpp"),
        help="Path to cpu65c02.cpp containing kMonitorSymbols.",
    )
    parser.add_argument(
        "--annotated-log",
        type=Path,
        default=Path("prodos8emu_disassembly_trace.annotated.log"),
        help="Path to write annotated trace output.",
    )
    parser.add_argument(
        "--new-labels",
        type=Path,
        default=Path("new_labels_kMonitorSymbols.txt"),
        help="Path to write insertion-ready kMonitorSymbols lines.",
    )
    parser.add_argument(
        "--runtime-label-report",
        type=Path,
        default=Path("runtime_labels_needed.tsv"),
        help="Path to write runtime-label-required source-recovery TSV.",
    )
    parser.add_argument(
        "--sync-window",
        type=int,
        default=6,
        help="Mnemonic n-gram window size used to acquire sync.",
    )
    parser.add_argument(
        "--max-lines",
        type=int,
        default=0,
        help="Optional cap on processed trace instructions (0 = no cap).",
    )
    parser.add_argument(
        "--non-interactive",
        action="store_true",
        help="Stop at first unresolved sync issue instead of prompting for help.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse inputs and print summary/sample output.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run parser self-check assertions on synthetic snippets.",
    )
    parser.add_argument(
        "--run-after-self-check",
        action="store_true",
        help="Continue into alignment run after --self-check completes.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.self_check:
        run_self_check()
        if not args.run_after_self_check and not args.dry_run:
            return 0

    if args.dry_run:
        print_dry_run_summary(args)
        return 0

    return run_alignment(args)


if __name__ == "__main__":
    raise SystemExit(main())
