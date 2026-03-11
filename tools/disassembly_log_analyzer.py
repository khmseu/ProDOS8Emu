#!/usr/bin/env python3
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
from dataclasses import dataclass
from pathlib import Path
from typing import Deque, Iterable, Iterator, Sequence, TypeAlias, TypeVar
from unittest.mock import patch

T = TypeVar("T")
ReturnFrame: TypeAlias = tuple[int, int | None]
ReturnStackSnapshot: TypeAlias = tuple[ReturnFrame, ...]


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

OPERAND_ADDR16_RE = re.compile(r"\$([0-9A-Fa-f]{4})")

MONITOR_SYMBOL_RE = re.compile(
    r"\{\s*0x(?P<addr>[0-9A-Fa-f]+)\s*,\s*\"(?P<name>[^\"]+)\""
)

EQU_STAR_RE = re.compile(
    r"^(?P<label>[^\s:;]+)\s+EQU\s+\*$",
    re.IGNORECASE,
)

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


def parse_log_line(line: str) -> TraceEntry | None:
    stripped = line.rstrip("\n")
    match = TRACE_LINE_RE.match(stripped)
    if match is None:
        return None

    tail = match.group("tail")
    operand = tail.split(";", 1)[0].strip()

    pre_pc: int | None = None
    post_pc: int | None = None
    pc_symbol_raw = match.group("pcsym")
    pc_symbol = pc_symbol_raw.strip() if pc_symbol_raw else None
    pre_post = TRACE_PRE_POST_PC_RE.search(stripped)
    if pre_post is not None:
        pre_pc = int(pre_post.group("pre"), 16)
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
    )


def trace_entries(log_path: Path) -> Iterator[TraceEntry]:
    with log_path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            parsed = parse_log_line(raw)
            if parsed is not None:
                yield parsed


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

    block = text[block_start:block_end]

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
    if token.startswith("$") or token.startswith("#"):
        return None
    return token.rstrip(":")


def call_label_from_source(inst: SourceInstruction) -> str | None:
    if inst.mnemonic != "JSR":
        return None

    token = inst.operand.split(",", 1)[0].strip().split(" ", 1)[0]
    if not token:
        return None
    if token.startswith("$") or token.startswith("#") or token[0].isdigit():
        return None
    return token.rstrip(":")


def jump_label_from_source(inst: SourceInstruction) -> str | None:
    if inst.mnemonic != "JMP":
        return None

    token = inst.operand.split(",", 1)[0].strip().split(" ", 1)[0]
    if not token:
        return None
    if token.startswith("("):
        return None
    if token.startswith("$") or token.startswith("#") or token[0].isdigit():
        return None
    return token.rstrip(":")


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


def advance_source_position(
    trace_entry: TraceEntry,
    source_pos: int,
    source: list[SourceInstruction],
    label_to_source_indexes: dict[str, list[int]],
    return_stack: list[tuple[int, int | None]],
    observed_next_pc: int | None = None,
) -> int:
    source_inst = source[source_pos]
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

    if source_inst.mnemonic == "JMP":
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

    if source_inst.mnemonic == "JSR":
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

        if stepped_into_callee:
            return_stack.append((source_pos + 1, return_pc))

    if source_inst.mnemonic == "RTS" and return_stack:
        if observed_post_pc is not None:
            for i in range(len(return_stack) - 1, -1, -1):
                return_index, return_pc = return_stack[i]
                if return_pc == observed_post_pc:
                    del return_stack[i:]
                    return return_index

            # No reliable return target; keep alignment linear and let mismatch logic recover.
            return next_source_pos

        return_index, _ = return_stack.pop()
        return return_index

    return next_source_pos


def prompt_for_help(
    issue: SyncIssue,
    pending_trace: list[TraceEntry],
    source: list[SourceInstruction],
    current_source_index: int | None,
    label_to_source_indexes: dict[str, list[int]],
    recent_trace: list[TraceEntry] | None = None,
    last_matched_source_index: int | None = None,
    recent_stack_snapshots: Sequence[ReturnStackSnapshot | None] | None = None,
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
        displayed_snapshots = (
            recent_stack_snapshots[-6:] if recent_stack_snapshots else None
        )
        for entry in displayed:
            print(format_trace_prompt_entry(">>", entry))
        rts_idx = None
        for i in range(len(displayed) - 1, -1, -1):
            if displayed[i].mnemonic == "RTS":
                rts_idx = i
                break
        if rts_idx is not None:
            snapshot = (
                displayed_snapshots[rts_idx]
                if displayed_snapshots is not None
                and rts_idx < len(displayed_snapshots)
                else None
            )
            print("\nEmulated stack before last shown RTS (top first):")
            if snapshot is None:
                print("  (snapshot unavailable)")
            elif len(snapshot) == 0:
                print("  (empty)")
            else:
                for i, (src_idx, ret_pc) in enumerate(reversed(snapshot)):
                    pc_str = f"${ret_pc:04X}" if ret_pc is not None else "(unknown)"
                    print(f"  [{i}] source[{src_idx}], return_pc={pc_str}")
    for idx, entry in enumerate(pending_trace[:5]):
        marker = "=>" if idx == 0 else "  "
        print(format_trace_prompt_entry(marker, entry))

    if current_source_index is not None:
        lo = max(0, current_source_index - 6)
        hi = min(len(source), current_source_index + 4)
        print("\nSource context:")
        for idx in range(lo, hi):
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
            print(
                f"{pointer} [{idx}] {inst.file_path}:{inst.line_number} "
                f"{label}{inst.mnemonic} {inst.operand}".rstrip()
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
        return runtime_idx

    if trace_entry.pc_symbol is None:
        return None

    return choose_source_label_target(
        trace_entry.pc_symbol,
        current_source_index,
        source,
        label_to_source_indexes,
    )


def attempt_indirect_jmp_next_trace_resync(
    pending: Deque[TraceEntry],
    source: list[SourceInstruction],
    label_to_source_indexes: dict[str, list[int]],
    current_source_index: int,
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
    )


def format_return_stack_for_annotation(snapshot: ReturnStackSnapshot) -> str:
    if len(snapshot) == 0:
        return "(empty)"

    frames: list[str] = []
    for source_idx, return_pc in reversed(snapshot):
        pc_text = f"${return_pc:04X}" if return_pc is not None else "(unknown)"
        frames.append(f"source[{source_idx}],return_pc={pc_text}")

    return " | ".join(frames)


def build_annotated_line(
    base_line: str,
    names_here: list[str],
    source_mnemonic: str,
    pre_exec_snapshot: ReturnStackSnapshot,
    post_exec_snapshot: ReturnStackSnapshot,
) -> str:
    suffixes: list[str] = []
    if names_here:
        suffixes.append(f"NEW_PC_LABELS: {', '.join(names_here)}")
    if source_mnemonic == "RTS":
        suffixes.append(
            "EMU_STACK_BEFORE_RTS: "
            + format_return_stack_for_annotation(pre_exec_snapshot)
        )
    if len(post_exec_snapshot) > len(pre_exec_snapshot):
        suffixes.append(
            "EMU_STACK_AFTER_PUSH: "
            + format_return_stack_for_annotation(post_exec_snapshot)
        )

    if not suffixes:
        return base_line

    return f"{base_line} ; {' ; '.join(suffixes)}"


def write_discovered_labels(
    out_path: Path,
    discovered: dict[int, set[str]],
    existing: dict[int, list[str]],
) -> tuple[int, int]:
    entries_written = 0
    aliases_written = 0

    lines: list[str] = []
    for addr in sorted(discovered.keys()):
        existing_names = set(existing.get(addr, []))
        new_names = sorted(
            name for name in discovered[addr] if name not in existing_names
        )
        if not new_names:
            continue
        for name in new_names:
            lines.append(f'{{0x{addr:04X}, "{name}", MonitorSymbolPc}},')
            entries_written += 1
            if len(new_names) > 1:
                aliases_written += 1

    out_path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    return entries_written, aliases_written


def run_alignment(args: argparse.Namespace) -> int:
    source = parse_source_tree(args.source_root)
    if not source:
        print(f"No source instructions parsed under: {args.source_root}")
        return 1

    existing_symbols = parse_existing_monitor_symbols(args.symbols_file)
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
    processed = 0
    source_pos: int | None = None
    pending: Deque[TraceEntry] = deque()
    recent_trace: Deque[TraceEntry] = deque(maxlen=6)
    recent_stack_snapshots: Deque[ReturnStackSnapshot | None] = deque(maxlen=6)
    last_matched_source_index: int | None = None
    source_return_stack: list[tuple[int, int | None]] = []
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
                        list(recent_stack_snapshots),
                    )
                    if action == "quit":
                        stop_reason = issue.reason
                        break
                    if action == "skip":
                        recent_trace.append(pending.popleft())
                        recent_stack_snapshots.append(None)
                        continue
                    if action == "jump" and value is not None:
                        source_pos = value
                        source_return_stack.clear()
                        synced = True
                        continue
                    continue

                source_pos = sync_idx
                source_return_stack.clear()
                synced = True
                print(
                    f"Synced at source index {source_pos} using window size {args.sync_window}."
                )

            # Synced mode: consume one trace instruction at a time.
            assert source_pos is not None
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
                    list(recent_stack_snapshots),
                )
                if action == "quit":
                    stop_reason = issue.reason
                    break
                if action == "skip":
                    recent_trace.append(pending.popleft())
                    recent_stack_snapshots.append(None)
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    source_return_stack.clear()
                    continue
                continue

            trace_entry = pending[0]
            source_inst = source[source_pos]

            if not mnemonics_match(trace_entry.mnemonic, source_inst.mnemonic):
                indirect_jmp_resync_idx = attempt_indirect_jmp_next_trace_resync(
                    pending,
                    source,
                    label_to_source_indexes,
                    source_pos,
                )
                if indirect_jmp_resync_idx is not None:
                    recent_trace.append(pending.popleft())
                    recent_stack_snapshots.append(None)
                    processed += 1
                    source_pos = indirect_jmp_resync_idx
                    source_return_stack.clear()
                    continue

                pc_symbol_resync_idx = attempt_pc_symbol_resync(
                    trace_entry,
                    source,
                    label_to_source_indexes,
                    source_pos,
                )
                if (
                    pc_symbol_resync_idx is not None
                    and pc_symbol_resync_idx != source_pos
                ):
                    source_pos = pc_symbol_resync_idx
                    source_return_stack.clear()
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
                    source_return_stack.clear()
                    continue

                issue = SyncIssue(
                    "mnemonic-mismatch",
                    (
                        f"Trace @{trace_entry.index} PC=${trace_entry.pc:04X} has {trace_entry.mnemonic}, "
                        f"source[{source_pos}] has {source_inst.mnemonic} "
                        f"({source_inst.file_path}:{source_inst.line_number})."
                    ),
                )
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
                    list(recent_stack_snapshots),
                )
                if action == "quit":
                    stop_reason = issue.reason
                    break
                if action == "skip":
                    recent_trace.append(pending.popleft())
                    recent_stack_snapshots.append(None)
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    source_return_stack.clear()
                    continue
                continue

            if source_inst.label:
                discovered[trace_entry.pc].add(source_inst.label)

            pre_exec_snapshot = tuple(source_return_stack)
            names_here = sorted(discovered.get(trace_entry.pc, set()))

            last_matched_source_index = source_pos
            recent_trace.append(trace_entry)
            recent_stack_snapshots.append(pre_exec_snapshot)
            pending.popleft()
            observed_next_pc = pending[0].pc if pending else None
            source_pos = advance_source_position(
                trace_entry,
                source_pos,
                source,
                label_to_source_indexes,
                source_return_stack,
                observed_next_pc,
            )

            post_exec_snapshot = tuple(source_return_stack)
            line_out = build_annotated_line(
                trace_entry.full_line,
                names_here,
                source_inst.mnemonic,
                pre_exec_snapshot,
                post_exec_snapshot,
            )
            annotated_out.write(line_out + "\n")
            processed += 1

            if args.max_lines and processed >= args.max_lines:
                stop_reason = "max-lines"
                break

    finally:
        annotated_out.close()

    entries_written, aliases_written = write_discovered_labels(
        args.new_labels,
        discovered,
        existing_symbols,
    )

    print("\nAlignment summary")
    print(f"  processed trace instructions: {processed}")
    print(f"  stop reason: {stop_reason}")
    print(f"  annotated log: {args.annotated_log}")
    print(f"  new labels file: {args.new_labels}")
    print(f"  inserted-ready entries: {entries_written}")
    print(f"  alias entries (same address, additional names): {aliases_written}")

    if stop_reason not in {"eof", "max-lines"}:
        print("  NOTE: stopped at unresolved sync point (help required).")

    return 0


def run_self_check() -> None:
    sample_trace = (
        "@123 PC=$2000 OP=$A9 LDA #$01 ; PRE PC=$2000 A=$00 X=$00 Y=$00 "
        "SP=$FF P=$24 POST PC=$2002 A=$01 X=$00 Y=$00 SP=$FF P=$24"
    )
    parsed_trace = parse_log_line(sample_trace)
    assert parsed_trace is not None
    assert parsed_trace.index == 123
    assert parsed_trace.pc == 0x2000
    assert parsed_trace.opcode == 0xA9
    assert parsed_trace.mnemonic == "LDA"
    assert parsed_trace.operand == "#$01"
    assert parsed_trace.pre_pc == 0x2000
    assert parsed_trace.post_pc == 0x2002

    mli_trace = parse_log_line(
        "@1 PC=$1000 OP=$20 MLI .byte $C8 .word $1234 (OPEN) ; PRE X POST X"
    )
    assert mli_trace is not None
    assert mli_trace.mnemonic == "MLI"

    no_operand_trace = parse_log_line(
        "@7 PC=$2002 OP=$9A TXS ; PRE PC=$2002 POST PC=$2003"
    )
    assert no_operand_trace is not None
    assert no_operand_trace.operand == ""

    pc_symbol_trace = parse_log_line(
        "@8 PC=$FC22 (LFC22) OP=$A5 LDA $25 ; PRE PC=$FC22 POST PC=$FC24"
    )
    assert pc_symbol_trace is not None
    assert pc_symbol_trace.pc == 0xFC22
    assert pc_symbol_trace.mnemonic == "LDA"
    assert pc_symbol_trace.pc_symbol == "LFC22"

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
    assert enriched.pc_symbol == "LFC22"

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
    assert action == "quit"
    assert value is None

    prompt_text = prompt_output.getvalue()
    assert prompt_text.count("\nLegend:\n") == 1
    legend_pos = prompt_text.index("=> current trace/source candidate")
    pending_pos = prompt_text.index("Pending trace entries:")
    assert legend_pos < pending_pos
    assert (
        prompt_text.count(">> previous trace entry / last matched source instruction")
        == 1
    )
    assert prompt_text.count(">> @100 PC=$1FF6 OP=$A9 LDA #$00") == 1
    assert prompt_text.count(">> @101 PC=$1FF8 OP=$8D STA $3000") == 1
    assert prompt_text.count(">> @102 PC=$1FFB OP=$EA NOP") == 1
    assert prompt_text.count(">> @103 PC=$1FFC OP=$A2 LDX #$04") == 1
    assert prompt_text.count(">> @104 PC=$1FFE OP=$86 STX $20") == 1
    assert prompt_text.count(">> @105 PC=$2000 OP=$A0 LDY #$01") == 1
    assert prompt_text.count("=> @106 PC=$2006 OP=$C9 CMP #$10") == 1
    assert "[0] Monitor.S:905 Older0 CLC" not in prompt_text
    assert "[1] Monitor.S:906 LDA #$00" not in prompt_text
    assert "[2] Monitor.S:907 STA $3000" in prompt_text
    assert "[5] Monitor.S:910 LDY #$01" in prompt_text
    assert ">> [6] Monitor.S:911 LastGood CMP #$08" in prompt_text
    assert "=> [8] Monitor.S:913 Next STY $20" in prompt_text
    assert "(prev)" not in prompt_text
    assert "=> current source candidate" not in prompt_text
    assert ">> last matched source instruction" not in prompt_text

    retained_recent_trace: Deque[TraceEntry] = deque(maxlen=6)
    for index in range(7):
        retained_recent_trace.append(
            TraceEntry(index, 0x2000 + index, 0xEA, "NOP", "", "", None, None)
        )
    assert [entry.index for entry in retained_recent_trace] == [1, 2, 3, 4, 5, 6]

    assert format_return_stack_for_annotation(()) == "(empty)"
    assert (
        format_return_stack_for_annotation(((4, 0x3003), (9, None)))
        == "source[9],return_pc=(unknown) | source[4],return_pc=$3003"
    )

    annotated_plain = build_annotated_line(
        "@1 PC=$3000 OP=$EA NOP",
        [],
        "NOP",
        (),
        (),
    )
    assert annotated_plain == "@1 PC=$3000 OP=$EA NOP"

    annotated_rts = build_annotated_line(
        "@2 PC=$3001 OP=$60 RTS",
        ["LB001"],
        "RTS",
        ((5, 0x3003),),
        (),
    )
    assert "NEW_PC_LABELS: LB001" in annotated_rts
    assert "EMU_STACK_BEFORE_RTS: source[5],return_pc=$3003" in annotated_rts

    annotated_push = build_annotated_line(
        "@3 PC=$3002 OP=$20 JSR $4000",
        [],
        "JSR",
        ((5, 0x3003),),
        ((5, 0x3003), (8, 0x3005)),
    )
    assert "EMU_STACK_AFTER_PUSH: source[8],return_pc=$3005 | source[5],return_pc=$3003" in annotated_push

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
        assert len(parsed_source) == 4
        assert parsed_source[0].label == "Start"
        assert parsed_source[2].mnemonic == "BBR"

        branch_label = branch_label_from_source(parsed_source[2])
        assert branch_label == "Next"

        equ_file = src_dir / "EQUSTAR.S"
        equ_file.write_text(
            """Alias EQU *
Labeled LDA #$01
""",
            encoding="utf-8",
        )
        parsed_source = parse_source_tree(src_dir)
        equ_inst = next(inst for inst in parsed_source if inst.label == "Labeled")
        assert "Alias" in equ_inst.aliases

        call_source = [
            SourceInstruction("main.s", 1, "Main", "JSR", "Worker"),
            SourceInstruction("main.s", 2, "AfterCall", "LDA", "#$01"),
            SourceInstruction("main.s", 3, None, "STA", "$2000"),
            SourceInstruction("worker.s", 1, "Worker", "PHA", ""),
            SourceInstruction("worker.s", 2, None, "RTS", ""),
        ]
        call_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(call_source):
            if inst.label:
                call_labels[inst.label.upper()].append(idx)

        return_stack: list[tuple[int, int | None]] = []
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
            return_stack,
        )
        assert next_pos == 3
        assert return_stack == [(1, 0x2003)]

        # Do not push a return frame when JSR does not step into a selected target.
        no_step_return_stack: list[tuple[int, int | None]] = []
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
        no_step_pos = advance_source_position(
            jsr_no_step_trace,
            0,
            call_source,
            call_labels,
            no_step_return_stack,
        )
        assert no_step_pos == 1
        assert no_step_return_stack == []

        return_stack = [(1, 0x2003), (2, 0x2FFF)]
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
        next_pos = advance_source_position(
            rts_trace,
            4,
            call_source,
            call_labels,
            return_stack,
        )
        assert next_pos == 1
        assert return_stack == []

        # If post_pc is present and no frame matches it, do not blind-pop stack frames.
        unmatched_stack: list[tuple[int, int | None]] = [(1, 0x2003), (2, 0x2FFF)]
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
        unmatched_pos = advance_source_position(
            rts_unmatched_trace,
            4,
            call_source,
            call_labels,
            unmatched_stack,
        )
        assert unmatched_pos == 5
        assert unmatched_stack == [(1, 0x2003), (2, 0x2FFF)]

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
            [],
            observed_next_pc=0x2007,
        )
        assert taken_pos == 0

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
            [],
            observed_next_pc=0x200D,
        )
        assert not_taken_pos == 3

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
        jsr_missing_post_stack: list[tuple[int, int | None]] = []
        jsr_missing_post_pos = advance_source_position(
            jsr_missing_post_return,
            0,
            call_source,
            call_labels,
            jsr_missing_post_stack,
            observed_next_pc=0x2003,
        )
        assert jsr_missing_post_pos == 1
        assert jsr_missing_post_stack == []

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
        jsr_missing_post_step_stack: list[tuple[int, int | None]] = []
        jsr_missing_post_step_pos = advance_source_position(
            jsr_missing_post_step,
            0,
            call_source,
            call_labels,
            jsr_missing_post_step_stack,
            observed_next_pc=0x3000,
        )
        assert jsr_missing_post_step_pos == 3
        assert jsr_missing_post_step_stack == [(1, 0x2003)]

        # Phase 3: RTS fallback should match stack frame via observed next PC.
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
        rts_missing_post_stack: list[tuple[int, int | None]] = [
            (1, 0x2003),
            (2, 0x2FFF),
        ]
        rts_missing_post_pos = advance_source_position(
            rts_missing_post_trace,
            4,
            call_source,
            call_labels,
            rts_missing_post_stack,
            observed_next_pc=0x2003,
        )
        assert rts_missing_post_pos == 1
        assert rts_missing_post_stack == []

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
            [],
            observed_next_pc=0x2000,
        )
        assert jmp_pos == 2

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
        assert symbols[0x0079] == ["SrcP", "UnsortedP"]
        assert symbols[0x00AF] == ["Accum"]

        ng = build_ngram_index(parsed_source, window=2)
        key = tuple(inst.mnemonic for inst in parsed_source[:2])
        assert key in ng

        # Phase 2: resolve ambiguous mnemonic windows with operand/address hints.
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
        assert sync_issue is None
        assert sync_idx == 3

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
        assert local_resync_idx == 0

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
        assert weak_sync_idx is None
        assert weak_sync_issue is not None

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
        assert weak_local_idx is None

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
        )
        assert pc_symbol_idx == 1

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
        )
        assert pc_addr_idx == 1

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
        )
        assert indirect_jmp_idx == 0

        # Xhhhh and Lhhhh labels should resolve to the same source target.
        xmap_source = [
            SourceInstruction("xmap.s", 1, "LA75B", "LDA", "#$00"),
            SourceInstruction("xmap.s", 2, "LB000", "RTS", ""),
        ]
        xmap_labels: dict[str, list[int]] = defaultdict(list)
        for idx, inst in enumerate(xmap_source):
            if inst.label:
                xmap_labels[normalize_label_token(inst.label)].append(idx)

        assert choose_source_label_target("XA75B", 1, xmap_source, xmap_labels) == 0
        assert choose_source_label_target("LA75B", 1, xmap_source, xmap_labels) == 0

    # Phase 1-3: RTS stack snapshot rendered in prompt.
    rts_recent = [
        TraceEntry(200, 0x3000, 0x20, "JSR", "$4000", "@200", None, None),
        TraceEntry(201, 0x4000, 0xA9, "LDA", "#$01", "@201", None, None),
        TraceEntry(202, 0x4002, 0x60, "RTS", "", "@202", None, None),
    ]

    # RTS with non-empty snapshot -> section with frame lines.
    rts_snapshots_nonempty: list[ReturnStackSnapshot | None] = [
        (),
        ((5, 0x3003),),
        ((5, 0x3003),),
    ]
    rts_out1 = io.StringIO()
    with patch("builtins.input", side_effect=["q"]), redirect_stdout(rts_out1):
        prompt_for_help(
            SyncIssue("mnemonic-mismatch", "test"),
            help_pending_trace,
            help_source,
            8,
            help_labels,
            rts_recent,
            6,
            rts_snapshots_nonempty,
        )
    rts_text1 = rts_out1.getvalue()
    assert "Emulated stack before last shown RTS (top first):" in rts_text1
    assert "[0] source[5], return_pc=$3003" in rts_text1

    # RTS with empty snapshot -> explicit empty output.
    rts_snapshots_empty: list[ReturnStackSnapshot | None] = [None, None, ()]
    rts_out_empty = io.StringIO()
    with patch("builtins.input", side_effect=["q"]), redirect_stdout(rts_out_empty):
        prompt_for_help(
            SyncIssue("mnemonic-mismatch", "test"),
            help_pending_trace,
            help_source,
            8,
            help_labels,
            rts_recent,
            6,
            rts_snapshots_empty,
        )
    rts_text_empty = rts_out_empty.getvalue()
    assert "Emulated stack before last shown RTS (top first):" in rts_text_empty
    assert "(empty)" in rts_text_empty

    # RTS with unavailable snapshot (None) -> fallback line.
    rts_snapshots_unavail: list[ReturnStackSnapshot | None] = [None, None, None]
    rts_out2 = io.StringIO()
    with patch("builtins.input", side_effect=["q"]), redirect_stdout(rts_out2):
        prompt_for_help(
            SyncIssue("mnemonic-mismatch", "test"),
            help_pending_trace,
            help_source,
            8,
            help_labels,
            rts_recent,
            6,
            rts_snapshots_unavail,
        )
    rts_text2 = rts_out2.getvalue()
    assert "Emulated stack before last shown RTS (top first):" in rts_text2
    assert "(snapshot unavailable)" in rts_text2

    # No RTS in recent trace -> no stack section.
    rts_out3 = io.StringIO()
    no_rts_snapshots: list[ReturnStackSnapshot | None] = [None] * len(help_recent_trace)
    with patch("builtins.input", side_effect=["q"]), redirect_stdout(rts_out3):
        prompt_for_help(
            help_issue,
            help_pending_trace,
            help_source,
            8,
            help_labels,
            help_recent_trace,
            6,
            no_rts_snapshots,
        )
    assert "Emulated stack before last shown RTS" not in rts_out3.getvalue()

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
