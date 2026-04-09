#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "prodos8emu/apple2mem.hpp"
#include "prodos8emu/cpu65c02.hpp"
#include "prodos8emu/memory.hpp"

static void writeProgram(prodos8emu::Apple2Memory& mem, uint16_t addr,
                         const std::initializer_list<uint8_t>& bytes) {
  uint16_t a = addr;
  for (uint8_t b : bytes) {
    prodos8emu::write_u8(mem.banks(), a, b);
    a = static_cast<uint16_t>(a + 1);
  }
}

static void disassembly_emits_mnemonic_and_operand_text_with_stable_order(int& failures) {
  std::cout << "Test 1: disassembly_emits_mnemonic_and_operand_text_with_stable_order\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2000;
  writeProgram(*mem, start,
               {
                   0xA9,
                   0x01,
                   0xAA,
                   0xEA,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  const uint32_t c1 = cpu.step();
  const uint32_t c2 = cpu.step();
  const uint32_t c3 = cpu.step();

  const std::string expected =
      "@1 PC=$2000 (L2000) OP=$A9 LDA #$01 ; PRE PC=$2000 A=$00 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2002 A=$01 X=$00 Y=$00 SP=$FF P=$24\n"
      "@2 PC=$2002 OP=$AA TAX ; PRE PC=$2002 A=$01 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2003 A=$01 X=$01 Y=$00 SP=$FF P=$24\n"
      "@3 PC=$2003 OP=$EA NOP ; PRE PC=$2003 A=$01 X=$01 Y=$00 SP=$FF P=$24 POST "
      "PC=$2004 A=$01 X=$01 Y=$00 SP=$FF P=$24\n";
  const std::string actual = disassemblyLog.str();

  if (c1 != 2 || c2 != 2 || c3 != 2) {
    std::cerr << "FAIL: Unexpected cycle counts for baseline instruction sequence\n";
    failures++;
  } else if (cpu.regs().pc != static_cast<uint16_t>(start + 4)) {
    std::cerr << "FAIL: Unexpected PC after executing 3 instructions\n";
    failures++;
  } else if (actual != expected) {
    std::cerr << "FAIL: Disassembly core output mismatch\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_emits_mnemonic_and_operand_text_with_stable_order\n";
  }
}

static void disassembly_resolves_known_symbol_for_absolute_operand(int& failures) {
  std::cout << "Test 2: disassembly_resolves_known_symbol_for_absolute_operand\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2200;
  writeProgram(*mem, start,
               {
                   0x8D,
                   0x60,
                   0x00,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  cpu.step();

  const std::string expected =
      "@1 PC=$2200 OP=$8D STA $0060 (BCDNbr) ; PRE PC=$2200 A=$00 X=$00 Y=$00 SP=$FF P=$24 "
      "POST PC=$2203 A=$00 X=$00 Y=$00 SP=$FF P=$24\n";
  const std::string actual = disassemblyLog.str();

  if (actual != expected) {
    std::cerr << "FAIL: Known absolute operand symbol should be appended\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_resolves_known_symbol_for_absolute_operand\n";
  }
}

static void disassembly_falls_back_to_hex_for_unmapped_absolute_operand(int& failures) {
  std::cout << "Test 3: disassembly_falls_back_to_hex_for_unmapped_absolute_operand\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2210;
  writeProgram(*mem, start,
               {
                   0x8D,
                   0x34,
                   0x12,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  cpu.step();

  const std::string expected =
      "@1 PC=$2210 OP=$8D STA $1234 ; PRE PC=$2210 A=$00 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2213 A=$00 X=$00 Y=$00 SP=$FF P=$24\n";
  const std::string actual = disassemblyLog.str();

  if (actual != expected) {
    std::cerr << "FAIL: Unmapped absolute operand should remain raw hex\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_falls_back_to_hex_for_unmapped_absolute_operand\n";
  }
}

static void disassembly_formats_mli_pseudo_instruction_for_jsr_bf00(int& failures) {
  std::cout << "Test 4: disassembly_formats_mli_pseudo_instruction_for_jsr_bf00\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2300;
  writeProgram(*mem, start,
               {
                   0x20,
                   0x00,
                   0xBF,
                   0xC8,
                   0xB0,
                   0x03,
                   0xEA,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  const uint32_t cycles = cpu.step();

  const std::string expected =
      "@1 PC=$2300 OP=$20 MLI .byte $C8 .word $03B0 (OPEN) ; PRE PC=$2300 A=$00 X=$00 Y=$00 "
      "SP=$FF P=$24 POST PC=$BF00 A=$00 X=$00 Y=$00 SP=$FD P=$24\n"
      "  STACK META[INSN=1 PHASE=POST OP=$20 PC=$2300 SP=$FD] SP=$FD USED=2: $01FE=$02 "
      "$01FF=$23\n";
  const std::string actual = disassemblyLog.str();

  if (cycles != 6) {
    std::cerr << "FAIL: JSR should retain original cycle count\n";
    failures++;
  } else if (actual != expected) {
    std::cerr << "FAIL: JSR $BF00 should format as one-line MLI pseudo-op\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_formats_mli_pseudo_instruction_for_jsr_bf00\n";
  }
}

static void disassembly_emits_stack_dump_after_jsr_line(int& failures) {
  std::cout << "Test 5: disassembly_emits_stack_dump_after_jsr_line\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2500;
  const uint16_t sub   = 0x2530;
  writeProgram(*mem, start,
               {
                   0x20,
                   static_cast<uint8_t>(sub & 0xFF),
                   static_cast<uint8_t>((sub >> 8) & 0xFF),
                   0xEA,
               });
  writeProgram(*mem, sub,
               {
                   0x60,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  const uint32_t cycles = cpu.step();

  const std::string expected =
      "@1 PC=$2500 OP=$20 JSR $2530 ; PRE PC=$2500 A=$00 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2530 A=$00 X=$00 Y=$00 SP=$FD P=$24\n"
      "  STACK META[INSN=1 PHASE=POST OP=$20 PC=$2500 SP=$FD] SP=$FD USED=2: $01FE=$02 "
      "$01FF=$25\n";
  const std::string actual = disassemblyLog.str();

  if (cycles != 6) {
    std::cerr << "FAIL: JSR should retain original cycle count\n";
    failures++;
  } else if (actual != expected) {
    std::cerr << "FAIL: JSR should emit a stack dump line after the disassembly line\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_emits_stack_dump_after_jsr_line\n";
  }
}

static void disassembly_emits_stack_dump_before_rts_line(int& failures) {
  std::cout << "Test 6: disassembly_emits_stack_dump_before_rts_line\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2510;
  const uint16_t sub   = 0x2540;
  writeProgram(*mem, start,
               {
                   0x20,
                   static_cast<uint8_t>(sub & 0xFF),
                   static_cast<uint8_t>((sub >> 8) & 0xFF),
                   0xEA,
               });
  writeProgram(*mem, sub,
               {
                   0x60,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  cpu.step();
  const uint32_t cycles = cpu.step();

  const std::string expected =
      "@1 PC=$2510 OP=$20 JSR $2540 ; PRE PC=$2510 A=$00 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2540 A=$00 X=$00 Y=$00 SP=$FD P=$24\n"
      "  STACK META[INSN=1 PHASE=POST OP=$20 PC=$2510 SP=$FD] SP=$FD USED=2: $01FE=$12 "
      "$01FF=$25\n"
      "  STACK META[INSN=2 PHASE=PRE OP=$60 PC=$2540 SP=$FD] SP=$FD USED=2: $01FE=$12 "
      "$01FF=$25\n"
      "@2 PC=$2540 OP=$60 RTS ; PRE PC=$2540 A=$00 X=$00 Y=$00 SP=$FD P=$24 POST "
      "PC=$2513 A=$00 X=$00 Y=$00 SP=$FF P=$24\n";
  const std::string actual = disassemblyLog.str();

  if (cycles != 6) {
    std::cerr << "FAIL: RTS should retain original cycle count\n";
    failures++;
  } else if (actual != expected) {
    std::cerr << "FAIL: RTS should emit a stack dump line before the disassembly line\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_emits_stack_dump_before_rts_line\n";
  }
}

static void disassembly_only_emits_while_sink_is_non_null(int& failures) {
  std::cout << "Test 7: disassembly_only_emits_while_sink_is_non_null\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2100;
  writeProgram(*mem, start,
               {
                   0xEA,
                   0xEA,
                   0xEA,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  cpu.step();
  cpu.setDisassemblyTraceLog(nullptr);
  cpu.step();
  cpu.step();

  const std::string expected =
      "@1 PC=$2100 OP=$EA NOP ; PRE PC=$2100 A=$00 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2101 A=$00 X=$00 Y=$00 SP=$FF P=$24\n";
  const std::string actual = disassemblyLog.str();

  if (actual != expected) {
    std::cerr << "FAIL: Disassembly should emit only while sink is configured\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_only_emits_while_sink_is_non_null\n";
  }
}

static void disassembly_register_comment_shows_p_and_sp_transitions(int& failures) {
  std::cout << "Test 8: disassembly_register_comment_shows_p_and_sp_transitions\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2400;
  writeProgram(*mem, start,
               {
                   0xA9,
                   0x00,
                   0x48,
                   0xC8,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  cpu.step();
  cpu.step();
  cpu.step();

  const std::string expected =
      "@1 PC=$2400 OP=$A9 LDA #$00 ; PRE PC=$2400 A=$00 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2402 A=$00 X=$00 Y=$00 SP=$FF P=$26\n"
      "@2 PC=$2402 OP=$48 PHA ; PRE PC=$2402 A=$00 X=$00 Y=$00 SP=$FF P=$26 POST "
      "PC=$2403 A=$00 X=$00 Y=$00 SP=$FE P=$26\n"
      "  STACK META[INSN=2 PHASE=POST OP=$48 PC=$2402 SP=$FE] SP=$FE USED=1: $01FF=$00\n"
      "@3 PC=$2403 OP=$C8 INY ; PRE PC=$2403 A=$00 X=$00 Y=$00 SP=$FE P=$26 POST "
      "PC=$2404 A=$00 X=$00 Y=$01 SP=$FE P=$24\n";
  const std::string actual = disassemblyLog.str();

  if (actual != expected) {
    std::cerr << "FAIL: Register comment should include PRE/POST snapshots for all registers\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_register_comment_shows_p_and_sp_transitions\n";
  }
}

static void disassembly_emits_stack_dump_around_pha_pla_pair(int& failures) {
  std::cout << "Test 9: disassembly_emits_stack_dump_around_pha_pla_pair\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0x2600;
  writeProgram(*mem, start,
               {
                   0xA9,
                   0x33,
                   0x48,
                   0x68,
                   0xC8,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  cpu.step();
  cpu.step();
  cpu.step();
  cpu.step();

  const std::string expected =
      "@1 PC=$2600 OP=$A9 LDA #$33 ; PRE PC=$2600 A=$00 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2602 A=$33 X=$00 Y=$00 SP=$FF P=$24\n"
      "@2 PC=$2602 OP=$48 PHA ; PRE PC=$2602 A=$33 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2603 A=$33 X=$00 Y=$00 SP=$FE P=$24\n"
      "  STACK META[INSN=2 PHASE=POST OP=$48 PC=$2602 SP=$FE] SP=$FE USED=1: $01FF=$33\n"
      "  STACK META[INSN=3 PHASE=PRE OP=$68 PC=$2603 SP=$FE] SP=$FE USED=1: $01FF=$33\n"
      "@3 PC=$2603 OP=$68 PLA ; PRE PC=$2603 A=$33 X=$00 Y=$00 SP=$FE P=$24 POST "
      "PC=$2604 A=$33 X=$00 Y=$00 SP=$FF P=$24\n"
      "@4 PC=$2604 OP=$C8 INY ; PRE PC=$2604 A=$33 X=$00 Y=$00 SP=$FF P=$24 POST "
      "PC=$2605 A=$33 X=$00 Y=$01 SP=$FF P=$24\n";
  const std::string actual = disassemblyLog.str();

  if (actual != expected) {
    std::cerr << "FAIL: PHA/PLA should emit post-push and pre-pop stack dump lines in order\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_emits_stack_dump_around_pha_pla_pair\n";
  }
}

static void disassembly_emits_known_pc_symbol_in_trace_header(int& failures) {
  std::cout << "Test 10: disassembly_emits_known_pc_symbol_in_trace_header\n";

  auto mem = std::make_unique<prodos8emu::Apple2Memory>();
  mem->setLCReadEnabled(true);
  mem->setLCWriteEnabled(true);

  const uint16_t start = 0xFC22;
  writeProgram(*mem, start,
               {
                   0xA5,
                   0x26,
               });
  prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

  prodos8emu::CPU65C02 cpu(*mem);
  std::stringstream    disassemblyLog;
  cpu.setDisassemblyTraceLog(&disassemblyLog);
  cpu.reset();

  cpu.step();

  const std::string expected =
      "@1 PC=$FC22 (LFC22) OP=$A5 LDA $26 ; PRE PC=$FC22 A=$00 X=$00 Y=$00 SP=$FF P=$24 "
      "POST PC=$FC24 A=$00 X=$00 Y=$00 SP=$FF P=$26\n";
  const std::string actual = disassemblyLog.str();

  if (actual != expected) {
    std::cerr << "FAIL: Known PC symbol should be appended in trace header\n"
              << "Expected:\n"
              << expected << "Actual:\n"
              << actual << "\n";
    failures++;
  } else {
    std::cout << "PASS: disassembly_emits_known_pc_symbol_in_trace_header\n";
  }
}

int main() {
  int failures = 0;

  disassembly_emits_mnemonic_and_operand_text_with_stable_order(failures);
  disassembly_resolves_known_symbol_for_absolute_operand(failures);
  disassembly_falls_back_to_hex_for_unmapped_absolute_operand(failures);
  disassembly_formats_mli_pseudo_instruction_for_jsr_bf00(failures);
  disassembly_emits_stack_dump_after_jsr_line(failures);
  disassembly_emits_stack_dump_before_rts_line(failures);
  disassembly_only_emits_while_sink_is_non_null(failures);
  disassembly_register_comment_shows_p_and_sp_transitions(failures);
  disassembly_emits_stack_dump_around_pha_pla_pair(failures);
  disassembly_emits_known_pc_symbol_in_trace_header(failures);

  if (failures > 0) {
    std::cerr << "\nFAILED with " << failures << " failure(s).\n";
    return 1;
  }

  std::cout << "\nAll disassembly core CPU tests passed.\n";
  return 0;
}
