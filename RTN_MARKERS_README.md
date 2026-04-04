# RTN Marker Insertion Script

## Overview

`insert_rtn_markers.sh` is a utility script that annotates EDASM assembly source files with RTN (return instruction) tracking markers. For each RTN opcode found in EDASM.SRC/**/*.S files, the script inserts a marker line that records the source filename and line number where the RTN occurred.

## Usage

```bash
./insert_rtn_markers.sh
```

The script must be run from the project root directory (where EDASM.SRC/ exists).

## What It Does

1. **Recursively scans** all `.S` files in the EDASM.SRC directory tree
2. **Detects RTN opcodes** using pattern matching: lines containing `RTN` (with leading space) followed by either a space or end-of-line
3. **Inserts tracking markers** immediately after each RTN in the format: `_FILENAME_LINENUMBER EQU *`
   - Example: RTN on line 15 of EDITOR1.S → inserts `_EDITOR1_15 EQU *`
4. **Modifies files in-place** with safe temporary file handling (checks if changes were made before overwriting)
5. **Reports results** showing which files were updated and how many markers were added

## Output Format

Each RTN instruction receives a marker line with:

- Underscore prefix: `_`
- Source filename (without .S extension): `FILENAME`
- Underscore separator: `_`
- Line number where RTN appears: `LINENUMBER`
- EQU directive: `EQU *`

Example from EDITOR1.S:

```asm
LD28A SET R4,XBD80 ;PN is in $BD80 buf
 RTN
_EDITOR1_15 EQU *
```

## Results (Latest Execution)

When executed on EDASM.SRC, the script produces:

- 93 total RTN tracking markers inserted
- 5 assembly source files modified:
  - EDITOR1.S: 72 markers
  - EDITOR3.S: 15 markers
  - EDITOR2.S: 4 markers
  - SWEET16.S: 1 marker
  - EDASMINT.S: 1 marker

## Technical Details

- **Language**: Bash shell script
- **Dependencies**: `find`, `awk`, `grep`, `cmp`, `mv`, `basename`
- **Pattern matching**: Uses awk regex `RTN( |$)` to match RTN opcodes with proper word boundaries
- **Safety**: Uses temporary files (.tmp) and compares before overwriting to avoid unnecessary changes
- **Encoding**: Handles files with various character encodings; grep errors on encoding mismatches are non-fatal

## Implementation Notes

- The script operates in a subshell for each file (piped find), which is intentional for resource efficiency
- Markers are inserted at the line-level immediately after RTN detection
- Files are only modified if changes are detected (cmp check)
- The script exits on any critical error (set -e)

## Purpose

These markers serve as tracking points for assembly analysis, allowing tools to:

- Map return instructions to their source locations
- Correlate execution traces back to specific assembly routines
- Support disassembly log analysis and alignment debugging
