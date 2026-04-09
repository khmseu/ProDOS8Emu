#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

DISK_IMAGE="${DISK_IMAGE:-inputs/EDASM_SRC.2mg}"
ROM_IMAGE="${ROM_IMAGE:-inputs/apple_II.rom}"
CADIUS_BIN="${CADIUS_BIN:-/bigdata/KAI/projects/C-EDASM/third_party/cadius/bin/release/cadius}"
TMP_WORK_DIR="${TMP_WORK_DIR:-$(mktemp -d /tmp/edasm_regen.XXXXXX)}"
VOLUME_NAME="${VOLUME_NAME:-EDASM}"
EXTRACTED_SRC_DIR="$TMP_WORK_DIR/volumes/$VOLUME_NAME/EDASM.SRC"

cleanup() {
	if [[ -n "${TMP_WORK_DIR:-}" && -d "$TMP_WORK_DIR" && "$TMP_WORK_DIR" == /tmp/edasm_regen.* ]]; then
		rm -rf "$TMP_WORK_DIR"
	fi
}
trap cleanup EXIT

if [[ ! -x "$CADIUS_BIN" ]]; then
	echo "Error: cadius not executable at: $CADIUS_BIN" >&2
	echo "Set CADIUS_BIN to a valid path if needed." >&2
	exit 1
fi

if [[ ! -f "$DISK_IMAGE" ]]; then
	echo "Error: disk image not found: $DISK_IMAGE" >&2
	exit 1
fi

if [[ ! -f "$ROM_IMAGE" ]]; then
	echo "Error: ROM image not found: $ROM_IMAGE" >&2
	exit 1
fi

if [[ ! -x "tools/edasm_setup.py" ]]; then
	echo "Error: tools/edasm_setup.py is not executable" >&2
	exit 1
fi

echo "Removing existing EDASM.SRC directory..."
rm -rf EDASM.SRC

echo "Extracting disk image via tools/edasm_setup.py..."
./tools/edasm_setup.py \
	--cadius "$CADIUS_BIN" \
	--disk-image "$DISK_IMAGE" \
	--no-run \
	--rom "$ROM_IMAGE" \
	--work-dir "$TMP_WORK_DIR" >/dev/null

if [[ ! -d "$EXTRACTED_SRC_DIR" ]]; then
	echo "Error: expected source directory not found after extraction: $EXTRACTED_SRC_DIR" >&2
	exit 1
fi

echo "Copying .S files to workspace EDASM.SRC and converting CR to LF..."
while IFS= read -r src_file; do
	rel_path="${src_file#"$EXTRACTED_SRC_DIR/"}"
	dst_file="EDASM.SRC/$rel_path"
	mkdir -p "$(dirname "$dst_file")"
	perl -pe 's/\r\n/\n/g; s/\r/\n/g' <"$src_file" >"$dst_file"
done < <(find "$EXTRACTED_SRC_DIR" -type f -name "*.S" | sort)

mkdir -vp "$ROOT_DIR/EDASM.SRC/Monitor"
ln -sfv ../../../PEAdisasm/work/am_output.txt "$ROOT_DIR/EDASM.SRC/Monitor/Monitor.S"

echo "Running RTN marker insertion..."
./insert_rtn_markers.sh

echo "Done. Regenerated EDASM.SRC from $DISK_IMAGE"
