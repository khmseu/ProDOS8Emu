#! /bin/bash

# Usage: ./do.sh [option]
# Options:
#   d - Build and run disassembler
#   m - Run external script from PEAdisasm project (requires ../PEAdisasm/work/am.sh)
#   a - Run disassembly log analyzer
#   v - Generate diff of annotated disassembly log
#   h - Show this help message

case "$1" in
d)
	cmake --build build --config Debug --target all
	./inputs/edasm.sh
	;;
m)
	AM_SCRIPT="${PRODOS8EMU_AM_SCRIPT:-../PEAdisasm/work/am.sh}"
	if [[ ! -x ${AM_SCRIPT} ]]; then
		echo "Error: am.sh script not found or not executable at '${AM_SCRIPT}'." >&2
		echo "Set PRODOS8EMU_AM_SCRIPT to override the default path if needed." >&2
		exit 1
	fi
	"${AM_SCRIPT}"
	;;
a)
	python3 tools/disassembly_log_analyzer.py
	;;
v)
	./tools/diff_continuation_lines.py prodos8emu_disassembly_trace.log prodos8emu_disassembly_trace.annotated.log >prodos8emu_disassembly_trace.diff
	;;
h)
	head -n 9 "$0" | tail -n +2
	;;
*)
	echo "Usage: $0 {a|d|m|v|h}"
	echo "Run '$0 h' for help."
	;;
esac
