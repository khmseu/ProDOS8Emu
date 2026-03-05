## Phase 3 Complete: Refactor BIT and Bit-Opcode Families

Phase 3 centralized BIT/TSB/TRB mode handling behind shared metadata/helpers and unified RMB/SMB/BBR/BBS paths with shared bit primitives while preserving execution behavior. New characterization-equivalence tests lock route parity, bit-index mapping, branch behavior, and runtime contracts across all bit indices.

**Files created/changed:**

- `include/prodos8emu/cpu65c02.hpp`
- `src/cpu65c02.cpp`
- `tests/cpu65c02_test.cpp`

**Functions created/changed:**

- `CPU65C02::read_bit_operand_for_mode(BitFamilyMode mode, uint8_t& operand, uint32_t& cycles)`
- `CPU65C02::read_bit_modify_target_for_mode(BitFamilyMode mode, uint16_t& addr, uint32_t& cycles)`
- `CPU65C02::apply_bit_test_flags(uint8_t operand, bool updateNV)`
- `CPU65C02::execute_bit_family_opcode(uint8_t op, uint32_t& cycles)`
- `CPU65C02::bit_index_from_opcode(uint8_t op)`
- `CPU65C02::bit_mask_for_index(uint8_t bitIndex)`
- `CPU65C02::apply_rmb_smb_bit(uint8_t value, uint8_t bitIndex, bool setBit)`
- `CPU65C02::test_bit_index(uint8_t value, uint8_t bitIndex)`
- `CPU65C02::execute_rmb_smb_opcode(uint8_t op)`
- `CPU65C02::execute_bbr_bbs_opcode(uint8_t op)`

**Tests created/changed:**

- `bit_family_mode_metadata_equivalence`
- `bit_opcode_shared_primitive_equivalence`
- Target run: `prodos8emu_cpu65c02_tests` ✅
- Target run: `prodos8emu_emulator_startup_tests` ✅

**Review Status:** APPROVED

**Git Commit Message:**
refactor: unify bit family helper routing

- centralize BIT/TSB/TRB decode through shared mode metadata and helpers
- unify RMB/SMB/BBR/BBS behavior with shared bit-index primitives
- add non-tautological equivalence tests for bit family routing contracts
- preserve cycles, PC behavior, branch outcomes, and decode precedence
- validate cpu65c02 and emulator-startup ctest targets
