#!/bin/bash

# Insert RTN tracking markers in EDASM.SRC/**/*.S files
# For each RTN opcode found, inserts marker: _FILENAME_LINENUMBER EQU *

set -e # Exit on error

if [ ! -d "EDASM.SRC" ]; then
	echo "Error: EDASM.SRC directory not found"
	exit 1
fi

# Process all .S files in EDASM.SRC recursively
find EDASM.SRC -name "*.S" -type f | while read -r file; do
	basename_no_ext=$(basename "$file" .S)

	# Use awk to process the file line by line
	# Insert marker after any line containing RTN opcode
	awk -v basename="$basename_no_ext" '
    {
        print $0
        if ($0 ~ / RTN( |$)/) {
            printf "_%s_%d EQU *\n", basename, NR
        }
    }
    ' "$file" >"$file.tmp"

	# Check if changes were made
	if ! cmp -s "$file" "$file.tmp" 2>/dev/null; then
		mv "$file.tmp" "$file"
		markers=$(grep -c "^_${basename_no_ext}_[0-9]* EQU \*" "$file" 2>/dev/null || true)
		echo "Updated $file (+$markers markers)"
	else
		rm -f "$file.tmp"
	fi
done

echo "Processing complete."
