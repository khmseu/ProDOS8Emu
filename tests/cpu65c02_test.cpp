#include "prodos8emu/cpu65c02.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "prodos8emu/apple2mem.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"

namespace fs = std::filesystem;

static void writeProgram(prodos8emu::Apple2Memory& mem, uint16_t addr,
                         const std::initializer_list<uint8_t>& bytes) {
  uint16_t a = addr;
  for (uint8_t b : bytes) {
    prodos8emu::write_u8(mem.banks(), a, b);
    a = static_cast<uint16_t>(a + 1);
  }
}

__attribute__((noinline)) static void run_branch_opcode_matrix_preserved_test(int& failures) {
  std::cout << "Test 43: branch_opcode_matrix_preserved\n";

  bool testFailed = false;

  struct BranchCase {
    const char* name;
    uint8_t     opcode;
    uint8_t     flagMask;
    bool        takenWhenSet;
  };

  static const BranchCase cases[] = {
      {"BPL", 0x10, 0x80, false},
      {"BMI", 0x30, 0x80, true},
      {"BVC", 0x50, 0x40, false},
      {"BVS", 0x70, 0x40, true},
      {"BCC", 0x90, 0x01, false},
      {"BCS", 0xB0, 0x01, true},
      {"BNE", 0xD0, 0x02, false},
      {"BEQ", 0xF0, 0x02, true},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])) && !testFailed; ++i) {
    const BranchCase& c = cases[i];
    for (int scenario = 0; scenario < 2 && !testFailed; ++scenario) {
      const bool expectedTaken = (scenario == 0);

      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = static_cast<uint16_t>(0x1700 + (i * 0x10) + (scenario * 0x04));
      writeProgram(mem, start,
                   {
                       c.opcode,
                       0x02,
                       0xEA,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint8_t startP = 0x20;
      if (expectedTaken == c.takenWhenSet) {
        startP = static_cast<uint8_t>(startP | c.flagMask);
      } else {
        startP = static_cast<uint8_t>(startP & ~c.flagMask);
      }
      cpu.regs().p = startP;

      uint32_t cycles     = cpu.step();
      uint16_t expectedPC = static_cast<uint16_t>(expectedTaken ? (start + 4) : (start + 2));
      if (cycles != 2 || cpu.regs().pc != expectedPC || cpu.regs().p != startP) {
        std::cerr << "FAIL: " << c.name << " " << (expectedTaken ? "taken" : "not-taken")
                  << " contract mismatch (cycles=" << cycles << ", pc=0x" << std::hex
                  << cpu.regs().pc << ", p=0x" << static_cast<uint32_t>(cpu.regs().p) << std::dec
                  << ")\n";
        failures++;
        testFailed = true;
      }
    }
  }

  if (!testFailed) {
    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start = 0x1790;
    writeProgram(mem, start,
                 {
                     0x80,
                     0x02,
                     0xEA,
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.reset();
    cpu.regs().p = 0xE3;

    uint32_t cycles = cpu.step();
    if (cycles != 3 || cpu.regs().pc != static_cast<uint16_t>(start + 4) || cpu.regs().p != 0xE3) {
      std::cerr << "FAIL: BRA unconditional branch contract mismatch\n";
      failures++;
      testFailed = true;
    }
  }

  if (!testFailed) {
    std::cout << "PASS: branch_opcode_matrix_preserved\n";
  }
}

__attribute__((noinline)) static void run_cout_escape_mapping_contracts_preserved_test(int& failures) {
  std::cout << "Test 44: cout_escape_mapping_contracts_preserved\n";

  bool testFailed = false;

  struct CoutCase {
    const char* name;
    uint8_t     aValue;
    const char* expected;
  };

  static const CoutCase cases[] = {
      {"printable_high_bit_clear", 0xC1, "A"},
      {"carriage_return_newline", 0x0D, "\n"},
      {"nul_escape", 0x00, "\\0"},
      {"bel_escape", 0x07, "\\a"},
      {"backspace_escape", 0x08, "\\b"},
      {"tab_escape", 0x09, "\\t"},
      {"line_feed_escape", 0x0A, "\\n"},
      {"vertical_tab_escape", 0x0B, "\\v"},
      {"form_feed_escape", 0x0C, "\\f"},
      {"escape_escape", 0x1B, "\\e"},
      {"del_escape", 0x7F, "\\x7f"},
      {"default_hex_escape", 0x01, "\\x01"},
      {"default_hex_escape_high_bit_clear", 0x9A, "\\x1A"},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])) && !testFailed; ++i) {
    const CoutCase& c = cases[i];

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start = static_cast<uint16_t>(0x17C0 + (i * 0x10));
    writeProgram(mem, start,
                 {
                     0xA9,
                     c.aValue,
                     0x6C,
                     0x36,
                     0x00,
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0x0036, static_cast<uint16_t>(start + 5));
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    std::stringstream    coutLog;
    cpu.setDebugLogs(nullptr, &coutLog);
    cpu.reset();

    uint32_t ldaCycles = cpu.step();
    uint32_t jmpCycles = cpu.step();
    uint32_t nopCycles = cpu.step();

    const std::string got = coutLog.str();
    if (ldaCycles != 2 || jmpCycles != 5 || nopCycles != 2 || got != c.expected ||
        cpu.regs().pc != static_cast<uint16_t>(start + 6)) {
      std::cerr << "FAIL: COUT mapping contract mismatch for " << c.name << " (got='";
      for (char ch : got) {
        if (ch == '\n') {
          std::cerr << "\\n";
        } else if (ch == '\\') {
          std::cerr << "\\\\";
        } else if (ch >= 0x20 && ch <= 0x7E) {
          std::cerr << ch;
        } else {
          std::cerr << "\\x" << std::hex << static_cast<int>(static_cast<uint8_t>(ch)) << std::dec;
        }
      }
      std::cerr << "', expected='" << c.expected << "')\n";
      failures++;
      testFailed = true;
    }
  }

  if (!testFailed) {
    std::cout << "PASS: cout_escape_mapping_contracts_preserved\n";
  }
}

__attribute__((noinline)) static void run_unknown_opcode_default_fallback_contracts_test(
    int& failures) {
  std::cout << "Test 45: unknown_opcode_default_fallback_contracts\n";

  bool testFailed = false;

  struct DefaultLikeCase {
    const char* name;
    uint8_t     opcode;
    uint32_t    expectedCycles;
  };

  static const DefaultLikeCase cases[] = {
      {"canonical_nop", 0xEA, 2},
      {"reserved_slot_nop_a", 0x03, 1},
      {"reserved_slot_nop_b", 0xFB, 1},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])) && !testFailed; ++i) {
    const DefaultLikeCase& c = cases[i];

    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    const uint16_t start = static_cast<uint16_t>(0x1800 + (i * 0x10));
    writeProgram(*mem, start,
                 {
                     c.opcode,
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();
    cpu.regs().a  = 0x12;
    cpu.regs().x  = 0x34;
    cpu.regs().y  = 0x56;
    cpu.regs().sp = 0xE1;
    cpu.regs().p  = 0xA5;

    const uint32_t cycles = cpu.step();
    if (cycles != c.expectedCycles || cpu.regs().pc != static_cast<uint16_t>(start + 1) ||
        cpu.regs().a != 0x12 || cpu.regs().x != 0x34 || cpu.regs().y != 0x56 ||
        cpu.regs().sp != 0xE1 || cpu.regs().p != 0xA5) {
      std::cerr << "FAIL: default-like fallback contract mismatch for " << c.name << "\n";
      failures++;
      testFailed = true;
    }
  }

  if (!testFailed) {
    std::cout << "PASS: unknown_opcode_default_fallback_contracts\n";
  }
}

__attribute__((noinline)) static void run_nop_variant_opcode_matrix_preserved_test(int& failures) {
  std::cout << "Test 46: nop_variant_opcode_matrix_preserved\n";

  bool testFailed = false;

  static const uint8_t oneByteNops[] = {
      0x03, 0x0B, 0x13, 0x1B, 0x23, 0x2B, 0x33, 0x3B, 0x43, 0x4B,
      0x53, 0x5B, 0x63, 0x6B, 0x73, 0x7B, 0x83, 0x8B, 0x93, 0x9B,
      0xA3, 0xAB, 0xB3, 0xBB, 0xC3, 0xD3, 0xE3, 0xEB, 0xF3, 0xFB,
  };

  static const uint8_t immediateNops[] = {
      0x02,
      0x22,
      0x42,
      0x62,
      0x82,
      0xC2,
      0xE2,
  };

  struct NopCase {
    const char* name;
    uint8_t     opcode;
    uint8_t     operandLo;
    uint8_t     operandHi;
    uint8_t     initialX;
    uint32_t    expectedCycles;
    uint16_t    expectedPcAdvance;
    uint16_t    observedAddress;
    uint8_t     observedValue;
  };

  auto runCase = [&](const NopCase& c, uint16_t start) {
    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    if (c.observedAddress != 0) {
      prodos8emu::write_u8(mem->banks(), c.observedAddress, c.observedValue);
    }

    prodos8emu::write_u8(mem->banks(), start, c.opcode);
    prodos8emu::write_u8(mem->banks(), static_cast<uint16_t>(start + 1), c.operandLo);
    prodos8emu::write_u8(mem->banks(), static_cast<uint16_t>(start + 2), c.operandHi);
    prodos8emu::write_u8(mem->banks(), static_cast<uint16_t>(start + 3), 0xEA);
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();
    cpu.regs().a  = 0x21;
    cpu.regs().x  = c.initialX;
    cpu.regs().y  = 0x43;
    cpu.regs().sp = 0xE2;
    cpu.regs().p  = 0x65;

    uint32_t cycles = cpu.step();
    if (cycles != c.expectedCycles ||
        cpu.regs().pc != static_cast<uint16_t>(start + c.expectedPcAdvance) ||
        cpu.regs().a != 0x21 || cpu.regs().x != c.initialX || cpu.regs().y != 0x43 ||
        cpu.regs().sp != 0xE2 || cpu.regs().p != 0x65) {
      std::cerr << "FAIL: NOP matrix cycle/PC/non-mutation mismatch for " << c.name << "\n";
      failures++;
      testFailed = true;
      return;
    }

    if (c.observedAddress != 0 && prodos8emu::read_u8(mem->constBanks(), c.observedAddress) !=
                                    c.observedValue) {
      std::cerr << "FAIL: NOP matrix memory-stability mismatch for " << c.name << "\n";
      failures++;
      testFailed = true;
    }
  };

  uint16_t start = 0x1880;
  for (size_t i = 0; i < (sizeof(oneByteNops) / sizeof(oneByteNops[0])) && !testFailed; ++i) {
    runCase({"one_byte", oneByteNops[i], 0x99, 0x77, 0x09, 1, 1, 0, 0}, start);
    start = static_cast<uint16_t>(start + 0x10);
  }

  for (size_t i = 0; i < (sizeof(immediateNops) / sizeof(immediateNops[0])) && !testFailed; ++i) {
    runCase({"immediate", immediateNops[i], 0x99, 0x77, 0x09, 2, 2, 0, 0}, start);
    start = static_cast<uint16_t>(start + 0x10);
  }

  if (!testFailed) {
    runCase({"zp_read", 0x44, 0x44, 0x77, 0x09, 3, 2, 0x0044, 0xAB}, start);
    start = static_cast<uint16_t>(start + 0x10);
  }

  if (!testFailed) {
    runCase({"zpx_read_54", 0x54, 0x45, 0x77, 0x01, 4, 2, 0x0046, 0xBC}, start);
    start = static_cast<uint16_t>(start + 0x10);
  }
  if (!testFailed) {
    runCase({"zpx_read_D4", 0xD4, 0x45, 0x77, 0x01, 4, 2, 0x0046, 0xBC}, start);
    start = static_cast<uint16_t>(start + 0x10);
  }
  if (!testFailed) {
    runCase({"zpx_read_F4", 0xF4, 0x45, 0x77, 0x01, 4, 2, 0x0046, 0xBC}, start);
    start = static_cast<uint16_t>(start + 0x10);
  }

  if (!testFailed) {
    runCase({"abs_read_DC", 0xDC, 0x45, 0x23, 0x09, 4, 3, 0x2345, 0xCD}, start);
    start = static_cast<uint16_t>(start + 0x10);
  }
  if (!testFailed) {
    runCase({"abs_read_FC", 0xFC, 0x67, 0x45, 0x09, 4, 3, 0x4567, 0xD7}, start);
    start = static_cast<uint16_t>(start + 0x10);
  }

  if (!testFailed) {
    runCase({"abs_long_read_5C", 0x5C, 0x56, 0x34, 0x09, 8, 3, 0x3456, 0xDE}, start);
  }

  if (!testFailed) {
    std::cout << "PASS: nop_variant_opcode_matrix_preserved\n";
  }
}

__attribute__((noinline)) static void run_fallback_router_precedence_contracts_test(int& failures) {
  std::cout << "Test 47: fallback_router_precedence_contracts\n";

  bool testFailed = false;

  {
    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    const uint16_t start = 0x1E00;
    prodos8emu::write_u8(mem->banks(), 0x0010, 0xFF);
    writeProgram(*mem, start,
                 {
                     0x07,
                     0x10,
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();
    cpu.regs().a = 0x11;
    cpu.regs().x = 0x22;
    cpu.regs().y = 0x33;
    cpu.regs().p = 0xE5;

    uint32_t cycles = cpu.step();
    if (cycles != 5 || cpu.regs().pc != static_cast<uint16_t>(start + 2) ||
        prodos8emu::read_u8(mem->constBanks(), 0x0010) != 0xFE || cpu.regs().a != 0x11 ||
        cpu.regs().x != 0x22 || cpu.regs().y != 0x33 || cpu.regs().p != 0xE5) {
      std::cerr << "FAIL: router precedence mismatch for RMB special decode path\n";
      failures++;
      testFailed = true;
    }
  }

  {
    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    const uint16_t start = 0x1E20;
    prodos8emu::write_u8(mem->banks(), 0x0011, 0x00);
    writeProgram(*mem, start,
                 {
                     0x0F,
                     0x11,
                     0x02,
                     0xEA,
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();
    cpu.regs().p = 0x61;

    uint32_t cycles = cpu.step();
    if (!testFailed &&
        (cycles != 5 || cpu.regs().pc != static_cast<uint16_t>(start + 5) || cpu.regs().p != 0x61)) {
      std::cerr << "FAIL: router precedence mismatch for BBR special decode path\n";
      failures++;
      testFailed = true;
    }
  }

  {
    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    const uint16_t start = 0x1E40;
    writeProgram(*mem, start,
                 {
                     0x03,
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();
    cpu.regs().a = 0x77;
    cpu.regs().x = 0x88;
    cpu.regs().y = 0x99;
    cpu.regs().p = 0x27;

    uint32_t cycles = cpu.step();
    if (!testFailed &&
        (cycles != 1 || cpu.regs().pc != static_cast<uint16_t>(start + 1) || cpu.regs().a != 0x77 ||
         cpu.regs().x != 0x88 || cpu.regs().y != 0x99 || cpu.regs().p != 0x27)) {
      std::cerr << "FAIL: router precedence mismatch for NOP variant handler\n";
      failures++;
      testFailed = true;
    }
  }

  {
    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    const uint16_t start = 0x1E60;
    writeProgram(*mem, start,
                 {
                     0xE8,
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();
    cpu.regs().x = 0x7F;
    cpu.regs().p = 0x20;

    uint32_t cycles = cpu.step();
    if (!testFailed &&
        (cycles != 2 || cpu.regs().pc != static_cast<uint16_t>(start + 1) || cpu.regs().x != 0x80 ||
         (cpu.regs().p & 0x80) == 0 || (cpu.regs().p & 0x02) != 0)) {
      std::cerr << "FAIL: router precedence mismatch for misc-tail fallback route\n";
      failures++;
      testFailed = true;
    }
  }

  {
    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    const uint16_t start = 0x1E80;
    writeProgram(*mem, start,
                 {
                     0xA9,
                     0xF0,
                     0x09,
                     0x0F,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();

    (void)cpu.step();
    uint32_t cycles = cpu.step();
    if (!testFailed &&
        (cycles != 2 || cpu.regs().pc != static_cast<uint16_t>(start + 4) || cpu.regs().a != 0xFF ||
         (cpu.regs().p & 0x80) == 0 || (cpu.regs().p & 0x02) != 0)) {
      std::cerr << "FAIL: router precedence mismatch for ALU fallback route\n";
      failures++;
      testFailed = true;
    }
  }

  if (!testFailed) {
    std::cout << "PASS: fallback_router_precedence_contracts\n";
  }
}

int main() {
  int failures = 0;

  fs::path tempDir = fs::temp_directory_path() / "prodos8emu_cpu65c02_test";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir);

  // Test 1: JSR $BF00 triggers MLI dispatch using inline call encoding
  //         (byte callNumber, word paramBlockAddr) immediately after the JSR.
  {
    std::cout << "Test 1: JSR $BF00 MLI trap\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    // This repo's Apple2Memory model currently has a zero-filled ROM area.
    // For this CPU test, place vectors into the readable RAM mapping.
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    // Parameter block at $0300 for ALLOC_INTERRUPT ($40)
    // +0 param_count = 2
    // +1 int_num (result)
    // +2 int_code_ptr = $2000
    const uint16_t param = 0x0300;
    prodos8emu::write_u8(mem.banks(), param + 0, 2);
    prodos8emu::write_u8(mem.banks(), param + 1, 0);
    prodos8emu::write_u16_le(mem.banks(), param + 2, 0x2000);

    // Program at $0200 (ProDOS MLI calling convention):
    //   JSR $BF00
    //   .byte $40
    //   .word $0300
    //   NOP
    const uint16_t start = 0x0200;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0x40,
                     static_cast<uint8_t>(param & 0xFF),
                     static_cast<uint8_t>((param >> 8) & 0xFF),
                     0xEA,
                 });

    // Set reset vector to $0200
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    std::stringstream coutLog;
    cpu.setDebugLogs(&mliLog, &coutLog);
    cpu.reset();

    // ProDOS MLI should return with decimal mode clear.
    cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x08);

    // Execute a few instructions (JSR trap + NOP)
    cpu.step();
    cpu.step();

    uint8_t     slot    = prodos8emu::read_u8(mem.constBanks(), param + 1);
    uint8_t     a       = cpu.regs().a;
    bool        c       = (cpu.regs().p & 0x01) != 0;
    bool        z       = (cpu.regs().p & 0x02) != 0;
    bool        d       = (cpu.regs().p & 0x08) != 0;
    std::string mliText = mliLog.str();

    if (slot != 1) {
      std::cerr << "FAIL: Expected ALLOC_INTERRUPT to write slot=1, got " << (int)slot << "\n";
      failures++;
    } else if (a != 0) {
      std::cerr << "FAIL: Expected A=0 (ERR_NO_ERROR) after MLI, got 0x" << std::hex << (int)a
                << std::dec << "\n";
      failures++;
    } else if (c) {
      std::cerr << "FAIL: Expected Carry clear on success\n";
      failures++;
    } else if (!z) {
      std::cerr << "FAIL: Expected Z set on success (A=0)\n";
      failures++;
    } else if (d) {
      std::cerr << "FAIL: Expected D clear on ProDOS MLI return\n";
      failures++;
    } else if (mliText.find("ALLOC_INTERRUPT") == std::string::npos) {
      std::cerr << "FAIL: Expected MLI log to include call name ALLOC_INTERRUPT\n";
      failures++;
    } else if (mliText.find("result=$00") == std::string::npos) {
      std::cerr << "FAIL: Expected MLI log to include success result=$00\n";
      failures++;
    } else if (cpu.regs().pc != static_cast<uint16_t>(start + 7)) {
      std::cerr << "FAIL: Expected PC to reach NOP+1 (JSR+3 then NOP), got 0x" << std::hex
                << cpu.regs().pc << std::dec << "\n";
      failures++;
    } else {
      std::cout << "PASS: JSR $BF00 MLI trap\n";
    }
  }

  // Test 1b: JSR $BF00 QUIT ($65) stops emulation after logging
  {
    std::cout << "Test 1b: JSR $BF00 QUIT stops CPU\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t param = 0x0310;
    prodos8emu::write_u8(mem.banks(), param + 0, 4);  // param_count
    prodos8emu::write_u8(mem.banks(), param + 1, 0);  // quit_type
    prodos8emu::write_u16_le(mem.banks(), param + 2, 0);
    prodos8emu::write_u8(mem.banks(), param + 4, 0);
    prodos8emu::write_u16_le(mem.banks(), param + 5, 0);

    const uint16_t start = 0x0220;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0x65,
                     static_cast<uint8_t>(param & 0xFF),
                     static_cast<uint8_t>((param >> 8) & 0xFF),
                     0xEA,
                 });

    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    std::stringstream coutLog;
    cpu.setDebugLogs(&mliLog, &coutLog);
    cpu.reset();

    cpu.step();
    uint16_t pcAfterQuit = cpu.regs().pc;
    cpu.step();  // Should be a no-op once stopped

    std::string mliText = mliLog.str();

    if (!cpu.isStopped()) {
      std::cerr << "FAIL: Expected CPU to stop after QUIT ($65)\n";
      failures++;
    } else if (pcAfterQuit != static_cast<uint16_t>(start + 6)) {
      std::cerr << "FAIL: Expected PC at return address after QUIT, got 0x" << std::hex
                << pcAfterQuit << std::dec << "\n";
      failures++;
    } else if (cpu.regs().pc != pcAfterQuit) {
      std::cerr << "FAIL: Expected stopped CPU to keep PC unchanged\n";
      failures++;
    } else if (mliText.find("QUIT") == std::string::npos) {
      std::cerr << "FAIL: Expected MLI log to include QUIT\n";
      failures++;
    } else if (mliText.find("result=$00") == std::string::npos) {
      std::cerr << "FAIL: Expected QUIT log to include success result=$00\n";
      failures++;
    } else {
      std::cout << "PASS: JSR $BF00 QUIT stops CPU\n";
    }
  }

  // Test 2: JMP ($0036) logs A register as COUT stream output.
  {
    std::cout << "Test 2: JMP ($0036) COUT logging\n";

    prodos8emu::Apple2Memory mem;

    // For this CPU test, place vectors into the readable RAM mapping.
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start = 0x0400;
    // Program:
    //   LDA #$C1
    //   JMP ($0036)
    //   NOP
    writeProgram(mem, start,
                 {
                     0xA9,
                     0xC1,
                     0x6C,
                     0x36,
                     0x00,
                     0xEA,
                 });

    // Point COUT vector to NOP so execution continues deterministically.
    prodos8emu::write_u16_le(mem.banks(), 0x0036, static_cast<uint16_t>(start + 5));

    // Set reset vector to start.
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    std::stringstream    coutLog;
    cpu.setDebugLogs(nullptr, &coutLog);
    cpu.reset();

    // Execute LDA, JMP ($0036), NOP.
    cpu.step();
    cpu.step();
    cpu.step();

    const std::string coutText = coutLog.str();

    // 0xC1 & 0x7F = 0x41 = 'A'
    if (coutText != "A") {
      std::cerr << "FAIL: Expected COUT log to be 'A', got: '" << coutText << "'\n";
      failures++;
    } else if (cpu.regs().pc != static_cast<uint16_t>(start + 6)) {
      std::cerr << "FAIL: Expected PC to reach NOP+1 after JMP vector, got 0x" << std::hex
                << cpu.regs().pc << std::dec << "\n";
      failures++;
    } else {
      std::cout << "PASS: JMP ($0036) COUT logging\n";
    }
  }

  // Test 3: COUT control character handling
  {
    std::cout << "Test 3: COUT control character handling\n";

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start = 0x0400;
    // Program that outputs various control characters:
    //   LDA #$0D   ; CR -> should output newline
    //   JMP ($0036)
    //   LDA #$89   ; 0x89 & 0x7F = 0x09 = TAB -> should output \t
    //   JMP ($0036)
    //   LDA #$87   ; 0x87 & 0x7F = 0x07 = BEL -> should output \a
    //   JMP ($0036)
    //   NOP
    writeProgram(mem, start,
                 {
                     0xA9, 0x0D,        // LDA #$0D
                     0x6C, 0x36, 0x00,  // JMP ($0036)
                     0xA9, 0x89,        // LDA #$89
                     0x6C, 0x36, 0x00,  // JMP ($0036)
                     0xA9, 0x87,        // LDA #$87
                     0x6C, 0x36, 0x00,  // JMP ($0036)
                     0xEA,              // NOP
                 });

    // Point COUT vector to continue after each JMP
    prodos8emu::write_u16_le(mem.banks(), 0x0036, static_cast<uint16_t>(start + 5));
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    std::stringstream    coutLog;
    cpu.setDebugLogs(nullptr, &coutLog);
    cpu.reset();

    // Execute first sequence: LDA #$0D, JMP ($0036)
    cpu.step();
    cpu.step();

    // Update vector to continue after second JMP
    prodos8emu::write_u16_le(mem.banks(), 0x0036, static_cast<uint16_t>(start + 10));

    // Execute second sequence: LDA #$89, JMP ($0036)
    cpu.step();
    cpu.step();

    // Update vector to continue after third JMP
    prodos8emu::write_u16_le(mem.banks(), 0x0036, static_cast<uint16_t>(start + 15));

    // Execute third sequence: LDA #$87, JMP ($0036)
    cpu.step();
    cpu.step();

    const std::string coutText = coutLog.str();

    // Expected: newline, then \t, then \a
    if (coutText != "\n\\t\\a") {
      std::cerr << "FAIL: Expected COUT log to be '\\n\\\\t\\\\a', got: '";
      for (char c : coutText) {
        if (c == '\n') {
          std::cerr << "\\n";
        } else if (c == '\\') {
          std::cerr << "\\\\";
        } else if (c >= 0x20 && c <= 0x7E) {
          std::cerr << c;
        } else {
          std::cerr << "\\x" << std::hex << static_cast<int>(static_cast<uint8_t>(c)) << std::dec;
        }
      }
      std::cerr << "'\n";
      failures++;
    } else {
      std::cout << "PASS: COUT control character handling\n";
    }
  }

  // Test 4: MLI pathname logging
  {
    std::cout << "Test 4: MLI pathname logging\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    // Create a test volume and file
    fs::path volume = tempDir / "VOL1";
    fs::create_directories(volume);
    fs::path testFile = volume / "TESTFILE";
    std::ofstream(testFile) << "test data";

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    // Pathname counted string at $0400: "/VOL1/TESTFILE"
    const uint16_t    pathnameAddr = 0x0400;
    const std::string pathname     = "/VOL1/TESTFILE";
    prodos8emu::write_u8(mem.banks(), pathnameAddr, static_cast<uint8_t>(pathname.length()));
    for (size_t i = 0; i < pathname.length(); i++) {
      prodos8emu::write_u8(mem.banks(), static_cast<uint16_t>(pathnameAddr + 1 + i),
                           static_cast<uint8_t>(pathname[i]));
    }

    // Parameter block at $0300 for GET_FILE_INFO ($C4)
    // +0 param_count = 10
    // +1 pathname_ptr = $0400
    const uint16_t param = 0x0300;
    prodos8emu::write_u8(mem.banks(), param + 0, 10);
    prodos8emu::write_u16_le(mem.banks(), param + 1, pathnameAddr);

    // Program at $0200: JSR $BF00, .byte $C4, .word $0300, NOP
    const uint16_t start = 0x0200;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0xC4,  // GET_FILE_INFO
                     static_cast<uint8_t>(param & 0xFF),
                     static_cast<uint8_t>((param >> 8) & 0xFF),
                     0xEA,
                 });

    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    // Execute JSR trap + NOP
    cpu.step();
    cpu.step();

    std::string mliText = mliLog.str();

    if (mliText.find("GET_FILE_INFO") == std::string::npos) {
      std::cerr << "FAIL: Expected MLI log to include GET_FILE_INFO\n";
      failures++;
    } else if (mliText.find("path='/VOL1/TESTFILE'") == std::string::npos) {
      std::cerr << "FAIL: Expected MLI log to include path='/VOL1/TESTFILE', got:\n"
                << mliText << "\n";
      failures++;
    } else {
      std::cout << "PASS: MLI pathname logging\n";
    }
  }

  // Test 5: MLI error name display
  {
    std::cout << "Test 5: MLI error name display\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    // Create a test volume (no file)
    fs::path volume = tempDir / "VOL2";
    fs::create_directories(volume);

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    // Pathname counted string at $0400: "/VOL2/NONEXISTENT"
    const uint16_t    pathnameAddr = 0x0400;
    const std::string pathname     = "/VOL2/NONEXISTENT";
    prodos8emu::write_u8(mem.banks(), pathnameAddr, static_cast<uint8_t>(pathname.length()));
    for (size_t i = 0; i < pathname.length(); i++) {
      prodos8emu::write_u8(mem.banks(), static_cast<uint16_t>(pathnameAddr + 1 + i),
                           static_cast<uint8_t>(pathname[i]));
    }

    // Parameter block at $0300 for OPEN ($C8)
    // +0 param_count = 3
    // +1 pathname_ptr = $0400
    // +3 io_buffer = $2000
    // +5 ref_num = 0 (output)
    const uint16_t param = 0x0300;
    prodos8emu::write_u8(mem.banks(), param + 0, 3);
    prodos8emu::write_u16_le(mem.banks(), param + 1, pathnameAddr);
    prodos8emu::write_u16_le(mem.banks(), param + 3, 0x2000);

    // Program at $0200: JSR $BF00, .byte $C8, .word $0300, NOP
    const uint16_t start = 0x0200;
    writeProgram(mem, start,
                 {
                     0x20, 0x00, 0xBF,
                     0xC8,  // OPEN
                     static_cast<uint8_t>(param & 0xFF), static_cast<uint8_t>((param >> 8) & 0xFF),
                     0xEA,  // NOP
                 });

    // Set reset vector to $0200
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);

    std::ostringstream mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    // Execute JSR trap + NOP
    cpu.step();
    cpu.step();

    std::string mliText = mliLog.str();

    if (mliText.find("OPEN") == std::string::npos) {
      std::cerr << "FAIL: Expected MLI log to include OPEN\n";
      failures++;
    } else if (mliText.find("ERROR (FILE_NOT_FOUND)") == std::string::npos) {
      std::cerr << "FAIL: Expected MLI log to include ERROR (FILE_NOT_FOUND), got:\n"
                << mliText << "\n";
      failures++;
    } else {
      std::cout << "PASS: MLI error name display\n";
    }
  }

  // Test 6: mli_detached_jsr_abs_behaves_as_normal_jsr
  {
    std::cout << "Test 6: mli_detached_jsr_abs_behaves_as_normal_jsr\n";

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start = 0x0200;

    // Program at $0200:
    //   JSR $BF00
    //   NOP
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0xEA,
                 });

    // Subroutine target at $BF00: RTS
    writeProgram(mem, 0xBF00,
                 {
                     0x60,
                 });

    // Set reset vector to $0200
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    std::stringstream    mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    cpu.step();

    uint8_t  spAfterJsr = cpu.regs().sp;
    uint16_t pcAfterJsr = cpu.regs().pc;
    uint8_t  retLo      = prodos8emu::read_u8(mem.constBanks(), 0x01FE);
    uint8_t  retHi      = prodos8emu::read_u8(mem.constBanks(), 0x01FF);

    cpu.step();
    uint16_t pcAfterRts = cpu.regs().pc;

    if (pcAfterJsr != 0xBF00) {
      std::cerr << "FAIL: Expected detached JSR $BF00 to jump to $BF00, got 0x" << std::hex
                << pcAfterJsr << std::dec << "\n";
      failures++;
    } else if (spAfterJsr != 0xFD) {
      std::cerr << "FAIL: Expected SP=0xFD after normal JSR push, got 0x" << std::hex
                << (int)spAfterJsr << std::dec << "\n";
      failures++;
    } else if (retLo != 0x02 || retHi != 0x02) {
      std::cerr << "FAIL: Expected return address bytes on stack to be $02 $02, got $" << std::hex
                << (int)retLo << " $" << (int)retHi << std::dec << "\n";
      failures++;
    } else if (pcAfterRts != static_cast<uint16_t>(start + 3)) {
      std::cerr << "FAIL: Expected RTS to return to JSR+3, got 0x" << std::hex << pcAfterRts
                << std::dec << "\n";
      failures++;
    } else if (!mliLog.str().empty()) {
      std::cerr << "FAIL: Expected no MLI log when MLI is detached\n";
      failures++;
    } else {
      std::cout << "PASS: mli_detached_jsr_abs_behaves_as_normal_jsr\n";
    }
  }

  // Test 7: mli_quit_and_non_quit_stop_contract
  {
    std::cout << "Test 7: mli_quit_and_non_quit_stop_contract\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);

      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      // ALLOC_INTERRUPT ($40), success path should not stop CPU
      const uint16_t param = 0x0320;
      prodos8emu::write_u8(mem.banks(), param + 0, 2);
      prodos8emu::write_u8(mem.banks(), param + 1, 0);
      prodos8emu::write_u16_le(mem.banks(), param + 2, 0x2100);

      const uint16_t start = 0x0240;
      writeProgram(mem, start,
                   {
                       0x20,
                       0x00,
                       0xBF,
                       0x40,
                       static_cast<uint8_t>(param & 0xFF),
                       static_cast<uint8_t>((param >> 8) & 0xFF),
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.attachMLI(ctx);
      cpu.reset();

      cpu.step();
      if (cpu.isStopped()) {
        std::cerr << "FAIL: Expected non-QUIT MLI call to keep CPU running\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);

      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      // QUIT ($65), success path should stop CPU
      const uint16_t param = 0x0330;
      prodos8emu::write_u8(mem.banks(), param + 0, 4);
      prodos8emu::write_u8(mem.banks(), param + 1, 0);
      prodos8emu::write_u16_le(mem.banks(), param + 2, 0);
      prodos8emu::write_u8(mem.banks(), param + 4, 0);
      prodos8emu::write_u16_le(mem.banks(), param + 5, 0);

      const uint16_t start = 0x0250;
      writeProgram(mem, start,
                   {
                       0x20,
                       0x00,
                       0xBF,
                       0x65,
                       static_cast<uint8_t>(param & 0xFF),
                       static_cast<uint8_t>((param >> 8) & 0xFF),
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.attachMLI(ctx);
      cpu.reset();

      cpu.step();
      if (!cpu.isStopped()) {
        std::cerr << "FAIL: Expected QUIT MLI call to stop CPU\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: mli_quit_and_non_quit_stop_contract\n";
    }
  }

  // Test 8: mli_log_contains_expected_field_order_for_open
  {
    std::cout << "Test 8: mli_log_contains_expected_field_order_for_open\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    fs::path volume = tempDir / "VOL3";
    fs::create_directories(volume);
    fs::path filePath = volume / "OPENME";
    std::ofstream(filePath) << "open data";

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t    pathnameAddr = 0x0450;
    const std::string pathname     = "/VOL3/OPENME";
    prodos8emu::write_u8(mem.banks(), pathnameAddr, static_cast<uint8_t>(pathname.length()));
    for (size_t i = 0; i < pathname.length(); i++) {
      prodos8emu::write_u8(mem.banks(), static_cast<uint16_t>(pathnameAddr + 1 + i),
                           static_cast<uint8_t>(pathname[i]));
    }

    // OPEN ($C8): param_count=3, pathname ptr, io_buffer ptr, ref_num out
    const uint16_t param = 0x0340;
    prodos8emu::write_u8(mem.banks(), param + 0, 3);
    prodos8emu::write_u16_le(mem.banks(), param + 1, pathnameAddr);
    prodos8emu::write_u16_le(mem.banks(), param + 3, 0x2200);
    prodos8emu::write_u8(mem.banks(), param + 5, 0);

    const uint16_t start = 0x0260;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0xC8,
                     static_cast<uint8_t>(param & 0xFF),
                     static_cast<uint8_t>((param >> 8) & 0xFF),
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    cpu.step();

    const std::string logText = mliLog.str();

    size_t callPos   = logText.find(" MLI call=$C8 (OPEN)");
    size_t paramPos  = logText.find(" param=$0340");
    size_t pathPos   = logText.find(" path='/VOL3/OPENME'");
    size_t refPos    = logText.find(" ref=");
    size_t resultPos = logText.find(" result=$00");
    size_t okPos     = logText.find(" OK");

    if (callPos == std::string::npos || paramPos == std::string::npos ||
        pathPos == std::string::npos || refPos == std::string::npos ||
        resultPos == std::string::npos || okPos == std::string::npos) {
      std::cerr << "FAIL: Expected OPEN MLI log fields were missing, got:\n" << logText << "\n";
      failures++;
    } else if (!(callPos < paramPos && paramPos < pathPos && pathPos < refPos &&
                 refPos < resultPos && resultPos < okPos)) {
      std::cerr
          << "FAIL: Expected OPEN MLI log field order call->param->path->ref->result->OK, got:\n"
          << logText << "\n";
      failures++;
    } else {
      std::cout << "PASS: mli_log_contains_expected_field_order_for_open\n";
    }
  }

  // Test 9: mli_flags_contract_after_trap_return
  {
    std::cout << "Test 9: mli_flags_contract_after_trap_return\n";

    bool testFailed = false;

    {
      // Success case: C clear, Z set, N clear, D clear, A=0
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);

      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t param = 0x0350;
      prodos8emu::write_u8(mem.banks(), param + 0, 2);
      prodos8emu::write_u8(mem.banks(), param + 1, 0);
      prodos8emu::write_u16_le(mem.banks(), param + 2, 0x2300);

      const uint16_t start = 0x0270;
      writeProgram(mem, start,
                   {
                       0x20,
                       0x00,
                       0xBF,
                       0x40,
                       static_cast<uint8_t>(param & 0xFF),
                       static_cast<uint8_t>((param >> 8) & 0xFF),
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.attachMLI(ctx);
      cpu.reset();
      cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x08);

      cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool d = (cpu.regs().p & 0x08) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;

      if (cpu.regs().a != 0 || c || !z || d || n) {
        std::cerr << "FAIL: Expected success flags A=0,C=0,Z=1,D=0,N=0 after MLI trap\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // Error case: C set, Z clear, N follows A bit 7, D clear, A=error
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);

      fs::path volume = tempDir / "VOL4";
      fs::create_directories(volume);

      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t    pathnameAddr = 0x0460;
      const std::string pathname     = "/VOL4/MISSING";
      prodos8emu::write_u8(mem.banks(), pathnameAddr, static_cast<uint8_t>(pathname.length()));
      for (size_t i = 0; i < pathname.length(); i++) {
        prodos8emu::write_u8(mem.banks(), static_cast<uint16_t>(pathnameAddr + 1 + i),
                             static_cast<uint8_t>(pathname[i]));
      }

      const uint16_t param = 0x0360;
      prodos8emu::write_u8(mem.banks(), param + 0, 3);
      prodos8emu::write_u16_le(mem.banks(), param + 1, pathnameAddr);
      prodos8emu::write_u16_le(mem.banks(), param + 3, 0x2400);

      const uint16_t start = 0x0280;
      writeProgram(mem, start,
                   {
                       0x20,
                       0x00,
                       0xBF,
                       0xC8,
                       static_cast<uint8_t>(param & 0xFF),
                       static_cast<uint8_t>((param >> 8) & 0xFF),
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.attachMLI(ctx);
      cpu.reset();
      cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x08);

      cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool d = (cpu.regs().p & 0x08) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;

      if (cpu.regs().a != 0x46 || !c || z || d || n) {
        std::cerr
            << "FAIL: Expected error flags A=ERR_FILE_NOT_FOUND,C=1,Z=0,D=0,N=0 after MLI trap\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: mli_flags_contract_after_trap_return\n";
    }
  }

  // Test 10: extract_pathname_len_zero_formats_empty
  {
    std::cout << "Test 10: extract_pathname_len_zero_formats_empty\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t pathnameAddr = 0x0470;
    prodos8emu::write_u8(mem.banks(), pathnameAddr, 0);  // length=0

    const uint16_t param = 0x0370;
    prodos8emu::write_u8(mem.banks(), param + 0, 0x0A);  // GET_FILE_INFO param_count
    prodos8emu::write_u16_le(mem.banks(), param + 1, pathnameAddr);

    const uint16_t start = 0x0290;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0xC4,
                     static_cast<uint8_t>(param & 0xFF),
                     static_cast<uint8_t>((param >> 8) & 0xFF),
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    cpu.step();

    const std::string logText = mliLog.str();

    if (logText.find("GET_FILE_INFO") == std::string::npos) {
      std::cerr << "FAIL: Expected GET_FILE_INFO in MLI log\n";
      failures++;
    } else if (logText.find(" path=<empty>") == std::string::npos) {
      std::cerr << "FAIL: Expected path=<empty> formatting, got:\n" << logText << "\n";
      failures++;
    } else {
      std::cout << "PASS: extract_pathname_len_zero_formats_empty\n";
    }
  }

  // Test 11: extract_pathname_invalid_length_formats_error
  {
    std::cout << "Test 11: extract_pathname_invalid_length_formats_error\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t pathnameAddr = 0x0480;
    prodos8emu::write_u8(mem.banks(), pathnameAddr, 65);  // invalid length > 64

    const uint16_t param = 0x0380;
    prodos8emu::write_u8(mem.banks(), param + 0, 0x0A);  // GET_FILE_INFO param_count
    prodos8emu::write_u16_le(mem.banks(), param + 1, pathnameAddr);

    const uint16_t start = 0x02A0;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0xC4,
                     static_cast<uint8_t>(param & 0xFF),
                     static_cast<uint8_t>((param >> 8) & 0xFF),
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    cpu.step();

    const std::string logText = mliLog.str();

    if (logText.find("GET_FILE_INFO") == std::string::npos) {
      std::cerr << "FAIL: Expected GET_FILE_INFO in MLI log\n";
      failures++;
    } else if (logText.find(" path=<invalid:len=65>") == std::string::npos) {
      std::cerr << "FAIL: Expected path=<invalid:len=65> formatting, got:\n" << logText << "\n";
      failures++;
    } else {
      std::cout << "PASS: extract_pathname_invalid_length_formats_error\n";
    }
  }

  // Test 12: mli_get_prefix_logging_remains_stable
  {
    std::cout << "Test 12: mli_get_prefix_logging_remains_stable\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t    setPrefixPathname = 0x0490;
    const std::string prefixPath        = "/VOLP2/TOOLS";
    prodos8emu::write_u8(mem.banks(), setPrefixPathname, static_cast<uint8_t>(prefixPath.length()));
    for (size_t i = 0; i < prefixPath.length(); i++) {
      prodos8emu::write_u8(mem.banks(), static_cast<uint16_t>(setPrefixPathname + 1 + i),
                           static_cast<uint8_t>(prefixPath[i]));
    }

    const uint16_t setPrefixParam = 0x0390;
    prodos8emu::write_u8(mem.banks(), setPrefixParam + 0, 1);
    prodos8emu::write_u16_le(mem.banks(), setPrefixParam + 1, setPrefixPathname);
    uint8_t setPrefixErr = ctx.setPrefixCall(mem.constBanks(), setPrefixParam);

    const uint16_t getPrefixBuffer = 0x05A0;
    const uint16_t getPrefixParam  = 0x03A0;
    prodos8emu::write_u8(mem.banks(), getPrefixParam + 0, 1);
    prodos8emu::write_u16_le(mem.banks(), getPrefixParam + 1, getPrefixBuffer);

    const uint16_t start = 0x02B0;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0xC7,
                     static_cast<uint8_t>(getPrefixParam & 0xFF),
                     static_cast<uint8_t>((getPrefixParam >> 8) & 0xFF),
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    cpu.step();

    const std::string logText   = mliLog.str();
    size_t            callPos   = logText.find(" MLI call=$C7 (GET_PREFIX)");
    size_t            paramPos  = logText.find(" param=$03A0");
    size_t            prefixPos = logText.find(" prefix='/VOLP2/TOOLS'");
    size_t            resultPos = logText.find(" result=$00");
    size_t            okPos     = logText.find(" OK");

    if (setPrefixErr != 0) {
      std::cerr << "FAIL: Expected SET_PREFIX precondition to succeed, got 0x" << std::hex
                << static_cast<int>(setPrefixErr) << std::dec << "\n";
      failures++;
    } else if (callPos == std::string::npos || paramPos == std::string::npos ||
               prefixPos == std::string::npos || resultPos == std::string::npos ||
               okPos == std::string::npos) {
      std::cerr << "FAIL: Expected GET_PREFIX log fields were missing, got:\n" << logText << "\n";
      failures++;
    } else if (!(callPos < paramPos && paramPos < prefixPos && prefixPos < resultPos &&
                 resultPos < okPos)) {
      std::cerr
          << "FAIL: Expected GET_PREFIX log field order call->param->prefix->result->OK, got:\n"
          << logText << "\n";
      failures++;
    } else {
      std::cout << "PASS: mli_get_prefix_logging_remains_stable\n";
    }
  }

  // Test 13: mli_trap_sets_a_c_z_and_clears_d_as_expected
  {
    std::cout << "Test 13: mli_trap_sets_a_c_z_and_clears_d_as_expected\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t param = 0x03B0;
    prodos8emu::write_u8(mem.banks(), param + 0, 2);
    prodos8emu::write_u8(mem.banks(), param + 1, 0);
    prodos8emu::write_u16_le(mem.banks(), param + 2, 0x2500);

    const uint16_t start = 0x02C0;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0x40,
                     static_cast<uint8_t>(param & 0xFF),
                     static_cast<uint8_t>((param >> 8) & 0xFF),
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    cpu.reset();

    cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x08);
    cpu.step();

    bool c = (cpu.regs().p & 0x01) != 0;
    bool z = (cpu.regs().p & 0x02) != 0;
    bool d = (cpu.regs().p & 0x08) != 0;
    bool n = (cpu.regs().p & 0x80) != 0;

    if (cpu.regs().a != 0) {
      std::cerr << "FAIL: Expected A=0 after successful MLI trap\n";
      failures++;
    } else if (c) {
      std::cerr << "FAIL: Expected Carry clear after successful MLI trap\n";
      failures++;
    } else if (!z) {
      std::cerr << "FAIL: Expected Z set after successful MLI trap\n";
      failures++;
    } else if (d) {
      std::cerr << "FAIL: Expected D clear on MLI trap return\n";
      failures++;
    } else if (n) {
      std::cerr << "FAIL: Expected N clear when A=0 on MLI trap return\n";
      failures++;
    } else {
      std::cout << "PASS: mli_trap_sets_a_c_z_and_clears_d_as_expected\n";
    }
  }

  // Test 14: mli_error_sets_carry_success_clears_carry
  {
    std::cout << "Test 14: mli_error_sets_carry_success_clears_carry\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);

      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t param = 0x03C0;
      prodos8emu::write_u8(mem.banks(), param + 0, 2);
      prodos8emu::write_u8(mem.banks(), param + 1, 0);
      prodos8emu::write_u16_le(mem.banks(), param + 2, 0x2600);

      const uint16_t start = 0x02D0;
      writeProgram(mem, start,
                   {
                       0x20,
                       0x00,
                       0xBF,
                       0x40,
                       static_cast<uint8_t>(param & 0xFF),
                       static_cast<uint8_t>((param >> 8) & 0xFF),
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.attachMLI(ctx);
      cpu.reset();
      cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      if (c) {
        std::cerr << "FAIL: Expected Carry clear for successful MLI call\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);

      fs::path volume = tempDir / "VOL5";
      fs::create_directories(volume);

      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t    pathnameAddr = 0x04A0;
      const std::string pathname     = "/VOL5/MISSING";
      prodos8emu::write_u8(mem.banks(), pathnameAddr, static_cast<uint8_t>(pathname.length()));
      for (size_t i = 0; i < pathname.length(); i++) {
        prodos8emu::write_u8(mem.banks(), static_cast<uint16_t>(pathnameAddr + 1 + i),
                             static_cast<uint8_t>(pathname[i]));
      }

      const uint16_t param = 0x03D0;
      prodos8emu::write_u8(mem.banks(), param + 0, 3);
      prodos8emu::write_u16_le(mem.banks(), param + 1, pathnameAddr);
      prodos8emu::write_u16_le(mem.banks(), param + 3, 0x2700);

      const uint16_t start = 0x02E0;
      writeProgram(mem, start,
                   {
                       0x20,
                       0x00,
                       0xBF,
                       0xC8,
                       static_cast<uint8_t>(param & 0xFF),
                       static_cast<uint8_t>((param >> 8) & 0xFF),
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.attachMLI(ctx);
      cpu.reset();
      cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      if (cpu.regs().a == 0) {
        std::cerr << "FAIL: Expected non-zero A for failing MLI call\n";
        failures++;
        testFailed = true;
      } else if (!c) {
        std::cerr << "FAIL: Expected Carry set for failing MLI call\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: mli_error_sets_carry_success_clears_carry\n";
    }
  }

  // Test 15: quit_stops_cpu_non_quit_does_not_stop
  {
    std::cout << "Test 15: quit_stops_cpu_non_quit_does_not_stop\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);

      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t param = 0x03E0;
      prodos8emu::write_u8(mem.banks(), param + 0, 2);
      prodos8emu::write_u8(mem.banks(), param + 1, 0);
      prodos8emu::write_u16_le(mem.banks(), param + 2, 0x2800);

      const uint16_t start = 0x02F0;
      writeProgram(mem, start,
                   {
                       0x20,
                       0x00,
                       0xBF,
                       0x40,
                       static_cast<uint8_t>(param & 0xFF),
                       static_cast<uint8_t>((param >> 8) & 0xFF),
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.attachMLI(ctx);
      cpu.reset();
      cpu.step();

      if (cpu.isStopped()) {
        std::cerr << "FAIL: Expected non-QUIT MLI call to keep CPU running\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);

      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t param = 0x03F0;
      prodos8emu::write_u8(mem.banks(), param + 0, 4);
      prodos8emu::write_u8(mem.banks(), param + 1, 0);
      prodos8emu::write_u16_le(mem.banks(), param + 2, 0);
      prodos8emu::write_u8(mem.banks(), param + 4, 0);
      prodos8emu::write_u16_le(mem.banks(), param + 5, 0);

      const uint16_t start = 0x0300;
      writeProgram(mem, start,
                   {
                       0x20,
                       0x00,
                       0xBF,
                       0x65,
                       static_cast<uint8_t>(param & 0xFF),
                       static_cast<uint8_t>((param >> 8) & 0xFF),
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.attachMLI(ctx);
      cpu.reset();
      cpu.step();

      uint16_t pcAfterQuit = cpu.regs().pc;
      cpu.step();

      if (!cpu.isStopped()) {
        std::cerr << "FAIL: Expected QUIT MLI call to stop CPU\n";
        failures++;
        testFailed = true;
      } else if (cpu.regs().pc != pcAfterQuit) {
        std::cerr << "FAIL: Expected stopped CPU to keep PC unchanged\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: quit_stops_cpu_non_quit_does_not_stop\n";
    }
  }

  // Test 16: branch_page_cross_behavior_preserved
  {
    std::cout << "Test 16: branch_page_cross_behavior_preserved\n";

    bool testFailed = false;

    {
      // BNE not taken: PC advances to next instruction, cycles remain at opcode baseline.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x04F0;
      writeProgram(mem, start,
                   {
                       0xD0,
                       0x05,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x02);  // Z=1 => BNE not taken

      uint32_t cycles = cpu.step();
      if (cycles != 2) {
        std::cerr << "FAIL: Expected BNE not-taken cycles=2, got " << cycles << "\n";
        failures++;
        testFailed = true;
      } else if (cpu.regs().pc != static_cast<uint16_t>(start + 2)) {
        std::cerr << "FAIL: Expected BNE not-taken PC=start+2, got 0x" << std::hex << cpu.regs().pc
                  << std::dec << "\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // BNE taken with page cross: branch target crosses from $04xx to $05xx.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x04FD;
      writeProgram(mem, start,
                   {
                       0xD0,
                       0x01,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().p = static_cast<uint8_t>(cpu.regs().p & ~0x02);  // Z=0 => BNE taken

      uint32_t cycles = cpu.step();
      if (cycles != 2) {
        std::cerr << "FAIL: Expected BNE taken page-cross cycles=2, got " << cycles << "\n";
        failures++;
        testFailed = true;
      } else if (cpu.regs().pc != 0x0500) {
        std::cerr << "FAIL: Expected BNE taken page-cross target PC=$0500, got 0x" << std::hex
                  << cpu.regs().pc << std::dec << "\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // BRA unconditional branch with page cross keeps BRA cycle contract.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x04FD;
      writeProgram(mem, start,
                   {
                       0x80,
                       0x01,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t cycles = cpu.step();
      if (cycles != 3) {
        std::cerr << "FAIL: Expected BRA page-cross cycles=3, got " << cycles << "\n";
        failures++;
        testFailed = true;
      } else if (cpu.regs().pc != 0x0500) {
        std::cerr << "FAIL: Expected BRA page-cross target PC=$0500, got 0x" << std::hex
                  << cpu.regs().pc << std::dec << "\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: branch_page_cross_behavior_preserved\n";
    }
  }

  // Test 17: brk_rti_stack_and_flags_behavior_preserved
  {
    std::cout << "Test 17: brk_rti_stack_and_flags_behavior_preserved\n";

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start      = 0x0600;
    const uint16_t irqHandler = 0x0700;

    writeProgram(mem, start,
                 {
                     0x00,
                     0xEA,
                 });
    writeProgram(mem, irqHandler,
                 {
                     0x40,
                 });

    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);
    prodos8emu::write_u16_le(mem.banks(), 0xFFFE, irqHandler);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.reset();

    cpu.regs().p = 0x29;  // C=1,D=1,U=1

    uint32_t brkCycles = cpu.step();

    uint8_t stackedStatus = prodos8emu::read_u8(mem.constBanks(), 0x01FD);
    uint8_t stackedPCLo   = prodos8emu::read_u8(mem.constBanks(), 0x01FE);
    uint8_t stackedPCHi   = prodos8emu::read_u8(mem.constBanks(), 0x01FF);

    bool brkI = (cpu.regs().p & 0x04) != 0;
    bool brkD = (cpu.regs().p & 0x08) != 0;

    uint32_t rtiCycles = cpu.step();

    uint8_t expectedStackedStatus = static_cast<uint8_t>(0x29 | 0x10 | 0x20);
    uint8_t expectedRestoredP     = static_cast<uint8_t>(expectedStackedStatus | 0x20);

    if (brkCycles != 7) {
      std::cerr << "FAIL: Expected BRK cycles=7, got " << brkCycles << "\n";
      failures++;
    } else if (cpu.regs().sp != 0xFF) {
      std::cerr << "FAIL: Expected SP restored to $FF after RTI, got 0x" << std::hex
                << static_cast<int>(cpu.regs().sp) << std::dec << "\n";
      failures++;
    } else if (stackedPCLo != 0x02 || stackedPCHi != 0x06) {
      std::cerr << "FAIL: Expected BRK stack return PC bytes $02 $06, got $" << std::hex
                << static_cast<int>(stackedPCLo) << " $" << static_cast<int>(stackedPCHi)
                << std::dec << "\n";
      failures++;
    } else if (stackedStatus != expectedStackedStatus) {
      std::cerr << "FAIL: Expected BRK stacked status $" << std::hex
                << static_cast<int>(expectedStackedStatus) << ", got $"
                << static_cast<int>(stackedStatus) << std::dec << "\n";
      failures++;
    } else if (!brkI) {
      std::cerr << "FAIL: Expected BRK to set I flag\n";
      failures++;
    } else if (brkD) {
      std::cerr << "FAIL: Expected BRK to clear D flag\n";
      failures++;
    } else if (rtiCycles != 6) {
      std::cerr << "FAIL: Expected RTI cycles=6, got " << rtiCycles << "\n";
      failures++;
    } else if (cpu.regs().pc != static_cast<uint16_t>(start + 2)) {
      std::cerr << "FAIL: Expected RTI to restore PC to BRK+2, got 0x" << std::hex << cpu.regs().pc
                << std::dec << "\n";
      failures++;
    } else if (cpu.regs().p != expectedRestoredP) {
      std::cerr << "FAIL: Expected RTI to restore P=$" << std::hex
                << static_cast<int>(expectedRestoredP) << ", got $"
                << static_cast<int>(cpu.regs().p) << std::dec << "\n";
      failures++;
    } else {
      std::cout << "PASS: brk_rti_stack_and_flags_behavior_preserved\n";
    }
  }

  // Test 18: wai_stp_control_flow_preserved
  {
    std::cout << "Test 18: wai_stp_control_flow_preserved\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0800;
      writeProgram(mem, start,
                   {
                       0xCB,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t waiCycles  = cpu.step();
      uint16_t pcAfterWai = cpu.regs().pc;
      uint32_t waitCycles = cpu.step();

      if (waiCycles != 3) {
        std::cerr << "FAIL: Expected WAI cycles=3, got " << waiCycles << "\n";
        failures++;
        testFailed = true;
      } else if (!cpu.isWaiting()) {
        std::cerr << "FAIL: Expected CPU waiting state after WAI\n";
        failures++;
        testFailed = true;
      } else if (cpu.isStopped()) {
        std::cerr << "FAIL: Expected CPU not stopped after WAI\n";
        failures++;
        testFailed = true;
      } else if (waitCycles != 0) {
        std::cerr << "FAIL: Expected step() to return 0 while waiting, got " << waitCycles << "\n";
        failures++;
        testFailed = true;
      } else if (cpu.regs().pc != pcAfterWai) {
        std::cerr << "FAIL: Expected PC unchanged while waiting\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0810;
      writeProgram(mem, start,
                   {
                       0xDB,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t stpCycles     = cpu.step();
      uint16_t pcAfterStp    = cpu.regs().pc;
      uint32_t stoppedCycles = cpu.step();

      if (stpCycles != 3) {
        std::cerr << "FAIL: Expected STP cycles=3, got " << stpCycles << "\n";
        failures++;
        testFailed = true;
      } else if (!cpu.isStopped()) {
        std::cerr << "FAIL: Expected CPU stopped state after STP\n";
        failures++;
        testFailed = true;
      } else if (stoppedCycles != 0) {
        std::cerr << "FAIL: Expected step() to return 0 while stopped, got " << stoppedCycles
                  << "\n";
        failures++;
        testFailed = true;
      } else if (cpu.regs().pc != pcAfterStp) {
        std::cerr << "FAIL: Expected PC unchanged while stopped\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: wai_stp_control_flow_preserved\n";
    }
  }

  // Test 19: trace_markers_emit_for_known_entry_points
  {
    std::cout << "Test 19: trace_markers_emit_for_known_entry_points\n";

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    prodos8emu::write_u8(mem.banks(), 0x7800, 0xEA);
    prodos8emu::write_u8(mem.banks(), 0x7816, 0xEA);
    prodos8emu::write_u8(mem.banks(), 0x7C98, 0xEA);
    prodos8emu::write_u8(mem.banks(), 0x0067, 0x12);
    prodos8emu::write_u8(mem.banks(), 0x00BF, 0x34);
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, 0x7800);

    prodos8emu::CPU65C02 cpu(mem);
    std::stringstream    traceLog;
    cpu.setTraceLog(&traceLog);
    cpu.reset();

    cpu.step();
    cpu.regs().pc = 0x7816;
    cpu.step();
    cpu.regs().pc = 0x7C98;
    cpu.step();

    std::string traceText = traceLog.str();

    if (traceText.find("PC=$7800 >>> ENTER EdAsm.Asm") == std::string::npos) {
      std::cerr << "FAIL: Expected trace marker for PC=$7800 entry point\n";
      failures++;
    } else if (traceText.find("PC=$7816 >>> ENTER ExecAsm PassNbr(ZP$67)=$12 GenF(ZP$BF)=$34") ==
               std::string::npos) {
      std::cerr << "FAIL: Expected trace marker with PassNbr/GenF for PC=$7816\n";
      failures++;
    } else if (traceText.find("PC=$7C98 >>> PrtSetup") == std::string::npos) {
      std::cerr << "FAIL: Expected trace marker for PC=$7C98\n";
      failures++;
    } else {
      std::cout << "PASS: trace_markers_emit_for_known_entry_points\n";
    }
  }

  // Test 20: passnbr_genf_listing_deltas_logged_consistently
  {
    std::cout << "Test 20: passnbr_genf_listing_deltas_logged_consistently\n";

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start = 0x0900;
    writeProgram(mem, start,
                 {
                     0xA9,
                     0x05,
                     0x85,
                     0x67,
                     0xE6,
                     0x67,
                     0xA9,
                     0xAA,
                     0x85,
                     0xBF,
                     0xA9,
                     0x01,
                     0x85,
                     0x68,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    std::stringstream    traceLog;
    cpu.setTraceLog(&traceLog);
    cpu.reset();

    for (int i = 0; i < 7; ++i) {
      cpu.step();
    }

    std::string traceText = traceLog.str();

    if (traceText.find("STA PassNbr($67): $00 -> $05") == std::string::npos) {
      std::cerr << "FAIL: Expected STA PassNbr delta trace line\n";
      failures++;
    } else if (traceText.find("INC PassNbr($67): $05 -> $06") == std::string::npos) {
      std::cerr << "FAIL: Expected INC PassNbr delta trace line\n";
      failures++;
    } else if (traceText.find("GenF($BF): $00 -> $AA") == std::string::npos) {
      std::cerr << "FAIL: Expected GenF delta trace line\n";
      failures++;
    } else if (traceText.find("ListingF($68): $00 -> $01") == std::string::npos) {
      std::cerr << "FAIL: Expected ListingF delta trace line\n";
      failures++;
    } else {
      std::cout << "PASS: passnbr_genf_listing_deltas_logged_consistently\n";
    }
  }

  // Test 21: trace_disabled_emits_no_trace_lines
  {
    std::cout << "Test 21: trace_disabled_emits_no_trace_lines\n";

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    prodos8emu::write_u8(mem.banks(), 0x7800, 0xEA);

    const uint16_t start = 0x0920;
    writeProgram(mem, start,
                 {
                     0xA9,
                     0x01,
                     0x85,
                     0x67,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, 0x7800);

    prodos8emu::CPU65C02 cpu(mem);
    std::stringstream    traceLog;
    cpu.setTraceLog(&traceLog);
    cpu.setTraceLog(nullptr);
    cpu.reset();

    cpu.step();
    cpu.regs().pc = start;
    cpu.step();
    cpu.step();

    std::string traceText = traceLog.str();

    if (!traceText.empty()) {
      std::cerr << "FAIL: Expected no trace output when trace logging is disabled\n";
      failures++;
    } else {
      std::cout << "PASS: trace_disabled_emits_no_trace_lines\n";
    }
  }

  // Test 22: adc_sbc_binary_decimal_flag_contracts
  {
    std::cout << "Test 22: adc_sbc_binary_decimal_flag_contracts\n";

    bool testFailed = false;

    {
      // ADC binary: 0x50 + 0x50 => 0xA0, C=0, V=1, N=1, Z=0
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0A00;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x50,
                       0x18,
                       0x69,
                       0x50,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      cpu.step();
      cpu.step();
      cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool v = (cpu.regs().p & 0x40) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;
      bool d = (cpu.regs().p & 0x08) != 0;

      if (cpu.regs().a != 0xA0 || c || z || !v || !n || d) {
        std::cerr << "FAIL: ADC binary contract expected A=$A0,C=0,Z=0,V=1,N=1,D=0; got A=$"
                  << std::hex << static_cast<int>(cpu.regs().a) << std::dec << "\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // SBC binary: 0x50 - 0x10 => 0x40, C=1, V=0, N=0, Z=0
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0A20;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x50,
                       0x38,
                       0xE9,
                       0x10,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      cpu.step();
      cpu.step();
      cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool v = (cpu.regs().p & 0x40) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;
      bool d = (cpu.regs().p & 0x08) != 0;

      if (cpu.regs().a != 0x40 || !c || z || v || n || d) {
        std::cerr << "FAIL: SBC binary contract expected A=$40,C=1,Z=0,V=0,N=0,D=0; got A=$"
                  << std::hex << static_cast<int>(cpu.regs().a) << std::dec << "\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // ADC decimal: 0x45 + 0x55 => 0x00 (BCD), C=1, Z=1, N=0, D=1
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0A40;
      writeProgram(mem, start,
                   {
                       0xF8,
                       0xA9,
                       0x45,
                       0x18,
                       0x69,
                       0x55,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      cpu.step();
      cpu.step();
      cpu.step();
      cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool v = (cpu.regs().p & 0x40) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;
      bool d = (cpu.regs().p & 0x08) != 0;

      if (cpu.regs().a != 0x00 || !c || !z || !v || n || !d) {
        std::cerr << "FAIL: ADC decimal contract expected A=$00,C=1,Z=1,V=1,N=0,D=1; got A=$"
                  << std::hex << static_cast<int>(cpu.regs().a) << std::dec << "\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // SBC decimal with borrow: 0x00 - 0x01 => 0x99 (BCD), C=0, Z=0, N=1, D=1
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0A60;
      writeProgram(mem, start,
                   {
                       0xF8,
                       0xA9,
                       0x00,
                       0x38,
                       0xE9,
                       0x01,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      cpu.step();
      cpu.step();
      cpu.step();
      cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool v = (cpu.regs().p & 0x40) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;
      bool d = (cpu.regs().p & 0x08) != 0;

      if (cpu.regs().a != 0x99 || c || z || v || !n || !d) {
        std::cerr << "FAIL: SBC decimal contract expected A=$99,C=0,Z=0,V=0,N=1,D=1; got A=$"
                  << std::hex << static_cast<int>(cpu.regs().a) << std::dec << "\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: adc_sbc_binary_decimal_flag_contracts\n";
    }
  }

  // Test 23: logic_ora_and_eor_nz_contracts
  {
    std::cout << "Test 23: logic_ora_and_eor_nz_contracts\n";

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start = 0x0AC0;
    writeProgram(mem, start,
                 {
                     0xA9,
                     0x40,
                     0x09,
                     0x80,
                     0x29,
                     0x0F,
                     0x49,
                     0x0F,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.reset();

    cpu.step();

    // Prime C and V; ORA/AND/EOR should update only N/Z.
    cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x41);

    cpu.step();
    bool oraC = (cpu.regs().p & 0x01) != 0;
    bool oraZ = (cpu.regs().p & 0x02) != 0;
    bool oraV = (cpu.regs().p & 0x40) != 0;
    bool oraN = (cpu.regs().p & 0x80) != 0;

    cpu.step();
    bool andC = (cpu.regs().p & 0x01) != 0;
    bool andZ = (cpu.regs().p & 0x02) != 0;
    bool andV = (cpu.regs().p & 0x40) != 0;
    bool andN = (cpu.regs().p & 0x80) != 0;

    cpu.step();
    bool eorC = (cpu.regs().p & 0x01) != 0;
    bool eorZ = (cpu.regs().p & 0x02) != 0;
    bool eorV = (cpu.regs().p & 0x40) != 0;
    bool eorN = (cpu.regs().p & 0x80) != 0;

    if (cpu.regs().a != 0x0F) {
      std::cerr << "FAIL: Expected final A=$0F after ORA/AND/EOR sequence, got $" << std::hex
                << static_cast<int>(cpu.regs().a) << std::dec << "\n";
      failures++;
    } else if (!oraC || oraZ || !oraV || !oraN) {
      std::cerr << "FAIL: ORA expected C=1,Z=0,V=1,N=1 after A=$C0\n";
      failures++;
    } else if (!andC || !andZ || !andV || andN) {
      std::cerr << "FAIL: AND expected C=1,Z=1,V=1,N=0 after A=$00\n";
      failures++;
    } else if (!eorC || eorZ || !eorV || eorN) {
      std::cerr << "FAIL: EOR expected C=1,Z=0,V=1,N=0 after A=$0F\n";
      failures++;
    } else {
      std::cout << "PASS: logic_ora_and_eor_nz_contracts\n";
    }
  }

  // Test 24: cmp_does_not_mutate_registers_and_sets_czn
  {
    std::cout << "Test 24: cmp_does_not_mutate_registers_and_sets_czn\n";

    prodos8emu::Apple2Memory mem;
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t start = 0x0B00;
    writeProgram(mem, start,
                 {
                     0xA9,
                     0x40,
                     0xA2,
                     0x10,
                     0xA0,
                     0x80,
                     0xC9,
                     0x40,
                     0xC9,
                     0x50,
                     0xE0,
                     0x0F,
                     0xC0,
                     0x80,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.reset();

    cpu.step();
    cpu.step();
    cpu.step();

    cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x40);

    cpu.step();
    uint8_t aAfterCmpEq = cpu.regs().a;
    uint8_t xAfterCmpEq = cpu.regs().x;
    uint8_t yAfterCmpEq = cpu.regs().y;
    bool    cmpEqC      = (cpu.regs().p & 0x01) != 0;
    bool    cmpEqZ      = (cpu.regs().p & 0x02) != 0;
    bool    cmpEqN      = (cpu.regs().p & 0x80) != 0;
    bool    cmpEqV      = (cpu.regs().p & 0x40) != 0;

    cpu.step();
    uint8_t aAfterCmpLt = cpu.regs().a;
    uint8_t xAfterCmpLt = cpu.regs().x;
    uint8_t yAfterCmpLt = cpu.regs().y;
    bool    cmpLtC      = (cpu.regs().p & 0x01) != 0;
    bool    cmpLtZ      = (cpu.regs().p & 0x02) != 0;
    bool    cmpLtN      = (cpu.regs().p & 0x80) != 0;
    bool    cmpLtV      = (cpu.regs().p & 0x40) != 0;

    cpu.step();
    uint8_t aAfterCpx = cpu.regs().a;
    uint8_t xAfterCpx = cpu.regs().x;
    uint8_t yAfterCpx = cpu.regs().y;
    bool    cpxC      = (cpu.regs().p & 0x01) != 0;
    bool    cpxZ      = (cpu.regs().p & 0x02) != 0;
    bool    cpxN      = (cpu.regs().p & 0x80) != 0;
    bool    cpxV      = (cpu.regs().p & 0x40) != 0;

    cpu.step();
    uint8_t aAfterCpy = cpu.regs().a;
    uint8_t xAfterCpy = cpu.regs().x;
    uint8_t yAfterCpy = cpu.regs().y;
    bool    cpyC      = (cpu.regs().p & 0x01) != 0;
    bool    cpyZ      = (cpu.regs().p & 0x02) != 0;
    bool    cpyN      = (cpu.regs().p & 0x80) != 0;
    bool    cpyV      = (cpu.regs().p & 0x40) != 0;

    if (aAfterCmpEq != 0x40 || xAfterCmpEq != 0x10 || yAfterCmpEq != 0x80 || !cmpEqC || !cmpEqZ ||
        cmpEqN || !cmpEqV) {
      std::cerr << "FAIL: CMP equal expected A/X/Y unchanged and C=1,Z=1,N=0,V unchanged\n";
      failures++;
    } else if (aAfterCmpLt != 0x40 || xAfterCmpLt != 0x10 || yAfterCmpLt != 0x80 || cmpLtC ||
               cmpLtZ || !cmpLtN || !cmpLtV) {
      std::cerr << "FAIL: CMP less-than expected A/X/Y unchanged and C=0,Z=0,N=1,V unchanged\n";
      failures++;
    } else if (aAfterCpx != 0x40 || xAfterCpx != 0x10 || yAfterCpx != 0x80 || !cpxC || cpxZ ||
               cpxN || !cpxV) {
      std::cerr << "FAIL: CPX expected A/X/Y unchanged and C=1,Z=0,N=0,V unchanged\n";
      failures++;
    } else if (aAfterCpy != 0x40 || xAfterCpy != 0x10 || yAfterCpy != 0x80 || !cpyC || !cpyZ ||
               cpyN || !cpyV) {
      std::cerr << "FAIL: CPY expected A/X/Y unchanged and C=1,Z=1,N=0,V unchanged\n";
      failures++;
    } else {
      std::cout << "PASS: cmp_does_not_mutate_registers_and_sets_czn\n";
    }
  }

  // Test 25: rmw_inc_dec_shift_rotate_writeback_and_flags
  {
    std::cout << "Test 25: rmw_inc_dec_shift_rotate_writeback_and_flags\n";

    bool testFailed = false;

    {
      // INC/DEC memory writeback + N/Z, while preserving C.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0010, 0xFF);
      prodos8emu::write_u8(mem.banks(), 0x0011, 0x00);

      const uint16_t start = 0x0B80;
      writeProgram(mem, start,
                   {
                       0x38,
                       0xE6,
                       0x10,
                       0xC6,
                       0x11,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      cpu.step();
      cpu.step();

      uint8_t incMem = prodos8emu::read_u8(mem.constBanks(), 0x0010);
      bool    incC   = (cpu.regs().p & 0x01) != 0;
      bool    incZ   = (cpu.regs().p & 0x02) != 0;
      bool    incN   = (cpu.regs().p & 0x80) != 0;

      cpu.step();

      uint8_t decMem = prodos8emu::read_u8(mem.constBanks(), 0x0011);
      bool    decC   = (cpu.regs().p & 0x01) != 0;
      bool    decZ   = (cpu.regs().p & 0x02) != 0;
      bool    decN   = (cpu.regs().p & 0x80) != 0;

      if (incMem != 0x00 || !incC || !incZ || incN) {
        std::cerr << "FAIL: INC expected M[$10]=$00,C=1,Z=1,N=0\n";
        failures++;
        testFailed = true;
      } else if (decMem != 0xFF || !decC || decZ || !decN) {
        std::cerr << "FAIL: DEC expected M[$11]=$FF,C=1,Z=0,N=1\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // Shift/rotate memory writeback + C/N/Z contracts.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0012, 0x80);
      prodos8emu::write_u8(mem.banks(), 0x0013, 0x01);
      prodos8emu::write_u8(mem.banks(), 0x0014, 0x80);
      prodos8emu::write_u8(mem.banks(), 0x0015, 0x01);

      const uint16_t start = 0x0BA0;
      writeProgram(mem, start,
                   {
                       0x06,
                       0x12,
                       0x46,
                       0x13,
                       0x38,
                       0x26,
                       0x14,
                       0x38,
                       0x66,
                       0x15,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      cpu.step();
      uint8_t aslMem = prodos8emu::read_u8(mem.constBanks(), 0x0012);
      bool    aslC   = (cpu.regs().p & 0x01) != 0;
      bool    aslZ   = (cpu.regs().p & 0x02) != 0;
      bool    aslN   = (cpu.regs().p & 0x80) != 0;

      cpu.step();
      uint8_t lsrMem = prodos8emu::read_u8(mem.constBanks(), 0x0013);
      bool    lsrC   = (cpu.regs().p & 0x01) != 0;
      bool    lsrZ   = (cpu.regs().p & 0x02) != 0;
      bool    lsrN   = (cpu.regs().p & 0x80) != 0;

      cpu.step();
      cpu.step();
      uint8_t rolMem = prodos8emu::read_u8(mem.constBanks(), 0x0014);
      bool    rolC   = (cpu.regs().p & 0x01) != 0;
      bool    rolZ   = (cpu.regs().p & 0x02) != 0;
      bool    rolN   = (cpu.regs().p & 0x80) != 0;

      cpu.step();
      cpu.step();
      uint8_t rorMem = prodos8emu::read_u8(mem.constBanks(), 0x0015);
      bool    rorC   = (cpu.regs().p & 0x01) != 0;
      bool    rorZ   = (cpu.regs().p & 0x02) != 0;
      bool    rorN   = (cpu.regs().p & 0x80) != 0;

      if (aslMem != 0x00 || !aslC || !aslZ || aslN) {
        std::cerr << "FAIL: ASL expected M[$12]=$00,C=1,Z=1,N=0\n";
        failures++;
        testFailed = true;
      } else if (lsrMem != 0x00 || !lsrC || !lsrZ || lsrN) {
        std::cerr << "FAIL: LSR expected M[$13]=$00,C=1,Z=1,N=0\n";
        failures++;
        testFailed = true;
      } else if (rolMem != 0x01 || !rolC || rolZ || rolN) {
        std::cerr << "FAIL: ROL expected M[$14]=$01,C=1,Z=0,N=0\n";
        failures++;
        testFailed = true;
      } else if (rorMem != 0x80 || !rorC || rorZ || !rorN) {
        std::cerr << "FAIL: ROR expected M[$15]=$80,C=1,Z=0,N=1\n";
        failures++;
        testFailed = true;
      } else if (cpu.regs().a != 0x00 || cpu.regs().x != 0x00 || cpu.regs().y != 0x00) {
        std::cerr << "FAIL: Expected memory RMW sequence to keep A/X/Y unchanged\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: rmw_inc_dec_shift_rotate_writeback_and_flags\n";
    }
  }

  // Test 26: alu_family_dispatch_cycles_match_baseline
  {
    std::cout << "Test 26: alu_family_dispatch_cycles_match_baseline\n";

    bool testFailed = false;

    {
      // ORA abs,X with page cross: 4+1 cycles.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2100, 0xF0);

      const uint16_t start = 0x0C00;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x0F,
                       0xA2,
                       0x01,
                       0x1D,
                       0xFF,
                       0x20,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();

      bool z = (cpu.regs().p & 0x02) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;

      if (cycles != 5 || cpu.regs().a != 0xFF || z || !n) {
        std::cerr << "FAIL: ORA abs,X page-cross expected cycles=5,A=$FF,Z=0,N=1\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // AND abs,X without page cross: 4 cycles.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x20FF, 0x0F);

      const uint16_t start = 0x0C20;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0xF0,
                       0xA2,
                       0x01,
                       0x3D,
                       0xFE,
                       0x20,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();

      bool z = (cpu.regs().p & 0x02) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;

      if (cycles != 4 || cpu.regs().a != 0x00 || !z || n) {
        std::cerr << "FAIL: AND abs,X no-cross expected cycles=4,A=$00,Z=1,N=0\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // EOR (zp),Y with page cross: 5+1 cycles.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u16_le(mem.banks(), 0x0010, 0x21FF);
      prodos8emu::write_u8(mem.banks(), 0x2200, 0x0F);

      const uint16_t start = 0x0C40;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0xF0,
                       0xA0,
                       0x01,
                       0x51,
                       0x10,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();

      bool z = (cpu.regs().p & 0x02) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;

      if (cycles != 6 || cpu.regs().a != 0xFF || z || !n) {
        std::cerr << "FAIL: EOR (zp),Y page-cross expected cycles=6,A=$FF,Z=0,N=1\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // ADC (zp): 5 cycles.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u16_le(mem.banks(), 0x0020, 0x2000);
      prodos8emu::write_u8(mem.banks(), 0x2000, 0x02);

      const uint16_t start = 0x0C60;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x01,
                       0x18,
                       0x72,
                       0x20,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;

      if (cycles != 5 || cpu.regs().a != 0x03 || c || z || n) {
        std::cerr << "FAIL: ADC (zp) expected cycles=5,A=$03,C=0,Z=0,N=0\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // SBC abs: 4 cycles.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2000, 0x01);

      const uint16_t start = 0x0C80;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x05,
                       0x38,
                       0xED,
                       0x00,
                       0x20,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;

      if (cycles != 4 || cpu.regs().a != 0x04 || !c || z || n) {
        std::cerr << "FAIL: SBC abs expected cycles=4,A=$04,C=1,Z=0,N=0\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // CMP #imm: 2 cycles and A must remain unchanged.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0CA0;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x33,
                       0xC9,
                       0x44,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();

      bool c = (cpu.regs().p & 0x01) != 0;
      bool z = (cpu.regs().p & 0x02) != 0;
      bool n = (cpu.regs().p & 0x80) != 0;

      if (cycles != 2 || cpu.regs().a != 0x33 || c || z || !n) {
        std::cerr << "FAIL: CMP #imm expected cycles=2,A unchanged,C=0,Z=0,N=1\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: alu_family_dispatch_cycles_match_baseline\n";
    }
  }

  // Test 27: rmw_family_dispatch_preserves_carry_nz_and_memory
  {
    std::cout << "Test 27: rmw_family_dispatch_preserves_carry_nz_and_memory\n";

    bool testFailed = false;

    {
      // INC abs,X: cycles=7, memory writeback and C preserved.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2010, 0xFF);

      const uint16_t start = 0x0CC0;
      writeProgram(mem, start,
                   {
                       0x38,
                       0xA2,
                       0x01,
                       0xFE,
                       0x0F,
                       0x20,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();

      uint8_t memVal = prodos8emu::read_u8(mem.constBanks(), 0x2010);
      bool    c      = (cpu.regs().p & 0x01) != 0;
      bool    z      = (cpu.regs().p & 0x02) != 0;
      bool    n      = (cpu.regs().p & 0x80) != 0;

      if (cycles != 7 || memVal != 0x00 || !c || !z || n) {
        std::cerr << "FAIL: INC abs,X expected cycles=7,M=$00,C preserved=1,Z=1,N=0\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // DEC zp: cycles=5, memory writeback and C preserved.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0040, 0x00);

      const uint16_t start = 0x0CE0;
      writeProgram(mem, start,
                   {
                       0x38,
                       0xC6,
                       0x40,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();

      uint8_t memVal = prodos8emu::read_u8(mem.constBanks(), 0x0040);
      bool    c      = (cpu.regs().p & 0x01) != 0;
      bool    z      = (cpu.regs().p & 0x02) != 0;
      bool    n      = (cpu.regs().p & 0x80) != 0;

      if (cycles != 5 || memVal != 0xFF || !c || z || !n) {
        std::cerr << "FAIL: DEC zp expected cycles=5,M=$FF,C preserved=1,Z=0,N=1\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // ASL abs: cycles=6, carry from bit 7.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2030, 0x80);

      const uint16_t start = 0x0D00;
      writeProgram(mem, start,
                   {
                       0x0E,
                       0x30,
                       0x20,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t cycles = cpu.step();
      uint8_t  memVal = prodos8emu::read_u8(mem.constBanks(), 0x2030);
      bool     c      = (cpu.regs().p & 0x01) != 0;
      bool     z      = (cpu.regs().p & 0x02) != 0;
      bool     n      = (cpu.regs().p & 0x80) != 0;

      if (cycles != 6 || memVal != 0x00 || !c || !z || n) {
        std::cerr << "FAIL: ASL abs expected cycles=6,M=$00,C=1,Z=1,N=0\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // LSR zp,X: cycles=6, carry from bit 0.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0051, 0x01);

      const uint16_t start = 0x0D20;
      writeProgram(mem, start,
                   {
                       0xA2,
                       0x01,
                       0x56,
                       0x50,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      uint8_t  memVal = prodos8emu::read_u8(mem.constBanks(), 0x0051);
      bool     c      = (cpu.regs().p & 0x01) != 0;
      bool     z      = (cpu.regs().p & 0x02) != 0;
      bool     n      = (cpu.regs().p & 0x80) != 0;

      if (cycles != 6 || memVal != 0x00 || !c || !z || n) {
        std::cerr << "FAIL: LSR zp,X expected cycles=6,M=$00,C=1,Z=1,N=0\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // ROL zp: cycles=5, uses incoming carry and updates carry from bit 7.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0060, 0x80);

      const uint16_t start = 0x0D40;
      writeProgram(mem, start,
                   {
                       0x38,
                       0x26,
                       0x60,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      uint8_t  memVal = prodos8emu::read_u8(mem.constBanks(), 0x0060);
      bool     c      = (cpu.regs().p & 0x01) != 0;
      bool     z      = (cpu.regs().p & 0x02) != 0;
      bool     n      = (cpu.regs().p & 0x80) != 0;

      if (cycles != 5 || memVal != 0x01 || !c || z || n) {
        std::cerr << "FAIL: ROL zp expected cycles=5,M=$01,C=1,Z=0,N=0\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // ROR abs,X: cycles=7, uses incoming carry and updates carry from bit 0.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2041, 0x01);

      const uint16_t start = 0x0D60;
      writeProgram(mem, start,
                   {
                       0x38,
                       0xA2,
                       0x01,
                       0x7E,
                       0x40,
                       0x20,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();
      uint8_t  memVal = prodos8emu::read_u8(mem.constBanks(), 0x2041);
      bool     c      = (cpu.regs().p & 0x01) != 0;
      bool     z      = (cpu.regs().p & 0x02) != 0;
      bool     n      = (cpu.regs().p & 0x80) != 0;

      if (cycles != 7 || memVal != 0x80 || !c || z || !n) {
        std::cerr << "FAIL: ROR abs,X expected cycles=7,M=$80,C=1,Z=0,N=1\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: rmw_family_dispatch_preserves_carry_nz_and_memory\n";
    }
  }

  // Test 28: execute_special_decode_order_preserved
  {
    std::cout << "Test 28: execute_special_decode_order_preserved\n";

    bool testFailed = false;

    {
      // RMB0 zp ($07) must be handled by special decode path before default switch fallback.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0E00;
      prodos8emu::write_u8(mem.banks(), 0x0010, 0xFF);
      writeProgram(mem, start,
                   {
                       0x07,
                       0x10,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t cycles = cpu.step();
      uint8_t  zpVal  = prodos8emu::read_u8(mem.constBanks(), 0x0010);

      if (cycles != 5 || zpVal != 0xFE || cpu.regs().pc != static_cast<uint16_t>(start + 2)) {
        std::cerr << "FAIL: RMB0 decode precedence expected cycles=5,zp=$FE,pc=start+2\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // BBR0 zp,rel ($0F) must branch via special decode path.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0E20;
      prodos8emu::write_u8(mem.banks(), 0x0011, 0x00);  // bit 0 clear => branch taken
      writeProgram(mem, start,
                   {
                       0x0F,
                       0x11,
                       0x02,
                       0xEA,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t cycles = cpu.step();

      if (cycles != 5 || cpu.regs().pc != static_cast<uint16_t>(start + 5)) {
        std::cerr << "FAIL: BBR0 decode precedence expected cycles=5,branch to start+5\n";
        failures++;
        testFailed = true;
      }
    }

    {
      // BBS0 zp,rel ($8F) must branch via special decode path.
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0E40;
      prodos8emu::write_u8(mem.banks(), 0x0012, 0x01);  // bit 0 set => branch taken
      writeProgram(mem, start,
                   {
                       0x8F,
                       0x12,
                       0x02,
                       0xEA,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t cycles = cpu.step();

      if (cycles != 5 || cpu.regs().pc != static_cast<uint16_t>(start + 5)) {
        std::cerr << "FAIL: BBS0 decode precedence expected cycles=5,branch to start+5\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: execute_special_decode_order_preserved\n";
    }
  }

  // Test 29: mli_on_line_volume_list_logging_stable
  {
    std::cout << "Test 29: mli_on_line_volume_list_logging_stable\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    fs::path volume = tempDir / "ONVOL";
    fs::create_directories(volume);

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t onLineParam  = 0x0430;
    const uint16_t onLineBuffer = 0x0640;
    prodos8emu::write_u8(mem.banks(), onLineParam + 0, 2);
    prodos8emu::write_u8(mem.banks(), onLineParam + 1, 0);
    prodos8emu::write_u16_le(mem.banks(), onLineParam + 2, onLineBuffer);

    const uint16_t start = 0x0E80;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0xC5,
                     static_cast<uint8_t>(onLineParam & 0xFF),
                     static_cast<uint8_t>((onLineParam >> 8) & 0xFF),
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    cpu.step();

    const std::string logText = mliLog.str();

    size_t callPos    = logText.find(" MLI call=$C5 (ON_LINE)");
    size_t paramPos   = logText.find(" param=$0430");
    size_t volumesPos = logText.find(" volumes='");
    size_t onvolPos   = logText.find("ONVOL[S1D0]");
    size_t resultPos  = logText.find(" result=$00");
    size_t okPos      = logText.find(" OK");

    if (callPos == std::string::npos || paramPos == std::string::npos ||
        volumesPos == std::string::npos || onvolPos == std::string::npos ||
        resultPos == std::string::npos || okPos == std::string::npos) {
      std::cerr << "FAIL: Expected ON_LINE volume-list log fields were missing, got:\n"
                << logText << "\n";
      failures++;
    } else if (!(callPos < paramPos && paramPos < volumesPos && volumesPos < onvolPos &&
                 onvolPos < resultPos && resultPos < okPos)) {
      std::cerr << "FAIL: Expected ON_LINE log field order call->param->volumes->result->OK, "
                   "got:\n"
                << logText << "\n";
      failures++;
    } else {
      std::cout << "PASS: mli_on_line_volume_list_logging_stable\n";
    }
  }

  // Test 30: mli_read_directory_entry_logging_stable
  {
    std::cout << "Test 30: mli_read_directory_entry_logging_stable\n";

    prodos8emu::Apple2Memory mem;
    prodos8emu::MLIContext   ctx(tempDir);

    fs::path volume = tempDir / "VOLD";
    fs::create_directories(volume);
    std::ofstream(volume / "ALPHA") << "alpha data";

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    const uint16_t    dirPathAddr = 0x0540;
    const std::string dirPath     = "/VOLD";
    prodos8emu::write_u8(mem.banks(), dirPathAddr, static_cast<uint8_t>(dirPath.length()));
    for (size_t i = 0; i < dirPath.length(); i++) {
      prodos8emu::write_u8(mem.banks(), static_cast<uint16_t>(dirPathAddr + 1 + i),
                           static_cast<uint8_t>(dirPath[i]));
    }

    const uint16_t openParam = 0x0440;
    prodos8emu::write_u8(mem.banks(), openParam + 0, 3);
    prodos8emu::write_u16_le(mem.banks(), openParam + 1, dirPathAddr);
    prodos8emu::write_u16_le(mem.banks(), openParam + 3, 0x2800);
    prodos8emu::write_u8(mem.banks(), openParam + 5, 0);

    uint8_t openErr = ctx.openCall(mem.banks(), openParam);
    uint8_t refNum  = prodos8emu::read_u8(mem.constBanks(), openParam + 5);

    const uint16_t setMarkParam = 0x0450;
    prodos8emu::write_u8(mem.banks(), setMarkParam + 0, 2);
    prodos8emu::write_u8(mem.banks(), setMarkParam + 1, refNum);
    prodos8emu::write_u24_le(mem.banks(), setMarkParam + 2, 43);
    uint8_t setMarkErr = ctx.setMarkCall(mem.constBanks(), setMarkParam);

    const uint16_t readParam = 0x0460;
    const uint16_t readBuf   = 0x0660;
    prodos8emu::write_u8(mem.banks(), readParam + 0, 4);
    prodos8emu::write_u8(mem.banks(), readParam + 1, refNum);
    prodos8emu::write_u16_le(mem.banks(), readParam + 2, readBuf);
    prodos8emu::write_u16_le(mem.banks(), readParam + 4, 39);
    prodos8emu::write_u16_le(mem.banks(), readParam + 6, 0);

    const uint16_t start = 0x0EA0;
    writeProgram(mem, start,
                 {
                     0x20,
                     0x00,
                     0xBF,
                     0xCA,
                     static_cast<uint8_t>(readParam & 0xFF),
                     static_cast<uint8_t>((readParam >> 8) & 0xFF),
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    std::stringstream mliLog;
    cpu.setDebugLogs(&mliLog, nullptr);
    cpu.reset();

    cpu.step();

    const std::string logText = mliLog.str();

    size_t callPos    = logText.find(" MLI call=$CA (READ)");
    size_t paramPos   = logText.find(" param=$0460");
    size_t refPos     = logText.find(" ref=" + std::to_string(refNum));
    size_t reqPos     = logText.find(" req=39");
    size_t transPos   = logText.find(" trans=39");
    size_t firstMark  = logText.find(" mark=$43");
    size_t eofPos     = logText.find(" eof=$512");
    size_t secondMark = (firstMark == std::string::npos) ? std::string::npos
                                                         : logText.find(" mark=$43", firstMark + 1);
    size_t blkPos     = logText.find(" blk=0+43");
    size_t entryPos   = logText.find(" [ent1]");
    size_t entriesPos = logText.find(" entries='ALPHA'");
    size_t resultPos  = logText.find(" result=$00");
    size_t okPos      = logText.find(" OK");

    if (openErr != 0) {
      std::cerr << "FAIL: Expected OPEN precondition for directory read logging to succeed, got 0x"
                << std::hex << static_cast<int>(openErr) << std::dec << "\n";
      failures++;
    } else if (setMarkErr != 0) {
      std::cerr << "FAIL: Expected SET_MARK precondition for directory read logging to succeed, "
                   "got 0x"
                << std::hex << static_cast<int>(setMarkErr) << std::dec << "\n";
      failures++;
    } else if (callPos == std::string::npos || paramPos == std::string::npos ||
               refPos == std::string::npos || reqPos == std::string::npos ||
               transPos == std::string::npos || firstMark == std::string::npos ||
               eofPos == std::string::npos || secondMark == std::string::npos ||
               blkPos == std::string::npos || entryPos == std::string::npos ||
               entriesPos == std::string::npos || resultPos == std::string::npos ||
               okPos == std::string::npos) {
      std::cerr << "FAIL: Expected READ directory-entry log fields were missing, got:\n"
                << logText << "\n";
      failures++;
    } else if (!(callPos < paramPos && paramPos < refPos && refPos < reqPos && reqPos < transPos &&
                 transPos < firstMark && firstMark < eofPos && eofPos < secondMark &&
                 secondMark < blkPos && blkPos < entryPos && entryPos < entriesPos &&
                 entriesPos < resultPos && resultPos < okPos)) {
      std::cerr << "FAIL: Expected READ directory log field order to remain stable, got:\n"
                << logText << "\n";
      failures++;
    } else {
      std::cout << "PASS: mli_read_directory_entry_logging_stable\n";
    }
  }

  // Test 31: execute_flag_transfer_stack_contracts
  {
    std::cout << "Test 31: execute_flag_transfer_stack_contracts\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0EC0;
      writeProgram(mem, start,
                   {
                       0x38,
                       0x18,
                       0x78,
                       0x58,
                       0xF8,
                       0xD8,
                       0xB8,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().p = 0x20;

      uint32_t secCycles = cpu.step();
      bool     secC      = (cpu.regs().p & 0x01) != 0;

      uint32_t clcCycles = cpu.step();
      bool     clcC      = (cpu.regs().p & 0x01) != 0;

      uint32_t seiCycles = cpu.step();
      bool     seiI      = (cpu.regs().p & 0x04) != 0;

      uint32_t cliCycles = cpu.step();
      bool     cliI      = (cpu.regs().p & 0x04) != 0;

      uint32_t sedCycles = cpu.step();
      bool     sedD      = (cpu.regs().p & 0x08) != 0;

      uint32_t cldCycles = cpu.step();
      bool     cldD      = (cpu.regs().p & 0x08) != 0;

      cpu.regs().p       = static_cast<uint8_t>(cpu.regs().p | 0x40);
      uint32_t clvCycles = cpu.step();
      bool     clvV      = (cpu.regs().p & 0x40) != 0;

      if (secCycles != 2 || !secC || clcCycles != 2 || clcC || seiCycles != 2 || !seiI ||
          cliCycles != 2 || cliI || sedCycles != 2 || !sedD || cldCycles != 2 || cldD ||
          clvCycles != 2 || clvV) {
        std::cerr << "FAIL: Flag opcode fallback contract mismatch for C/I/D/V operations\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0EE0;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x80,
                       0xAA,
                       0x8A,
                       0xA9,
                       0x00,
                       0xA8,
                       0x98,
                       0xA2,
                       0xFF,
                       0x9A,
                       0xBA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t taxCycles = cpu.step();
      uint8_t  taxX      = cpu.regs().x;
      bool     taxZ      = (cpu.regs().p & 0x02) != 0;
      bool     taxN      = (cpu.regs().p & 0x80) != 0;

      uint32_t txaCycles = cpu.step();
      uint8_t  txaA      = cpu.regs().a;
      bool     txaZ      = (cpu.regs().p & 0x02) != 0;
      bool     txaN      = (cpu.regs().p & 0x80) != 0;

      (void)cpu.step();
      uint32_t tayCycles = cpu.step();
      uint8_t  tayY      = cpu.regs().y;
      bool     tayZ      = (cpu.regs().p & 0x02) != 0;
      bool     tayN      = (cpu.regs().p & 0x80) != 0;

      uint32_t tyaCycles = cpu.step();
      uint8_t  tyaA      = cpu.regs().a;
      bool     tyaZ      = (cpu.regs().p & 0x02) != 0;
      bool     tyaN      = (cpu.regs().p & 0x80) != 0;

      (void)cpu.step();

      cpu.regs().p = static_cast<uint8_t>((cpu.regs().p & static_cast<uint8_t>(~0x82)) | 0x02);
      uint32_t txsCycles = cpu.step();
      bool     txsZ      = (cpu.regs().p & 0x02) != 0;
      bool     txsN      = (cpu.regs().p & 0x80) != 0;

      uint32_t tsxCycles = cpu.step();
      uint8_t  tsxX      = cpu.regs().x;
      bool     tsxZ      = (cpu.regs().p & 0x02) != 0;
      bool     tsxN      = (cpu.regs().p & 0x80) != 0;

      if (taxCycles != 2 || taxX != 0x80 || taxZ || !taxN || txaCycles != 2 || txaA != 0x80 ||
          txaZ || !txaN || tayCycles != 2 || tayY != 0x00 || !tayZ || tayN || tyaCycles != 2 ||
          tyaA != 0x00 || !tyaZ || tyaN || txsCycles != 2 || cpu.regs().sp != 0xFF || !txsZ ||
          txsN || tsxCycles != 2 || tsxX != 0xFF || tsxZ || !tsxN) {
        std::cerr
            << "FAIL: Transfer opcode fallback contract mismatch for register and NZ behavior\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x0F20;
      writeProgram(mem, start,
                   {
                       0x08, 0x28, 0xA9, 0x42, 0x48, 0xA9, 0x00, 0x68, 0xA2, 0x80,
                       0xDA, 0xA2, 0x00, 0xFA, 0xA0, 0x01, 0x5A, 0xA0, 0x00, 0x7A,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      cpu.regs().p = 0x41;

      uint32_t phpCycles       = cpu.step();
      uint8_t  stackedStatus   = prodos8emu::read_u8(mem.constBanks(), 0x01FF);
      uint8_t  expectedStacked = static_cast<uint8_t>(0x41 | 0x10 | 0x20);

      cpu.regs().p       = 0x00;
      uint32_t plpCycles = cpu.step();

      (void)cpu.step();
      uint32_t phaCycles = cpu.step();
      uint8_t  phaByte   = prodos8emu::read_u8(mem.constBanks(), 0x01FF);

      (void)cpu.step();
      uint32_t plaCycles = cpu.step();
      bool     plaZ      = (cpu.regs().p & 0x02) != 0;
      bool     plaN      = (cpu.regs().p & 0x80) != 0;

      (void)cpu.step();
      uint32_t phxCycles = cpu.step();
      uint8_t  phxByte   = prodos8emu::read_u8(mem.constBanks(), 0x01FF);

      (void)cpu.step();
      uint32_t plxCycles = cpu.step();
      bool     plxZ      = (cpu.regs().p & 0x02) != 0;
      bool     plxN      = (cpu.regs().p & 0x80) != 0;

      (void)cpu.step();
      uint32_t phyCycles = cpu.step();
      uint8_t  phyByte   = prodos8emu::read_u8(mem.constBanks(), 0x01FF);

      (void)cpu.step();
      uint32_t plyCycles = cpu.step();
      bool     plyZ      = (cpu.regs().p & 0x02) != 0;
      bool     plyN      = (cpu.regs().p & 0x80) != 0;

      if (phpCycles != 3 || stackedStatus != expectedStacked || plpCycles != 4 ||
          cpu.regs().p != expectedStacked || phaCycles != 3 || phaByte != 0x42 || plaCycles != 4 ||
          cpu.regs().a != 0x42 || plaZ || plaN || phxCycles != 3 || phxByte != 0x80 ||
          plxCycles != 4 || cpu.regs().x != 0x80 || plxZ || !plxN || phyCycles != 3 ||
          phyByte != 0x01 || plyCycles != 4 || cpu.regs().y != 0x01 || plyZ || plyN ||
          cpu.regs().sp != 0xFF) {
        std::cerr
            << "FAIL: Stack opcode fallback contract mismatch for push/pull cycles or state\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: execute_flag_transfer_stack_contracts\n";
    }
  }

  // Test 32: bit_tsb_trb_flag_and_writeback_contracts
  {
    std::cout << "Test 32: bit_tsb_trb_flag_and_writeback_contracts\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x1000;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0xF0,
                       0x89,
                       0x0F,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      cpu.regs().p = static_cast<uint8_t>((cpu.regs().p | 0xC0) & static_cast<uint8_t>(~0x02));

      uint32_t bitImmCycles = cpu.step();
      bool     z            = (cpu.regs().p & 0x02) != 0;
      bool     n            = (cpu.regs().p & 0x80) != 0;
      bool     v            = (cpu.regs().p & 0x40) != 0;

      if (bitImmCycles != 2 || cpu.regs().a != 0xF0 || !z || !n || !v) {
        std::cerr << "FAIL: BIT #imm contract mismatch for Z update and N/V preservation\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0020, 0xC0);
      prodos8emu::write_u8(mem.banks(), 0x0021, 0x00);
      prodos8emu::write_u8(mem.banks(), 0x3000, 0x40);
      prodos8emu::write_u8(mem.banks(), 0x3100, 0x80);

      const uint16_t start = 0x1020;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0xFF,
                       0x24,
                       0x20,
                       0x2C,
                       0x00,
                       0x30,
                       0xA2,
                       0x01,
                       0x34,
                       0x20,
                       0x3C,
                       0xFF,
                       0x30,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();

      uint32_t bitZpCycles = cpu.step();
      bool     bitZpZ      = (cpu.regs().p & 0x02) != 0;
      bool     bitZpN      = (cpu.regs().p & 0x80) != 0;
      bool     bitZpV      = (cpu.regs().p & 0x40) != 0;

      uint32_t bitAbsCycles = cpu.step();
      bool     bitAbsZ      = (cpu.regs().p & 0x02) != 0;
      bool     bitAbsN      = (cpu.regs().p & 0x80) != 0;
      bool     bitAbsV      = (cpu.regs().p & 0x40) != 0;

      (void)cpu.step();

      uint32_t bitZpxCycles = cpu.step();
      bool     bitZpxZ      = (cpu.regs().p & 0x02) != 0;
      bool     bitZpxN      = (cpu.regs().p & 0x80) != 0;
      bool     bitZpxV      = (cpu.regs().p & 0x40) != 0;

      uint32_t bitAbsxCycles = cpu.step();
      bool     bitAbsxZ      = (cpu.regs().p & 0x02) != 0;
      bool     bitAbsxN      = (cpu.regs().p & 0x80) != 0;
      bool     bitAbsxV      = (cpu.regs().p & 0x40) != 0;

      if (bitZpCycles != 3 || bitZpZ || !bitZpN || !bitZpV || bitAbsCycles != 4 || bitAbsZ ||
          bitAbsN || !bitAbsV || bitZpxCycles != 4 || !bitZpxZ || bitZpxN || bitZpxV ||
          bitAbsxCycles != 5 || bitAbsxZ || !bitAbsxN || bitAbsxV) {
        std::cerr << "FAIL: BIT memory mode contract mismatch for NZV and cycle behavior\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0030, 0xF0);
      prodos8emu::write_u8(mem.banks(), 0x0031, 0x03);
      prodos8emu::write_u8(mem.banks(), 0x4000, 0x3C);
      prodos8emu::write_u8(mem.banks(), 0x4001, 0xF0);

      const uint16_t start = 0x1040;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x0F,
                       0x04,
                       0x30,
                       0x0C,
                       0x00,
                       0x40,
                       0x14,
                       0x31,
                       0x1C,
                       0x01,
                       0x40,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      cpu.regs().p = static_cast<uint8_t>((cpu.regs().p | 0xC0) & static_cast<uint8_t>(~0x02));

      uint32_t tsbZpCycles = cpu.step();
      uint8_t  tsbZpMem    = prodos8emu::read_u8(mem.constBanks(), 0x0030);
      bool     tsbZpZ      = (cpu.regs().p & 0x02) != 0;
      bool     tsbZpN      = (cpu.regs().p & 0x80) != 0;
      bool     tsbZpV      = (cpu.regs().p & 0x40) != 0;

      uint32_t tsbAbsCycles = cpu.step();
      uint8_t  tsbAbsMem    = prodos8emu::read_u8(mem.constBanks(), 0x4000);
      bool     tsbAbsZ      = (cpu.regs().p & 0x02) != 0;
      bool     tsbAbsN      = (cpu.regs().p & 0x80) != 0;
      bool     tsbAbsV      = (cpu.regs().p & 0x40) != 0;

      uint32_t trbZpCycles = cpu.step();
      uint8_t  trbZpMem    = prodos8emu::read_u8(mem.constBanks(), 0x0031);
      bool     trbZpZ      = (cpu.regs().p & 0x02) != 0;
      bool     trbZpN      = (cpu.regs().p & 0x80) != 0;
      bool     trbZpV      = (cpu.regs().p & 0x40) != 0;

      uint32_t trbAbsCycles = cpu.step();
      uint8_t  trbAbsMem    = prodos8emu::read_u8(mem.constBanks(), 0x4001);
      bool     trbAbsZ      = (cpu.regs().p & 0x02) != 0;
      bool     trbAbsN      = (cpu.regs().p & 0x80) != 0;
      bool     trbAbsV      = (cpu.regs().p & 0x40) != 0;

      if (tsbZpCycles != 5 || tsbZpMem != 0xFF || !tsbZpZ || !tsbZpN || !tsbZpV ||
          tsbAbsCycles != 6 || tsbAbsMem != 0x3F || tsbAbsZ || !tsbAbsN || !tsbAbsV ||
          trbZpCycles != 5 || trbZpMem != 0x00 || trbZpZ || !trbZpN || !trbZpV ||
          trbAbsCycles != 6 || trbAbsMem != 0xF0 || !trbAbsZ || !trbAbsN || !trbAbsV ||
          cpu.regs().a != 0x0F) {
        std::cerr << "FAIL: TSB/TRB contract mismatch for Z behavior, writeback, or cycles\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: bit_tsb_trb_flag_and_writeback_contracts\n";
    }
  }

  // Test 33: undocumented_nop_cycles_and_pc_advance_contracts
  {
    std::cout << "Test 33: undocumented_nop_cycles_and_pc_advance_contracts\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x1080;
      writeProgram(mem, start,
                   {
                       0x03,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().a  = 0x12;
      cpu.regs().x  = 0x34;
      cpu.regs().y  = 0x56;
      cpu.regs().sp = 0xEE;
      cpu.regs().p  = 0xA5;

      uint32_t cycles = cpu.step();
      if (cycles != 1 || cpu.regs().pc != static_cast<uint16_t>(start + 1) ||
          cpu.regs().a != 0x12 || cpu.regs().x != 0x34 || cpu.regs().y != 0x56 ||
          cpu.regs().sp != 0xEE || cpu.regs().p != 0xA5) {
        std::cerr << "FAIL: 1-byte undocumented NOP contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x10A0;
      writeProgram(mem, start,
                   {
                       0x82,
                       0x99,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().a  = 0x22;
      cpu.regs().x  = 0x33;
      cpu.regs().y  = 0x44;
      cpu.regs().sp = 0xDD;
      cpu.regs().p  = 0x65;

      uint32_t cycles = cpu.step();
      if (cycles != 2 || cpu.regs().pc != static_cast<uint16_t>(start + 2) ||
          cpu.regs().a != 0x22 || cpu.regs().x != 0x33 || cpu.regs().y != 0x44 ||
          cpu.regs().sp != 0xDD || cpu.regs().p != 0x65) {
        std::cerr << "FAIL: Immediate undocumented NOP contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0044, 0xAB);

      const uint16_t start = 0x10C0;
      writeProgram(mem, start,
                   {
                       0x44,
                       0x44,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().a  = 0x55;
      cpu.regs().x  = 0x66;
      cpu.regs().y  = 0x77;
      cpu.regs().sp = 0xCC;
      cpu.regs().p  = 0x27;

      uint32_t cycles = cpu.step();
      if (cycles != 3 || cpu.regs().pc != static_cast<uint16_t>(start + 2) ||
          cpu.regs().a != 0x55 || cpu.regs().x != 0x66 || cpu.regs().y != 0x77 ||
          cpu.regs().sp != 0xCC || cpu.regs().p != 0x27) {
        std::cerr << "FAIL: Zero-page undocumented NOP contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x0046, 0xBC);

      const uint16_t start = 0x10E0;
      writeProgram(mem, start,
                   {
                       0x54,
                       0x45,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().a  = 0x11;
      cpu.regs().x  = 0x01;
      cpu.regs().y  = 0x22;
      cpu.regs().sp = 0xBB;
      cpu.regs().p  = 0x47;

      uint32_t cycles = cpu.step();
      if (cycles != 4 || cpu.regs().pc != static_cast<uint16_t>(start + 2) ||
          cpu.regs().a != 0x11 || cpu.regs().x != 0x01 || cpu.regs().y != 0x22 ||
          cpu.regs().sp != 0xBB || cpu.regs().p != 0x47) {
        std::cerr << "FAIL: Zero-page,X undocumented NOP contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2345, 0xCD);

      const uint16_t start = 0x1100;
      writeProgram(mem, start,
                   {
                       0xDC,
                       0x45,
                       0x23,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().a  = 0x99;
      cpu.regs().x  = 0x88;
      cpu.regs().y  = 0x77;
      cpu.regs().sp = 0xAA;
      cpu.regs().p  = 0x23;

      uint32_t cycles = cpu.step();
      if (cycles != 4 || cpu.regs().pc != static_cast<uint16_t>(start + 3) ||
          cpu.regs().a != 0x99 || cpu.regs().x != 0x88 || cpu.regs().y != 0x77 ||
          cpu.regs().sp != 0xAA || cpu.regs().p != 0x23) {
        std::cerr << "FAIL: Absolute undocumented NOP contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x3456, 0xDE);

      const uint16_t start = 0x1120;
      writeProgram(mem, start,
                   {
                       0x5C,
                       0x56,
                       0x34,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();
      cpu.regs().a  = 0xAA;
      cpu.regs().x  = 0xBB;
      cpu.regs().y  = 0xCC;
      cpu.regs().sp = 0x99;
      cpu.regs().p  = 0x21;

      uint32_t cycles = cpu.step();
      if (cycles != 8 || cpu.regs().pc != static_cast<uint16_t>(start + 3) ||
          cpu.regs().a != 0xAA || cpu.regs().x != 0xBB || cpu.regs().y != 0xCC ||
          cpu.regs().sp != 0x99 || cpu.regs().p != 0x21) {
        std::cerr << "FAIL: Absolute long undocumented NOP contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: undocumented_nop_cycles_and_pc_advance_contracts\n";
    }
  }

  // Test 34: low_risk_group_dispatch_cycles_preserved
  {
    std::cout << "Test 34: low_risk_group_dispatch_cycles_preserved\n";

    bool testFailed = false;

    struct OpcodeCase {
      uint8_t  opcode;
      uint32_t expectedCycles;
    };

    const OpcodeCase opcodeCases[] = {
        {0x18, 2}, {0x38, 2}, {0x58, 2}, {0x78, 2}, {0xD8, 2}, {0xF8, 2}, {0xB8, 2},
        {0xAA, 2}, {0x8A, 2}, {0xA8, 2}, {0x98, 2}, {0xBA, 2}, {0x9A, 2}, {0x48, 3},
        {0x68, 4}, {0x08, 3}, {0x28, 4}, {0xDA, 3}, {0xFA, 4}, {0x5A, 3}, {0x7A, 4},
        {0x1A, 2}, {0x3A, 2}, {0x0A, 2}, {0x4A, 2}, {0x2A, 2}, {0x6A, 2},
    };

    for (const OpcodeCase& opcodeCase : opcodeCases) {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x1140;
      writeProgram(mem, start,
                   {
                       opcodeCase.opcode,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      cpu.regs().a  = 0x42;
      cpu.regs().x  = 0x24;
      cpu.regs().y  = 0x81;
      cpu.regs().sp = 0xFF;
      cpu.regs().p  = 0x20;

      switch (opcodeCase.opcode) {
        case 0x18:
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x01);
          break;
        case 0x38:
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p & static_cast<uint8_t>(~0x01));
          break;
        case 0x58:
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x04);
          break;
        case 0x78:
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p & static_cast<uint8_t>(~0x04));
          break;
        case 0xD8:
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x08);
          break;
        case 0xF8:
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p & static_cast<uint8_t>(~0x08));
          break;
        case 0xB8:
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x40);
          break;

        case 0xAA:
          cpu.regs().a = 0x80;
          break;
        case 0x8A:
          cpu.regs().x = 0x00;
          break;
        case 0xA8:
          cpu.regs().a = 0x7F;
          break;
        case 0x98:
          cpu.regs().y = 0x80;
          break;
        case 0xBA:
          cpu.regs().sp = 0x00;
          break;
        case 0x9A:
          cpu.regs().x = 0x44;
          cpu.regs().p = 0xA2;
          break;

        case 0x48:
          cpu.regs().a = 0x3C;
          break;
        case 0x68:
          cpu.regs().sp = 0xFE;
          prodos8emu::write_u8(mem.banks(), 0x01FF, 0x00);
          break;
        case 0x08:
          cpu.regs().p = 0x41;
          break;
        case 0x28:
          cpu.regs().sp = 0xFE;
          prodos8emu::write_u8(mem.banks(), 0x01FF, 0x41);
          break;
        case 0xDA:
          cpu.regs().x = 0xA5;
          break;
        case 0xFA:
          cpu.regs().sp = 0xFE;
          prodos8emu::write_u8(mem.banks(), 0x01FF, 0x80);
          break;
        case 0x5A:
          cpu.regs().y = 0x5E;
          break;
        case 0x7A:
          cpu.regs().sp = 0xFE;
          prodos8emu::write_u8(mem.banks(), 0x01FF, 0x00);
          break;

        case 0x1A:
          cpu.regs().a = 0x7F;
          break;
        case 0x3A:
          cpu.regs().a = 0x01;
          break;
        case 0x0A:
          cpu.regs().a = 0x80;
          break;
        case 0x4A:
          cpu.regs().a = 0x01;
          break;
        case 0x2A:
          cpu.regs().a = 0x80;
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x01);
          break;
        case 0x6A:
          cpu.regs().a = 0x01;
          cpu.regs().p = static_cast<uint8_t>(cpu.regs().p | 0x01);
          break;
      }

      uint32_t cycles = cpu.step();
      if (cycles != opcodeCase.expectedCycles ||
          cpu.regs().pc != static_cast<uint16_t>(start + 1)) {
        std::cerr << "FAIL: low-risk dispatch cycle/PC mismatch for opcode 0x" << std::hex
                  << static_cast<int>(opcodeCase.opcode) << std::dec << " (cycles=" << cycles
                  << ", expected=" << opcodeCase.expectedCycles << ", pc=0x" << std::hex
                  << cpu.regs().pc << std::dec << ")\n";
        failures++;
        testFailed = true;
        continue;
      }

      switch (opcodeCase.opcode) {
        case 0x18:
          if ((cpu.regs().p & 0x01) != 0) {
            std::cerr << "FAIL: CLC did not clear carry\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x38:
          if ((cpu.regs().p & 0x01) == 0) {
            std::cerr << "FAIL: SEC did not set carry\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x58:
          if ((cpu.regs().p & 0x04) != 0) {
            std::cerr << "FAIL: CLI did not clear interrupt disable\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x78:
          if ((cpu.regs().p & 0x04) == 0) {
            std::cerr << "FAIL: SEI did not set interrupt disable\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0xD8:
          if ((cpu.regs().p & 0x08) != 0) {
            std::cerr << "FAIL: CLD did not clear decimal mode\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0xF8:
          if ((cpu.regs().p & 0x08) == 0) {
            std::cerr << "FAIL: SED did not set decimal mode\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0xB8:
          if ((cpu.regs().p & 0x40) != 0) {
            std::cerr << "FAIL: CLV did not clear overflow\n";
            failures++;
            testFailed = true;
          }
          break;

        case 0xAA:
          if (cpu.regs().x != 0x80 || (cpu.regs().p & 0x80) == 0 || (cpu.regs().p & 0x02) != 0) {
            std::cerr << "FAIL: TAX state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x8A:
          if (cpu.regs().a != 0x00 || (cpu.regs().p & 0x02) == 0 || (cpu.regs().p & 0x80) != 0) {
            std::cerr << "FAIL: TXA state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0xA8:
          if (cpu.regs().y != 0x7F || (cpu.regs().p & 0x02) != 0 || (cpu.regs().p & 0x80) != 0) {
            std::cerr << "FAIL: TAY state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x98:
          if (cpu.regs().a != 0x80 || (cpu.regs().p & 0x80) == 0 || (cpu.regs().p & 0x02) != 0) {
            std::cerr << "FAIL: TYA state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0xBA:
          if (cpu.regs().x != 0x00 || (cpu.regs().p & 0x02) == 0 || (cpu.regs().p & 0x80) != 0) {
            std::cerr << "FAIL: TSX state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x9A:
          if (cpu.regs().sp != 0x44 || cpu.regs().p != 0xA2) {
            std::cerr << "FAIL: TXS state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;

        case 0x48:
          if (cpu.regs().sp != 0xFE || prodos8emu::read_u8(mem.constBanks(), 0x01FF) != 0x3C) {
            std::cerr << "FAIL: PHA state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x68:
          if (cpu.regs().a != 0x00 || cpu.regs().sp != 0xFF || (cpu.regs().p & 0x02) == 0 ||
              (cpu.regs().p & 0x80) != 0) {
            std::cerr << "FAIL: PLA state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x08:
          if (cpu.regs().sp != 0xFE || prodos8emu::read_u8(mem.constBanks(), 0x01FF) != 0x71) {
            std::cerr << "FAIL: PHP state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x28:
          if (cpu.regs().sp != 0xFF || cpu.regs().p != 0x61) {
            std::cerr << "FAIL: PLP state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0xDA:
          if (cpu.regs().sp != 0xFE || prodos8emu::read_u8(mem.constBanks(), 0x01FF) != 0xA5) {
            std::cerr << "FAIL: PHX state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0xFA:
          if (cpu.regs().x != 0x80 || cpu.regs().sp != 0xFF || (cpu.regs().p & 0x80) == 0 ||
              (cpu.regs().p & 0x02) != 0) {
            std::cerr << "FAIL: PLX state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x5A:
          if (cpu.regs().sp != 0xFE || prodos8emu::read_u8(mem.constBanks(), 0x01FF) != 0x5E) {
            std::cerr << "FAIL: PHY state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x7A:
          if (cpu.regs().y != 0x00 || cpu.regs().sp != 0xFF || (cpu.regs().p & 0x02) == 0 ||
              (cpu.regs().p & 0x80) != 0) {
            std::cerr << "FAIL: PLY state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;

        case 0x1A:
          if (cpu.regs().a != 0x80 || (cpu.regs().p & 0x80) == 0 || (cpu.regs().p & 0x02) != 0) {
            std::cerr << "FAIL: INC A state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x3A:
          if (cpu.regs().a != 0x00 || (cpu.regs().p & 0x02) == 0 || (cpu.regs().p & 0x80) != 0) {
            std::cerr << "FAIL: DEC A state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x0A:
          if (cpu.regs().a != 0x00 || (cpu.regs().p & 0x01) == 0 || (cpu.regs().p & 0x02) == 0) {
            std::cerr << "FAIL: ASL A state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x4A:
          if (cpu.regs().a != 0x00 || (cpu.regs().p & 0x01) == 0 || (cpu.regs().p & 0x02) == 0) {
            std::cerr << "FAIL: LSR A state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x2A:
          if (cpu.regs().a != 0x01 || (cpu.regs().p & 0x01) == 0 || (cpu.regs().p & 0x02) != 0 ||
              (cpu.regs().p & 0x80) != 0) {
            std::cerr << "FAIL: ROL A state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
        case 0x6A:
          if (cpu.regs().a != 0x80 || (cpu.regs().p & 0x01) == 0 || (cpu.regs().p & 0x80) == 0 ||
              (cpu.regs().p & 0x02) != 0) {
            std::cerr << "FAIL: ROR A state mismatch\n";
            failures++;
            testFailed = true;
          }
          break;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: low_risk_group_dispatch_cycles_preserved\n";
    }
  }

  // Test 35: load_store_addressing_cycle_matrix_preserved
  {
    std::cout << "Test 35: load_store_addressing_cycle_matrix_preserved\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2101, 0x34);

      const uint16_t start = 0x1160;
      writeProgram(mem, start,
                   {
                       0xA2,
                       0x01,
                       0xBD,
                       0x00,
                       0x21,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 4 || cpu.regs().a != 0x34 || (cpu.regs().p & 0x02) != 0 ||
          (cpu.regs().p & 0x80) != 0 || cpu.regs().pc != static_cast<uint16_t>(start + 5)) {
        std::cerr << "FAIL: LDA abs,X no-cross cycle/PC/NZ contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2200, 0x80);

      const uint16_t start = 0x1180;
      writeProgram(mem, start,
                   {
                       0xA2,
                       0x01,
                       0xBD,
                       0xFF,
                       0x21,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 5 || cpu.regs().a != 0x80 || (cpu.regs().p & 0x80) == 0 ||
          (cpu.regs().p & 0x02) != 0 || cpu.regs().pc != static_cast<uint16_t>(start + 5)) {
        std::cerr << "FAIL: LDA abs,X page-cross cycle/PC/NZ contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u16_le(mem.banks(), 0x0010, 0x2300);
      prodos8emu::write_u8(mem.banks(), 0x2301, 0x00);

      const uint16_t start = 0x11A0;
      writeProgram(mem, start,
                   {
                       0xA0,
                       0x01,
                       0xB1,
                       0x10,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 5 || cpu.regs().a != 0x00 || (cpu.regs().p & 0x02) == 0 ||
          (cpu.regs().p & 0x80) != 0 || cpu.regs().pc != static_cast<uint16_t>(start + 4)) {
        std::cerr << "FAIL: LDA (zp),Y no-cross cycle/PC/NZ contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u16_le(mem.banks(), 0x0012, 0x23FF);
      prodos8emu::write_u8(mem.banks(), 0x2400, 0x7F);

      const uint16_t start = 0x11C0;
      writeProgram(mem, start,
                   {
                       0xA0,
                       0x01,
                       0xB1,
                       0x12,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 6 || cpu.regs().a != 0x7F || (cpu.regs().p & 0x02) != 0 ||
          (cpu.regs().p & 0x80) != 0 || cpu.regs().pc != static_cast<uint16_t>(start + 4)) {
        std::cerr << "FAIL: LDA (zp),Y page-cross cycle/PC/NZ contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2500, 0x5A);

      const uint16_t start = 0x11E0;
      writeProgram(mem, start,
                   {
                       0xA0,
                       0x01,
                       0xBE,
                       0xFF,
                       0x24,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 5 || cpu.regs().x != 0x5A || (cpu.regs().p & 0x02) != 0 ||
          (cpu.regs().p & 0x80) != 0 || cpu.regs().pc != static_cast<uint16_t>(start + 5)) {
        std::cerr << "FAIL: LDX abs,Y page-cross cycle/PC/NZ contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2600, 0x00);

      const uint16_t start = 0x1200;
      writeProgram(mem, start,
                   {
                       0xA2,
                       0x01,
                       0xBC,
                       0xFF,
                       0x25,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 5 || cpu.regs().y != 0x00 || (cpu.regs().p & 0x02) == 0 ||
          (cpu.regs().p & 0x80) != 0 || cpu.regs().pc != static_cast<uint16_t>(start + 5)) {
        std::cerr << "FAIL: LDY abs,X page-cross cycle/PC/NZ contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x1220;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x5A,
                       0xA2,
                       0x01,
                       0x9D,
                       0xFF,
                       0x28,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 5 || prodos8emu::read_u8(mem.constBanks(), 0x2900) != 0x5A ||
          cpu.regs().pc != static_cast<uint16_t>(start + 7)) {
        std::cerr << "FAIL: STA abs,X write/cycle/PC contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u16_le(mem.banks(), 0x0020, 0x2AFF);

      const uint16_t start = 0x1240;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0xA5,
                       0xA0,
                       0x01,
                       0x91,
                       0x20,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 6 || prodos8emu::read_u8(mem.constBanks(), 0x2B00) != 0xA5 ||
          cpu.regs().pc != static_cast<uint16_t>(start + 6)) {
        std::cerr << "FAIL: STA (zp),Y write/cycle/PC contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u16_le(mem.banks(), 0x0024, 0x2C10);

      const uint16_t start = 0x1260;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x3C,
                       0x92,
                       0x24,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 5 || prodos8emu::read_u8(mem.constBanks(), 0x2C10) != 0x3C ||
          cpu.regs().pc != static_cast<uint16_t>(start + 4)) {
        std::cerr << "FAIL: STA (zp) write/cycle/PC contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x1280;
      writeProgram(mem, start,
                   {
                       0xA2,
                       0x7E,
                       0xA0,
                       0x03,
                       0x96,
                       0x30,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 4 || prodos8emu::read_u8(mem.constBanks(), 0x0033) != 0x7E ||
          cpu.regs().pc != static_cast<uint16_t>(start + 6)) {
        std::cerr << "FAIL: STX zp,Y write/cycle/PC contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x12A0;
      writeProgram(mem, start,
                   {
                       0xA0,
                       0x42,
                       0xA2,
                       0x04,
                       0x94,
                       0x31,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 4 || prodos8emu::read_u8(mem.constBanks(), 0x0035) != 0x42 ||
          cpu.regs().pc != static_cast<uint16_t>(start + 6)) {
        std::cerr << "FAIL: STY zp,X write/cycle/PC contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x2E00, 0x99);

      const uint16_t start = 0x12C0;
      writeProgram(mem, start,
                   {
                       0xA2,
                       0x01,
                       0x9E,
                       0xFF,
                       0x2D,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t cycles = cpu.step();
      if (cycles != 5 || prodos8emu::read_u8(mem.constBanks(), 0x2E00) != 0x00 ||
          cpu.regs().pc != static_cast<uint16_t>(start + 5)) {
        std::cerr << "FAIL: STZ abs,X write/cycle/PC contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: load_store_addressing_cycle_matrix_preserved\n";
    }
  }

  // Test 36: bit_family_page_cross_contracts
  {
    std::cout << "Test 36: bit_family_page_cross_contracts\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x31FF, 0x40);
      prodos8emu::write_u8(mem.banks(), 0x3200, 0x80);

      const uint16_t start = 0x1300;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0xFF,
                       0xA2,
                       0x01,
                       0x3C,
                       0xFE,
                       0x31,
                       0x3C,
                       0xFF,
                       0x31,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      (void)cpu.step();

      uint32_t noCrossCycles = cpu.step();
      bool     noCrossZ      = (cpu.regs().p & 0x02) != 0;
      bool     noCrossN      = (cpu.regs().p & 0x80) != 0;
      bool     noCrossV      = (cpu.regs().p & 0x40) != 0;

      uint32_t crossCycles = cpu.step();
      bool     crossZ      = (cpu.regs().p & 0x02) != 0;
      bool     crossN      = (cpu.regs().p & 0x80) != 0;
      bool     crossV      = (cpu.regs().p & 0x40) != 0;

      if (noCrossCycles != 4 || noCrossZ || noCrossN || !noCrossV || crossCycles != 5 ||
          crossZ || !crossN || crossV || cpu.regs().pc != static_cast<uint16_t>(start + 10)) {
        std::cerr << "FAIL: BIT abs,X page-cross cycle/flag/PC contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      const uint16_t start = 0x1320;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0xF0,
                       0x89,
                       0x0F,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      cpu.regs().p = static_cast<uint8_t>((cpu.regs().p | 0xC0) & static_cast<uint8_t>(~0x02));
      uint32_t cycles = cpu.step();
      if (cycles != 2 || (cpu.regs().p & 0x02) == 0 || (cpu.regs().p & 0x80) == 0 ||
          (cpu.regs().p & 0x40) == 0 || cpu.regs().pc != static_cast<uint16_t>(start + 4)) {
        std::cerr << "FAIL: BIT #imm Z-only update contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      prodos8emu::write_u8(mem.banks(), 0x3310, 0xF0);
      prodos8emu::write_u8(mem.banks(), 0x3311, 0xFF);

      const uint16_t start = 0x1340;
      writeProgram(mem, start,
                   {
                       0xA9,
                       0x0F,
                       0x0C,
                       0x10,
                       0x33,
                       0x1C,
                       0x11,
                       0x33,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      (void)cpu.step();
      uint32_t tsbCycles = cpu.step();
      bool     tsbZ      = (cpu.regs().p & 0x02) != 0;
      uint8_t  tsbMem    = prodos8emu::read_u8(mem.constBanks(), 0x3310);

      uint32_t trbCycles = cpu.step();
      bool     trbZ      = (cpu.regs().p & 0x02) != 0;
      uint8_t  trbMem    = prodos8emu::read_u8(mem.constBanks(), 0x3311);

      if (tsbCycles != 6 || !tsbZ || tsbMem != 0xFF || trbCycles != 6 || trbZ || trbMem != 0xF0 ||
          cpu.regs().pc != static_cast<uint16_t>(start + 8)) {
        std::cerr << "FAIL: TSB/TRB absolute cycle/writeback contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: bit_family_page_cross_contracts\n";
    }
  }

  // Test 37: nop_bus_read_shape_contracts
  {
    std::cout << "Test 37: nop_bus_read_shape_contracts\n";

    bool testFailed = false;

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);
      mem.setLCBank1(true);

      const uint16_t start = 0x1360;
      writeProgram(mem, start,
                   {
                       0x03,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t cycles = cpu.step();
      if (cycles != 1 || mem.isLCWritePrequalified() || !mem.isLCWriteEnabled() ||
          !mem.isLCReadEnabled() || !mem.isLCBank1() ||
          cpu.regs().pc != static_cast<uint16_t>(start + 1)) {
        std::cerr << "FAIL: 1-byte NOP bus-shape contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);
      mem.setLCBank1(true);

      const uint16_t start = 0x1380;
      writeProgram(mem, start,
                   {
                       0x82,
                       0x99,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t cycles = cpu.step();
      if (cycles != 2 || mem.isLCWritePrequalified() || !mem.isLCWriteEnabled() ||
          !mem.isLCReadEnabled() || !mem.isLCBank1() ||
          cpu.regs().pc != static_cast<uint16_t>(start + 2)) {
        std::cerr << "FAIL: Immediate NOP bus-shape contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);
      mem.setLCBank1(true);

      const uint16_t start = 0x13A0;
      writeProgram(mem, start,
                   {
                       0xDC,
                       0x81,
                       0xC0,
                       0xDC,
                       0x81,
                       0xC0,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t firstCycles = cpu.step();
      bool     firstPreq   = mem.isLCWritePrequalified();
      bool     firstWrite  = mem.isLCWriteEnabled();
      bool     firstRead   = mem.isLCReadEnabled();
      bool     firstBank1  = mem.isLCBank1();

      uint32_t secondCycles = cpu.step();
      bool     secondPreq   = mem.isLCWritePrequalified();
      bool     secondWrite  = mem.isLCWriteEnabled();
      bool     secondRead   = mem.isLCReadEnabled();
      bool     secondBank1  = mem.isLCBank1();

      if (firstCycles != 4 || !firstPreq || firstWrite || firstRead || firstBank1 ||
          secondCycles != 4 || secondPreq || !secondWrite || secondRead || secondBank1 ||
          cpu.regs().pc != static_cast<uint16_t>(start + 6)) {
        std::cerr << "FAIL: Absolute NOP bus-read side-effect contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      prodos8emu::Apple2Memory mem;
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);
      mem.setLCBank1(false);

      const uint16_t start = 0x13C0;
      writeProgram(mem, start,
                   {
                       0x5C,
                       0x8B,
                       0xC0,
                       0x5C,
                       0x8B,
                       0xC0,
                   });
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(mem);
      cpu.reset();

      uint32_t firstCycles = cpu.step();
      bool     firstPreq   = mem.isLCWritePrequalified();
      bool     firstWrite  = mem.isLCWriteEnabled();
      bool     firstRead   = mem.isLCReadEnabled();
      bool     firstBank1  = mem.isLCBank1();

      uint32_t secondCycles = cpu.step();
      bool     secondPreq   = mem.isLCWritePrequalified();
      bool     secondWrite  = mem.isLCWriteEnabled();
      bool     secondRead   = mem.isLCReadEnabled();
      bool     secondBank1  = mem.isLCBank1();

      if (firstCycles != 8 || !firstPreq || firstWrite || !firstRead || !firstBank1 ||
          secondCycles != 8 || secondPreq || !secondWrite || !secondRead || !secondBank1 ||
          cpu.regs().pc != static_cast<uint16_t>(start + 6)) {
        std::cerr << "FAIL: Absolute-long NOP bus-read side-effect contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: nop_bus_read_shape_contracts\n";
    }
  }

  // Test 38: register_incdec_nz_cycle_contracts
  {
    std::cout << "Test 38: register_incdec_nz_cycle_contracts\n";

    bool testFailed = false;

    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    const uint16_t start = 0x13E0;
    writeProgram(*mem, start,
                 {
                     0xE8,
                     0xCA,
                     0xC8,
                     0x88,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();

    cpu.regs().x = 0xFF;
    cpu.regs().y = 0x7F;
    cpu.regs().p = static_cast<uint8_t>((cpu.regs().p & static_cast<uint8_t>(~0xC3)) | 0x41);

    uint32_t inxCycles = cpu.step();
    bool     inxC      = (cpu.regs().p & 0x01) != 0;
    bool     inxV      = (cpu.regs().p & 0x40) != 0;
    bool     inxZ      = (cpu.regs().p & 0x02) != 0;
    bool     inxN      = (cpu.regs().p & 0x80) != 0;
    uint8_t  inxX      = cpu.regs().x;
    uint16_t inxPc     = cpu.regs().pc;

    uint32_t dexCycles = cpu.step();
    bool     dexC      = (cpu.regs().p & 0x01) != 0;
    bool     dexV      = (cpu.regs().p & 0x40) != 0;
    bool     dexZ      = (cpu.regs().p & 0x02) != 0;
    bool     dexN      = (cpu.regs().p & 0x80) != 0;
    uint8_t  dexX      = cpu.regs().x;
    uint16_t dexPc     = cpu.regs().pc;

    uint32_t inyCycles = cpu.step();
    bool     inyC      = (cpu.regs().p & 0x01) != 0;
    bool     inyV      = (cpu.regs().p & 0x40) != 0;
    bool     inyZ      = (cpu.regs().p & 0x02) != 0;
    bool     inyN      = (cpu.regs().p & 0x80) != 0;
    uint8_t  inyY      = cpu.regs().y;
    uint16_t inyPc     = cpu.regs().pc;

    uint32_t deyCycles = cpu.step();
    bool     deyC      = (cpu.regs().p & 0x01) != 0;
    bool     deyV      = (cpu.regs().p & 0x40) != 0;
    bool     deyZ      = (cpu.regs().p & 0x02) != 0;
    bool     deyN      = (cpu.regs().p & 0x80) != 0;
    uint8_t  deyY      = cpu.regs().y;

    if (inxCycles != 2 || inxPc != static_cast<uint16_t>(start + 1) || inxX != 0x00 ||
        !inxC || !inxV || !inxZ || inxN) {
      std::cerr << "FAIL: INX contract mismatch for cycles/flags\n";
      failures++;
      testFailed = true;
    } else if (dexCycles != 2 || dexPc != static_cast<uint16_t>(start + 2) || dexX != 0xFF ||
               !dexC || !dexV || dexZ || !dexN) {
      std::cerr << "FAIL: DEX contract mismatch for cycles/flags\n";
      failures++;
      testFailed = true;
    } else if (inyCycles != 2 || inyPc != static_cast<uint16_t>(start + 3) || inyY != 0x80 || !inyC ||
               !inyV || inyZ || !inyN) {
      std::cerr << "FAIL: INY contract mismatch for cycles/flags\n";
      failures++;
      testFailed = true;
    } else if (deyCycles != 2 || deyY != 0x7F || !deyC || !deyV || deyZ || deyN ||
               cpu.regs().pc != static_cast<uint16_t>(start + 4)) {
      std::cerr << "FAIL: DEY contract mismatch for cycles/PC/flags\n";
      failures++;
      testFailed = true;
    }

    if (!testFailed) {
      std::cout << "PASS: register_incdec_nz_cycle_contracts\n";
    }
  }

  // Test 39: cpx_cpy_zp_abs_cycle_flag_contracts
  {
    std::cout << "Test 39: cpx_cpy_zp_abs_cycle_flag_contracts\n";

    bool testFailed = false;

    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    prodos8emu::write_u8(mem->banks(), 0x0010, 0x20);
    prodos8emu::write_u8(mem->banks(), 0x0011, 0x30);
    prodos8emu::write_u8(mem->banks(), 0x2000, 0x30);
    prodos8emu::write_u8(mem->banks(), 0x2001, 0x40);

    const uint16_t start = 0x1400;
    writeProgram(*mem, start,
                 {
                     0xA2,
                     0x20,
                     0xA0,
                     0x40,
                     0xE4,
                     0x10,
                     0xEC,
                     0x00,
                     0x20,
                     0xC4,
                     0x11,
                     0xCC,
                     0x01,
                     0x20,
                 });
            prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

            prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();

    (void)cpu.step();
    (void)cpu.step();

    cpu.regs().p = static_cast<uint8_t>((cpu.regs().p & static_cast<uint8_t>(~0xC3)) | 0x40);

    uint32_t cpxZpCycles = cpu.step();
    bool     cpxZpC      = (cpu.regs().p & 0x01) != 0;
    bool     cpxZpZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpxZpN      = (cpu.regs().p & 0x80) != 0;
    bool     cpxZpV      = (cpu.regs().p & 0x40) != 0;

    uint32_t cpxAbsCycles = cpu.step();
    bool     cpxAbsC      = (cpu.regs().p & 0x01) != 0;
    bool     cpxAbsZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpxAbsN      = (cpu.regs().p & 0x80) != 0;
    bool     cpxAbsV      = (cpu.regs().p & 0x40) != 0;

    uint32_t cpyZpCycles = cpu.step();
    bool     cpyZpC      = (cpu.regs().p & 0x01) != 0;
    bool     cpyZpZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpyZpN      = (cpu.regs().p & 0x80) != 0;
    bool     cpyZpV      = (cpu.regs().p & 0x40) != 0;

    uint32_t cpyAbsCycles = cpu.step();
    bool     cpyAbsC      = (cpu.regs().p & 0x01) != 0;
    bool     cpyAbsZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpyAbsN      = (cpu.regs().p & 0x80) != 0;
    bool     cpyAbsV      = (cpu.regs().p & 0x40) != 0;

    if (cpxZpCycles != 3 || cpu.regs().x != 0x20 || cpu.regs().y != 0x40 || !cpxZpC || !cpxZpZ ||
        cpxZpN || !cpxZpV) {
      std::cerr << "FAIL: CPX zp contract mismatch for cycle/flags/register stability\n";
      failures++;
      testFailed = true;
    } else if (cpxAbsCycles != 4 || cpu.regs().x != 0x20 || cpu.regs().y != 0x40 || cpxAbsC ||
               cpxAbsZ || !cpxAbsN || !cpxAbsV) {
      std::cerr << "FAIL: CPX abs contract mismatch for cycle/flags/register stability\n";
      failures++;
      testFailed = true;
    } else if (cpyZpCycles != 3 || cpu.regs().x != 0x20 || cpu.regs().y != 0x40 || !cpyZpC ||
               cpyZpZ || cpyZpN || !cpyZpV) {
      std::cerr << "FAIL: CPY zp contract mismatch for cycle/flags/register stability\n";
      failures++;
      testFailed = true;
    } else if (cpyAbsCycles != 4 || cpu.regs().x != 0x20 || cpu.regs().y != 0x40 || !cpyAbsC ||
               !cpyAbsZ || cpyAbsN || !cpyAbsV || cpu.regs().pc != static_cast<uint16_t>(start + 14)) {
      std::cerr << "FAIL: CPY abs contract mismatch for cycle/PC/flags/register stability\n";
      failures++;
      testFailed = true;
    }

    if (!testFailed) {
      std::cout << "PASS: cpx_cpy_zp_abs_cycle_flag_contracts\n";
    }
  }

  // Test 40: bbr_bbs_not_taken_and_page_cross_contracts
  {
    std::cout << "Test 40: bbr_bbs_not_taken_and_page_cross_contracts\n";

    bool testFailed = false;

    {
      auto mem = std::make_unique<prodos8emu::Apple2Memory>();
      mem->setLCReadEnabled(true);
      mem->setLCWriteEnabled(true);

      prodos8emu::write_u8(mem->banks(), 0x0020, 0x01);

      const uint16_t start = 0x1450;
      writeProgram(*mem, start,
                   {
                       0x0F,
                       0x20,
                       0x05,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(*mem);
      cpu.reset();
      cpu.regs().p = 0xE1;

      uint32_t cycles = cpu.step();
      if (cycles != 5 || cpu.regs().pc != static_cast<uint16_t>(start + 3) || cpu.regs().p != 0xE1) {
        std::cerr << "FAIL: BBR not-taken cycle/PC/flags contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      auto mem = std::make_unique<prodos8emu::Apple2Memory>();
      mem->setLCReadEnabled(true);
      mem->setLCWriteEnabled(true);

      prodos8emu::write_u8(mem->banks(), 0x0021, 0x00);

      const uint16_t start = 0x1470;
      writeProgram(*mem, start,
                   {
                       0x8F,
                       0x21,
                       0x05,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(*mem);
      cpu.reset();
      cpu.regs().p = 0x61;

      uint32_t cycles = cpu.step();
      if (cycles != 5 || cpu.regs().pc != static_cast<uint16_t>(start + 3) || cpu.regs().p != 0x61) {
        std::cerr << "FAIL: BBS not-taken cycle/PC/flags contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      auto mem = std::make_unique<prodos8emu::Apple2Memory>();
      mem->setLCReadEnabled(true);
      mem->setLCWriteEnabled(true);

      prodos8emu::write_u8(mem->banks(), 0x0022, 0x00);

      const uint16_t start = 0x14FD;
      writeProgram(*mem, start,
                   {
                       0x0F,
                       0x22,
                       0x00,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(*mem);
      cpu.reset();
      cpu.regs().p = 0xA1;

      uint32_t cycles = cpu.step();
      if (cycles != 5 || cpu.regs().pc != 0x1500 || cpu.regs().p != 0xA1) {
        std::cerr << "FAIL: BBR page-cross taken cycle/PC/flags contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    {
      auto mem = std::make_unique<prodos8emu::Apple2Memory>();
      mem->setLCReadEnabled(true);
      mem->setLCWriteEnabled(true);

      prodos8emu::write_u8(mem->banks(), 0x0023, 0x01);

      const uint16_t start = 0x15FD;
      writeProgram(*mem, start,
                   {
                       0x8F,
                       0x23,
                       0xFF,
                       0xEA,
                   });
      prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

      prodos8emu::CPU65C02 cpu(*mem);
      cpu.reset();
      cpu.regs().p = 0x23;

      uint32_t cycles = cpu.step();
      if (cycles != 5 || cpu.regs().pc != 0x15FF || cpu.regs().p != 0x23) {
        std::cerr << "FAIL: BBS page-cross taken cycle/PC/flags contract mismatch\n";
        failures++;
        testFailed = true;
      }
    }

    if (!testFailed) {
      std::cout << "PASS: bbr_bbs_not_taken_and_page_cross_contracts\n";
    }
  }

  // Test 41: misc_tail_dispatch_cycles_preserved
  {
    std::cout << "Test 41: misc_tail_dispatch_cycles_preserved\n";

    bool testFailed = false;

    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    const uint16_t start = 0x1620;
    writeProgram(*mem, start,
                 {
                     0xE8,
                     0xCA,
                     0xC8,
                     0x88,
                     0xEA,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();

    cpu.regs().x = 0x00;
    cpu.regs().y = 0x00;
    cpu.regs().p = static_cast<uint8_t>((cpu.regs().p & static_cast<uint8_t>(~0xC3)) | 0x41);

    uint32_t inxCycles = cpu.step();
    uint8_t  inxX      = cpu.regs().x;
    bool     inxZ      = (cpu.regs().p & 0x02) != 0;
    bool     inxN      = (cpu.regs().p & 0x80) != 0;

    uint32_t dexCycles = cpu.step();
    uint8_t  dexX      = cpu.regs().x;
    bool     dexZ      = (cpu.regs().p & 0x02) != 0;
    bool     dexN      = (cpu.regs().p & 0x80) != 0;

    uint32_t inyCycles = cpu.step();
    uint8_t  inyY      = cpu.regs().y;
    bool     inyZ      = (cpu.regs().p & 0x02) != 0;
    bool     inyN      = (cpu.regs().p & 0x80) != 0;

    uint32_t deyCycles = cpu.step();
    uint8_t  deyY      = cpu.regs().y;
    bool     deyZ      = (cpu.regs().p & 0x02) != 0;
    bool     deyN      = (cpu.regs().p & 0x80) != 0;

    uint32_t nopCycles = cpu.step();

    if (inxCycles != 2 || inxX != 0x01 || inxZ || inxN) {
      std::cerr << "FAIL: INX dispatch cycle/flag contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (dexCycles != 2 || dexX != 0x00 || !dexZ || dexN) {
      std::cerr << "FAIL: DEX dispatch cycle/flag contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (inyCycles != 2 || inyY != 0x01 || inyZ || inyN) {
      std::cerr << "FAIL: INY dispatch cycle/flag contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (deyCycles != 2 || deyY != 0x00 || !deyZ || deyN) {
      std::cerr << "FAIL: DEY dispatch cycle/flag contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (nopCycles != 2 || cpu.regs().pc != static_cast<uint16_t>(start + 5)) {
      std::cerr << "FAIL: Tail dispatch PC/cycle sequencing mismatch\n";
      failures++;
      testFailed = true;
    }

    if (!testFailed) {
      std::cout << "PASS: misc_tail_dispatch_cycles_preserved\n";
    }
  }

  // Test 42: compare_xy_dispatch_contracts_preserved
  {
    std::cout << "Test 42: compare_xy_dispatch_contracts_preserved\n";

    bool testFailed = false;

    auto mem = std::make_unique<prodos8emu::Apple2Memory>();
    mem->setLCReadEnabled(true);
    mem->setLCWriteEnabled(true);

    prodos8emu::write_u8(mem->banks(), 0x0012, 0x31);
    prodos8emu::write_u8(mem->banks(), 0x0013, 0x30);
    prodos8emu::write_u8(mem->banks(), 0x2002, 0x30);
    prodos8emu::write_u8(mem->banks(), 0x2003, 0x40);

    const uint16_t start = 0x1660;
    writeProgram(*mem, start,
                 {
                     0xA2,
                     0x30,
                     0xA0,
                     0x31,
                     0xE0,
                     0x30,
                     0xE4,
                     0x12,
                     0xEC,
                     0x02,
                     0x20,
                     0xC0,
                     0x31,
                     0xC4,
                     0x13,
                     0xCC,
                     0x03,
                     0x20,
                 });
    prodos8emu::write_u16_le(mem->banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(*mem);
    cpu.reset();

    (void)cpu.step();
    (void)cpu.step();

    cpu.regs().p = static_cast<uint8_t>((cpu.regs().p & static_cast<uint8_t>(~0xC3)) | 0x40);

    uint32_t cpxImmCycles = cpu.step();
    bool     cpxImmC      = (cpu.regs().p & 0x01) != 0;
    bool     cpxImmZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpxImmN      = (cpu.regs().p & 0x80) != 0;

    uint32_t cpxZpCycles = cpu.step();
    bool     cpxZpC      = (cpu.regs().p & 0x01) != 0;
    bool     cpxZpZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpxZpN      = (cpu.regs().p & 0x80) != 0;

    uint32_t cpxAbsCycles = cpu.step();
    bool     cpxAbsC      = (cpu.regs().p & 0x01) != 0;
    bool     cpxAbsZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpxAbsN      = (cpu.regs().p & 0x80) != 0;

    uint32_t cpyImmCycles = cpu.step();
    bool     cpyImmC      = (cpu.regs().p & 0x01) != 0;
    bool     cpyImmZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpyImmN      = (cpu.regs().p & 0x80) != 0;

    uint32_t cpyZpCycles = cpu.step();
    bool     cpyZpC      = (cpu.regs().p & 0x01) != 0;
    bool     cpyZpZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpyZpN      = (cpu.regs().p & 0x80) != 0;

    uint32_t cpyAbsCycles = cpu.step();
    bool     cpyAbsC      = (cpu.regs().p & 0x01) != 0;
    bool     cpyAbsZ      = (cpu.regs().p & 0x02) != 0;
    bool     cpyAbsN      = (cpu.regs().p & 0x80) != 0;
    bool     cpyAbsV      = (cpu.regs().p & 0x40) != 0;

    if (cpxImmCycles != 2 || !cpxImmC || !cpxImmZ || cpxImmN) {
      std::cerr << "FAIL: CPX imm dispatch contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (cpxZpCycles != 3 || cpxZpC || cpxZpZ || !cpxZpN) {
      std::cerr << "FAIL: CPX zp dispatch contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (cpxAbsCycles != 4 || !cpxAbsC || !cpxAbsZ || cpxAbsN) {
      std::cerr << "FAIL: CPX abs dispatch contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (cpyImmCycles != 2 || !cpyImmC || !cpyImmZ || cpyImmN) {
      std::cerr << "FAIL: CPY imm dispatch contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (cpyZpCycles != 3 || !cpyZpC || cpyZpZ || cpyZpN) {
      std::cerr << "FAIL: CPY zp dispatch contract mismatch\n";
      failures++;
      testFailed = true;
    } else if (cpyAbsCycles != 4 || cpyAbsC || cpyAbsZ || !cpyAbsN || !cpyAbsV ||
               cpu.regs().x != 0x30 || cpu.regs().y != 0x31 ||
               cpu.regs().pc != static_cast<uint16_t>(start + 18)) {
      std::cerr << "FAIL: CPY abs dispatch cycle/flag/register/PC mismatch\n";
      failures++;
      testFailed = true;
    }

    if (!testFailed) {
      std::cout << "PASS: compare_xy_dispatch_contracts_preserved\n";
    }
  }

  run_branch_opcode_matrix_preserved_test(failures);
  run_cout_escape_mapping_contracts_preserved_test(failures);
  run_unknown_opcode_default_fallback_contracts_test(failures);
  run_nop_variant_opcode_matrix_preserved_test(failures);
  run_fallback_router_precedence_contracts_test(failures);

  fs::remove_all(tempDir);

  if (failures == 0) {
    std::cout << "\nAll CPU65C02 tests passed!\n";
  } else {
    std::cout << "\n" << failures << " test(s) failed!\n";
  }
  return failures;
}
