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

static void disassembly_emits_one_line_per_instruction_with_stable_order(int& failures) {
  std::cout << "Test 1: disassembly_emits_one_line_per_instruction_with_stable_order\n";

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
      "@1 PC=$2000 OP=$A9\n"
      "@2 PC=$2002 OP=$AA\n"
      "@3 PC=$2003 OP=$EA\n";
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
    std::cout << "PASS: disassembly_emits_one_line_per_instruction_with_stable_order\n";
  }
}

static void disassembly_only_emits_while_sink_is_non_null(int& failures) {
  std::cout << "Test 2: disassembly_only_emits_while_sink_is_non_null\n";

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

  const std::string expected = "@1 PC=$2100 OP=$EA\n";
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

  disassembly_emits_one_line_per_instruction_with_stable_order(failures);
  disassembly_only_emits_while_sink_is_non_null(failures);

  if (failures > 0) {
    std::cerr << "\nFAILED with " << failures << " failure(s).\n";
    return 1;
  }

  std::cout << "\nAll disassembly core CPU tests passed.\n";
  return 0;
}
