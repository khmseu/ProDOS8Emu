# Plan: Disassembly Log Analyzer With Interactive Resync

Build a Python tool that streams `prodos8emu_disassembly_trace.log`, parses EDASM source instructions from `EDASM.SRC/**/*.S`, auto-syncs trace mnemonics to source mnemonics, discovers new labels for traced PCs, and pauses for user guidance when sync is lost or ambiguous. The tool will emit a `kMonitorSymbols`-ready label list and an annotated log up to the point where user assistance is needed.

## Phases 4

1. **Phase 1: Parser and Index Foundation**
   - **Objective:** Parse trace lines, source instruction lines, and existing monitor symbols to build baseline indexes.
   - **Files/Functions to Modify/Create:** `tools/disassembly_log_analyzer.py` (`parse_log_line`, `parse_source_tree`, `parse_existing_monitor_symbols`, CLI `--dry-run`).
   - **Tests to Write:** Script dry-run/self-check output validation for parsed counts and sample records.
   - **Steps:**
     1. Implement robust trace-line parser for current disassembly format.
     2. Implement assembler source parser for labels/mnemonics in `EDASM.SRC` tree.
     3. Parse `kMonitorSymbols` from `src/cpu65c02.cpp` and expose alias-aware lookup.

2. **Phase 2: Auto-Sync Engine**
   - **Objective:** Align trace instruction stream with source instruction stream using mnemonic windows and unique candidate selection.
   - **Files/Functions to Modify/Create:** `tools/disassembly_log_analyzer.py` (`build_ngram_index`, sync state machine, alignment scoring).
   - **Tests to Write:** Bounded run test (`--max-lines`) proving sync acquisition and continuation for initial segment.
   - **Steps:**
     1. Build mnemonic n-gram index over source stream.
     2. Stream trace lines and acquire sync windows.
     3. Continue synced matching and detect mismatch/ambiguity boundaries.

3. **Phase 3: Interactive Help + Partial Artifacts**
   - **Objective:** Ask user for help on ambiguous continuation or opcode mismatch and stop/continue deterministically.
   - **Files/Functions to Modify/Create:** `tools/disassembly_log_analyzer.py` (`prompt_for_help`, manual resync controls, safe stop).
   - **Tests to Write:** Interactive/manual flow test exercising ambiguity prompt and controlled stop.
   - **Steps:**
     1. Add prompt actions: manual source jump, skip, retry window, quit.
     2. Persist partial annotated log through last confirmed synced line.
     3. Persist unresolved context snapshot for next run.

4. **Phase 4: Label Discovery Output + Annotation Insertion**
   - **Objective:** Emit discovered labels (including aliases) and annotate output log lines with newly found names.
   - **Files/Functions to Modify/Create:** `tools/disassembly_log_analyzer.py` (label collector, output writers).
   - **Tests to Write:** Bounded run test generating non-empty `kMonitorSymbols` snippet and annotated log file.
   - **Steps:**
     1. Collect source labels for matched PCs not yet present in existing symbols.
     2. Emit insertion-ready lines like `{0xB6E6, "DoAsmbly", MonitorSymbolPc},` and include aliases.
     3. Write `prodos8emu_disassembly_trace.annotated.log` up to sync-loss/help point.

## Open Questions 0

1. None. Accepted defaults: output names are fine, and alias labels should be retained.
