## Phase 2 Complete: Implement Toggle and JSR/RTS Trace Emission

Implemented an opt-in JSR/RTS transition monitor with default-off behavior and integrated it into normal JSR/RTS control-flow paths. The monitor now emits old/new PC trace lines when enabled, excludes `$BF00` MLI trap transitions, and covers `$DCB8` redirect behavior through the normal JSR path.

**Files created/changed:**
- include/prodos8emu/cpu65c02.hpp
- src/cpu65c02.cpp

**Functions created/changed:**
- CPU65C02::setJsrRtsTraceMonitorEnabled
- CPU65C02::log_jsr_rts_transition
- CPU65C02::jsr_abs
- CPU65C02::execute_control_flow_jump_return_opcode

**Tests created/changed:**
- None (Phase 1 characterization tests used)
- Validated with: `prodos8emu_cpu65c02_tests`, `prodos8emu_emulator_startup_tests`

**Review Status:** APPROVED

**Git Commit Message:**
feat: add opt-in JSR/RTS trace monitor

- add CPU toggle for JSR/RTS transition monitor output
- emit old/new PC trace lines for normal JSR and RTS paths
- exclude MLI trap transitions while preserving default trace behavior
