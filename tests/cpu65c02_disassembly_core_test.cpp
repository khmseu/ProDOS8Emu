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
      "@1 PC=$2000 OP=$A9 LDA #$01\n"
      "@2 PC=$2002 OP=$AA TAX\n"
      "@3 PC=$2003 OP=$EA NOP\n";
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

  const std::string expected = "@1 PC=$2200 OP=$8D STA $0060 (BCDNbr)\n";
  const std::string actual   = disassemblyLog.str();

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

  const std::string expected = "@1 PC=$2210 OP=$8D STA $1234\n";
  const std::string actual   = disassemblyLog.str();

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

  const std::string expected = "@1 PC=$2300 OP=$20 MLI .byte $C8 .word $03B0 (OPEN)\n";
  const std::string actual   = disassemblyLog.str();

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

static void disassembly_only_emits_while_sink_is_non_null(int& failures) {
  std::cout << "Test 5: disassembly_only_emits_while_sink_is_non_null\n";

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

  const std::string expected = "@1 PC=$2100 OP=$EA NOP\n";
  const std::string actual   = disassemblyLog.str();

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

int main() {
  int failures = 0;

  disassembly_emits_mnemonic_and_operand_text_with_stable_order(failures);
  disassembly_resolves_known_symbol_for_absolute_operand(failures);
  disassembly_falls_back_to_hex_for_unmapped_absolute_operand(failures);
  disassembly_formats_mli_pseudo_instruction_for_jsr_bf00(failures);
  disassembly_only_emits_while_sink_is_non_null(failures);

  if (failures > 0) {
    std::cerr << "\nFAILED with " << failures << " failure(s).\n";
    return 1;
  }

  std::cout << "\nAll disassembly core CPU tests passed.\n";
  return 0;
}
