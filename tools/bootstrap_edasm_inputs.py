#!/usr/bin/env python3
"""Bootstrap EDASM input artifacts into a fresh directory.

This script will:
1. Create an output directory
2. Download EDASM_SRC.2mg into that directory
3. Download apple_II.rom into that directory
4. Recreate edasm_src.json in that directory from an embedded template
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path

DEFAULT_DISK_URL = (
    "https://raw.githubusercontent.com/markpmlim/EdAsm/master/EDASM_SRC.2mg"
)
DEFAULT_ROM_URL = (
    "https://mirrors.apple2.org.za/ftp.apple.asimov.net/emulators/rom_images/apple.rom"
)

EDASM_SRC_CONFIG = {
    "rearrange": [
        {"from": "EDASM.SRC/BugByter/BUGBYTER", "to": "BUGBYTER"},
        {"from": "EDASM.SRC/BugByter/BUGBYTER.ORIG", "to": "BUGBYTER.ORIG"},
        {"from": "EDASM.SRC/ASM/EDASM.ASM.ORIG", "to": "EDASM.ASM"},
        {"from": "EDASM.SRC/EDITOR/EDASM.ED", "to": "EDASM.ED"},
        {"from": "EDASM.SRC/EI/EDASM.SYSTEM", "to": "EDASM.SYSTEM"},
        {"from": "EDASM.SRC/Linker/LINKER.ORIG", "to": "LINKER.ORIG"},
        {"from": "EDASM.SRC/Linker/LINKER", "to": "LINKER"},
    ]
}


def download_file(url: str, destination: Path) -> None:
    """Download a file to destination atomically."""
    destination.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile(
        prefix=f"{destination.name}.",
        suffix=".tmp",
        dir=destination.parent,
        delete=False,
    ) as tmp_file:
        temp_path = Path(tmp_file.name)

    try:
        with urllib.request.urlopen(url) as response, temp_path.open("wb") as out_file:
            shutil.copyfileobj(response, out_file)
        temp_path.replace(destination)
    except Exception:
        if temp_path.exists():
            temp_path.unlink()
        raise


def write_recreated_json(output_json: Path) -> None:
    """Write edasm_src.json from an embedded configuration template."""
    output_json.parent.mkdir(parents=True, exist_ok=True)
    with output_json.open("w", encoding="utf-8") as f:
        json.dump(EDASM_SRC_CONFIG, f, indent=4)
        f.write("\n")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create directory, download EDASM inputs, and recreate edasm_src.json",
    )
    parser.add_argument(
        "output_dir",
        help="Directory to create and populate",
    )
    parser.add_argument(
        "--disk-url",
        default=DEFAULT_DISK_URL,
        help="URL for EDASM_SRC.2mg",
    )
    parser.add_argument(
        "--rom-url",
        default=DEFAULT_ROM_URL,
        help="URL for apple_II.rom",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    output_dir = Path(args.output_dir)

    try:
        output_dir.mkdir(parents=True, exist_ok=True)

        disk_dest = output_dir / "EDASM_SRC.2mg"
        rom_dest = output_dir / "apple_II.rom"
        json_dest = output_dir / "edasm_src.json"

        print(f"Downloading {args.disk_url} -> {disk_dest}")
        download_file(args.disk_url, disk_dest)

        print(f"Downloading {args.rom_url} -> {rom_dest}")
        download_file(args.rom_url, rom_dest)

        print(f"Recreating {json_dest} from embedded template")
        write_recreated_json(json_dest)

        print("Done")
        return 0
    except urllib.error.URLError as e:
        print(f"Error: download failed: {e}", file=sys.stderr)
        return 1
    except OSError as e:
        print(f"Error: file operation failed: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
