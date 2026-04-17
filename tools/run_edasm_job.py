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
import datetime
import io
import os
import shutil
import subprocess  # nosec B404
import sys
import tarfile
from pathlib import Path

# Glob pattern matching all emulator-generated log files.
_EMULATOR_LOG_GLOB = "prodos8emu_*.log"


def remove_emulator_logs(repo_root: Path) -> None:
    """Delete any pre-existing emulator log files so only fresh logs survive."""
    for log_path in repo_root.glob(_EMULATOR_LOG_GLOB):
        log_path.unlink(missing_ok=True)


def _add_text_file_to_tar(tar: tarfile.TarFile, arcname: str, content: str) -> None:
    """Add an in-memory UTF-8 text file to a tar archive."""
    data = content.encode("utf-8")
    tar_info = tarfile.TarInfo(name=arcname)
    tar_info.size = len(data)
    tar.addfile(tar_info, io.BytesIO(data))


def _build_volumes_file_list(volumes_root: Path) -> str:
    """Build a sorted list of all files under the volumes root."""
    all_files = sorted(
        path.relative_to(volumes_root).as_posix()
        for path in volumes_root.rglob("*")
        if path.is_file()
    )
    if not all_files:
        return ""
    return "\n".join(all_files) + "\n"


def archive_job_outputs(
    repo_root: Path, work_dir: Path, inputs: list[str], invocation_argv: list[str]
) -> Path:
    """Create a compressed tar archive of OUT volume, ProDOS-format inputs, and emulator logs.

    The archive is placed in *repo_root* and named with a timestamp so successive
    runs never overwrite each other.

    Archive layout::

        OUT/          -- all files from work/volumes/OUT/
        inputs/       -- ProDOS-format input files from work/volumes/EDASM/
        logs/         -- all prodos8emu_*.log files
    """
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    archive_path = repo_root / f"edasm_job_{timestamp}.tar.gz"

    volumes_dir = work_dir / "volumes"
    out_dir = volumes_dir / "OUT"
    edasm_vol = work_dir / "volumes" / "EDASM"

    with tarfile.open(archive_path, "w:gz") as tar:
        # OUT volume
        if out_dir.exists():
            tar.add(out_dir, arcname="OUT")

        # ProDOS-format input files (imported into the EDASM volume with uppercased names)
        for input_path_str in inputs:
            prodos_name = Path(input_path_str).name.upper()
            prodos_file = edasm_vol / prodos_name
            if prodos_file.exists():
                tar.add(prodos_file, arcname=f"inputs/{prodos_name}")

        # Emulator logs
        for log_path in sorted(repo_root.glob(_EMULATOR_LOG_GLOB)):
            tar.add(log_path, arcname=f"logs/{log_path.name}")

        # Invocation arguments passed to run_edasm_job.py
        invocation_lines = ["run_edasm_job.py invocation arguments:"]
        invocation_lines.extend(invocation_argv)
        _add_text_file_to_tar(
            tar,
            "metadata/run_edasm_job_arguments.txt",
            "\n".join(invocation_lines) + "\n",
        )

        # Full sorted file list under work/volumes
        _add_text_file_to_tar(
            tar,
            "metadata/volumes_file_list.txt",
            _build_volumes_file_list(volumes_dir),
        )

    return archive_path


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
    parser.add_argument(
        "--max-instructions",
        type=int,
        help="Maximum instructions to execute (passed to emulator runner)",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable emulator debug logs (passed to edasm_setup.py)",
    )
    return parser.parse_args(argv)


def write_autost(
    autost_path: Path, source_file: str, listing_file: str, output_file: str
) -> None:
    """Write the EdAsm.AutoST command script."""
    autost_path.parent.mkdir(parents=True, exist_ok=True)
    source_upper = source_file.upper()
    listing_upper = listing_file.upper()
    output_upper = output_file.upper()
    content = (
        f"PR#1,/OUT/{listing_upper}\n"
        f"ASM {source_upper},/OUT/{output_upper}\n"
        "END\n"
    )
    autost_path.write_text(content, encoding="ascii")


def reset_work_dir(work_dir: Path) -> Path:
    """Remove and recreate work directory layout, returning OUT directory path."""
    if work_dir.exists():
        shutil.rmtree(work_dir)
    out_dir = work_dir / "volumes" / "OUT"
    out_dir.mkdir(parents=True, exist_ok=True)
    return out_dir


def run_edasm_setup(
    inputs: list[str],
    work_dir: Path,
    max_instructions: int | None = None,
    debug: bool = False,
) -> None:
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

    if max_instructions is not None:
        cmd.extend(["--max-instructions", str(max_instructions)])

    if debug:
        cmd.append("--debug")

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
    invocation_argv = list(sys.argv[1:] if argv is None else argv)

    source_file = Path(args.input[0]).name
    autost_path = Path("inputs") / "EdAsm.AutoST"
    work_dir = Path(args.work_dir)

    repo_root = Path(__file__).resolve().parent.parent

    try:
        write_autost(autost_path, source_file, args.listing, args.output)
        out_dir = reset_work_dir(work_dir)
        remove_emulator_logs(repo_root)
        run_edasm_setup(args.input, work_dir, args.max_instructions, args.debug)
        archive_path = archive_job_outputs(
            repo_root, work_dir, args.input, invocation_argv
        )
        print(f"Job archive: {archive_path}")
        convert_out_text_files(out_dir)
        return 0
    except (OSError, RuntimeError) as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
