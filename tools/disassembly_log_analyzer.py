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
import re
import tempfile
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
from typing import Deque, Iterable, Iterator


@dataclass(frozen=True)
class TraceEntry:
    index: int
    pc: int
    opcode: int
    mnemonic: str
    operand: str
    full_line: str


@dataclass(frozen=True)
class SourceInstruction:
    file_path: str
    line_number: int
    label: str | None
    mnemonic: str
    operand: str


@dataclass(frozen=True)
class SyncIssue:
    reason: str
    detail: str


TRACE_LINE_RE = re.compile(
    r"^@(?P<index>\d+)\s+PC=\$(?P<pc>[0-9A-Fa-f]{4})\s+"
    r"OP=\$(?P<op>[0-9A-Fa-f]{2})\s+(?P<mnemonic>[A-Za-z0-9]{3,4})\b(?P<tail>.*)$"
)

MONITOR_SYMBOL_RE = re.compile(
    r"\{\s*0x(?P<addr>[0-9A-Fa-f]+)\s*,\s*\"(?P<name>[^\"]+)\""
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


def normalize_mnemonic(token: str) -> str:
    text = token.upper().rstrip(":")
    if text.startswith(("BBR", "BBS", "RMB", "SMB")) and len(text) >= 4 and text[3:].isdigit():
        return text[:3]
    return text


def parse_log_line(line: str) -> TraceEntry | None:
    stripped = line.rstrip("\n")
    match = TRACE_LINE_RE.match(stripped)
    if match is None:
        return None

    tail = match.group("tail")
    operand = tail.split(";", 1)[0].strip()

    return TraceEntry(
        index=int(match.group("index")),
        pc=int(match.group("pc"), 16),
        opcode=int(match.group("op"), 16),
        mnemonic=normalize_mnemonic(match.group("mnemonic")),
        operand=operand,
        full_line=stripped,
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
        with source_file.open("r", encoding="utf-8", errors="replace") as handle:
            for line_number, line in enumerate(handle, start=1):
                parsed = parse_source_instruction_line(source_file, line_number, line)
                if parsed is not None:
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


def parse_trace_log(log_path: Path, sample_limit: int = 5) -> tuple[int, int, list[TraceEntry]]:
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


def _preview(entries: Iterable[object], limit: int = 3) -> list[object]:
    out: list[object] = []
    for entry in entries:
        out.append(entry)
        if len(out) >= limit:
            break
    return out


def build_ngram_index(source: list[SourceInstruction], window: int) -> dict[tuple[str, ...], list[int]]:
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


def prompt_for_help(
    issue: SyncIssue,
    pending_trace: list[TraceEntry],
    source: list[SourceInstruction],
    current_source_index: int | None,
    label_to_source_indexes: dict[str, list[int]],
) -> tuple[str, int | None]:
    print("\n=== Alignment Needs Help ===")
    print(f"Reason: {issue.reason}")
    print(issue.detail)

    print("\nPending trace entries:")
    for entry in pending_trace[:5]:
        print(f"  @{entry.index} PC=${entry.pc:04X} OP=${entry.opcode:02X} {entry.mnemonic} {entry.operand}".rstrip())

    if current_source_index is not None:
        lo = max(0, current_source_index - 2)
        hi = min(len(source), current_source_index + 3)
        print("\nSource context:")
        for idx in range(lo, hi):
            inst = source[idx]
            label = f"{inst.label} " if inst.label else ""
            pointer = "->" if idx == current_source_index else "  "
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
            matches = label_to_source_indexes.get(label.upper(), [])
            if not matches:
                print("Label not found in parsed source instructions.")
                continue
            if len(matches) > 1:
                print(f"Label has {len(matches)} matches, using first index {matches[0]}.")
            return "jump", matches[0]

        print("Unknown action. Use s, j <index>, l <label>, or q.")


def acquire_sync_window(
    pending: Deque[TraceEntry],
    ngram_index: dict[tuple[str, ...], list[int]],
    window: int,
) -> tuple[int | None, SyncIssue | None]:
    if len(pending) < window:
        return None, SyncIssue("insufficient-window", f"Need {window} trace entries, have {len(pending)}")

    key = tuple(entry.mnemonic for entry in list(pending)[:window])
    candidates = ngram_index.get(key, [])
    if len(candidates) == 1:
        return candidates[0], None
    if len(candidates) == 0:
        return None, SyncIssue("no-sync-candidate", f"No source window matches mnemonic key: {' '.join(key)}")
    return None, SyncIssue("ambiguous-sync-candidate", f"{len(candidates)} source windows match key: {' '.join(key)}")


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
        new_names = sorted(name for name in discovered[addr] if name not in existing_names)
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
            label_to_source_indexes[inst.label.upper()].append(idx)

    ngram_index = build_ngram_index(source, args.sync_window)
    if not ngram_index:
        print("Could not build source n-gram index. Check --sync-window and source corpus size.")
        return 1

    discovered: dict[int, set[str]] = defaultdict(set)
    processed = 0
    source_pos: int | None = None
    pending: Deque[TraceEntry] = deque()
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
                    pending.append(next(entry_iter))
                except StopIteration:
                    stop_reason = "eof"
                    break

            if not pending:
                break

            if not synced:
                sync_idx, issue = acquire_sync_window(pending, ngram_index, args.sync_window)
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
                    )
                    if action == "quit":
                        stop_reason = issue.reason
                        break
                    if action == "skip":
                        pending.popleft()
                        continue
                    if action == "jump" and value is not None:
                        source_pos = value
                        synced = True
                        continue
                    continue

                source_pos = sync_idx
                synced = True
                print(f"Synced at source index {source_pos} using window size {args.sync_window}.")

            # Synced mode: consume one trace instruction at a time.
            assert source_pos is not None
            if source_pos >= len(source):
                issue = SyncIssue("source-exhausted", "Reached end of source instruction stream.")
                if args.non_interactive:
                    stop_reason = issue.reason
                    break
                action, value = prompt_for_help(
                    issue,
                    list(pending),
                    source,
                    len(source) - 1,
                    label_to_source_indexes,
                )
                if action == "quit":
                    stop_reason = issue.reason
                    break
                if action == "skip":
                    pending.popleft()
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    continue
                continue

            trace_entry = pending[0]
            source_inst = source[source_pos]

            if not mnemonics_match(trace_entry.mnemonic, source_inst.mnemonic):
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
                )
                if action == "quit":
                    stop_reason = issue.reason
                    break
                if action == "skip":
                    pending.popleft()
                    processed += 1
                    continue
                if action == "jump" and value is not None:
                    source_pos = value
                    continue
                continue

            if source_inst.label:
                discovered[trace_entry.pc].add(source_inst.label)

            line_out = trace_entry.full_line
            names_here = sorted(discovered.get(trace_entry.pc, set()))
            if names_here:
                line_out = f"{line_out} ; NEW_PC_LABELS: {', '.join(names_here)}"

            annotated_out.write(line_out + "\n")

            pending.popleft()
            source_pos += 1
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

    mli_trace = parse_log_line("@1 PC=$1000 OP=$20 MLI .byte $C8 .word $1234 (OPEN) ; PRE X POST X")
    assert mli_trace is not None
    assert mli_trace.mnemonic == "MLI"

    no_operand_trace = parse_log_line("@7 PC=$2002 OP=$9A TXS ; PRE PC=$2002 POST PC=$2003")
    assert no_operand_trace is not None
    assert no_operand_trace.operand == ""

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
    print(f"    total symbol names (incl aliases): {sum(len(v) for v in symbols.values())}")

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
