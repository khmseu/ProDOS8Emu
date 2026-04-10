#!/usr/bin/env python3
"""Run EDASM via emulator for given source inputs and collect outputs.

Workflow:
1. Build inputs/EdAsm.AutoST with PR#1 and ASM commands
2. Reset work directory and ensure work/volumes/OUT exists
3. Invoke tools/edasm_setup.py with required arguments
4. Convert text files in work/volumes/OUT using tools/prodos_text_to_linux.py
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess  # nosec B404
import sys
from pathlib import Path


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run EDASM through emulator and convert OUT text artifacts",
    )
    parser.add_argument(
        "--input",
        action="append",
        required=True,
        help="Input text file to import (repeatable). First input is used for ASM command.",
    )
    parser.add_argument(
        "--listing",
        required=True,
        help="Listing filename to write in /OUT (used in PR#1 command)",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output filename to write in /OUT (used in ASM command)",
    )
    parser.add_argument(
        "--work-dir",
        default="work",
        help="Work directory to reset and use (default: work)",
    )
    return parser.parse_args(argv)


def write_autost(
    autost_path: Path, source_file: str, listing_file: str, output_file: str
) -> None:
    """Write the EdAsm.AutoST command script."""
    autost_path.parent.mkdir(parents=True, exist_ok=True)
    content = (
        f"PR#1,/OUT/{listing_file}\n" f"ASM {source_file},/OUT/{output_file}\n" "END\n"
    )
    autost_path.write_text(content, encoding="ascii")


def reset_work_dir(work_dir: Path) -> Path:
    """Remove and recreate work directory layout, returning OUT directory path."""
    if work_dir.exists():
        shutil.rmtree(work_dir)
    out_dir = work_dir / "volumes" / "OUT"
    out_dir.mkdir(parents=True, exist_ok=True)
    return out_dir


def run_edasm_setup(inputs: list[str], work_dir: Path) -> None:
    """Invoke tools/edasm_setup.py with fixed required parameters."""
    repo_root = Path(__file__).resolve().parent.parent
    edasm_setup = repo_root / "tools" / "edasm_setup.py"

    cmd = [
        sys.executable,
        str(edasm_setup),
        "--cadius",
        "/bigdata/KAI/projects/C-EDASM/third_party/cadius/bin/release/cadius",
        "--disk-image",
        "inputs/EDASM_SRC.2mg",
        "--rearrange-config",
        "inputs/edasm_src.json",
        "--rom",
        "inputs/apple_II.rom",
        "--text",
        "inputs/EdAsm.AutoST",
        "--work-dir",
        str(work_dir),
    ]

    for path in inputs:
        cmd.extend(["--text", path])

    result = subprocess.run(cmd, cwd=repo_root, check=False)  # nosec B603
    if result.returncode != 0:
        raise RuntimeError(f"edasm_setup.py failed with exit code {result.returncode}")


def is_prodos_text_file(path: Path) -> bool:
    """Return True when file has ProDOS text file type xattr (04)."""
    try:
        value = os.getxattr(path, "user.prodos8.file_type")
    except OSError:
        return False

    file_type = value.decode("ascii", errors="ignore").strip().lower()
    return file_type == "04"


def convert_out_text_files(out_dir: Path) -> None:
    """Convert text files in OUT from ProDOS CR to Linux LF."""
    repo_root = Path(__file__).resolve().parent.parent
    converter = repo_root / "tools" / "prodos_text_to_linux.py"

    for file_path in sorted(out_dir.rglob("*")):
        if not file_path.is_file():
            continue
        if not is_prodos_text_file(file_path):
            continue

        cmd = [sys.executable, str(converter), str(file_path)]
        result = subprocess.run(cmd, cwd=repo_root, check=False)  # nosec B603
        if result.returncode != 0:
            raise RuntimeError(
                f"prodos_text_to_linux.py failed for {file_path} with exit code {result.returncode}"
            )


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    source_file = Path(args.input[0]).name
    autost_path = Path("inputs") / "EdAsm.AutoST"
    work_dir = Path(args.work_dir)

    try:
        write_autost(autost_path, source_file, args.listing, args.output)
        out_dir = reset_work_dir(work_dir)
        run_edasm_setup(args.input, work_dir)
        convert_out_text_files(out_dir)
        return 0
    except (OSError, RuntimeError) as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
