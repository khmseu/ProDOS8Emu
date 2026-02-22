## Phase 5 Complete: Emulator Initialization

Updated the `prodos8emu_run` CLI tool to construct the core emulator components (`Apple2Memory`, `MLIContext`, `CPU65C02`) and attach the MLI trap, preparing for the actual boot/run pipeline in the next phase.

**Files created/changed:**

- tools/prodos8emu_run.cpp

**Functions created/changed:**

- Emulator component initialization and MLI attachment in `prodos8emu_run`

**Tests created/changed:**

- No new tests (wiring only)

**Review Status:** APPROVED

**Git Commit Message:**
feat: Wire CLI to emulator components

- Instantiate Apple2Memory, MLIContext, and CPU65C02 in prodos8emu_run
- Attach MLI context to CPU for JSR $BF00 trapping
- Default volume root to current directory when not provided
