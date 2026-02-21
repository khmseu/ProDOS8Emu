#include <filesystem>
#include <iostream>

#include "prodos8emu/apple2mem.hpp"
#include "prodos8emu/cpu65c02.hpp"
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

int main() {
  int failures = 0;

  fs::path tempDir = fs::temp_directory_path() / "prodos8emu_cpu65c02_test";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir);

  // Test 1: JSR $BF00 triggers MLI dispatch using A=call number and X/Y=param addr
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
    writeProgram(mem, start, {
      0x20, 0x00, 0xBF,
      0x40,
      static_cast<uint8_t>(param & 0xFF),
      static_cast<uint8_t>((param >> 8) & 0xFF),
      0xEA,
    });

    // Set reset vector to $0200
    prodos8emu::write_u16_le(mem.banks(), 0xFFFC, start);

    prodos8emu::CPU65C02 cpu(mem);
    cpu.attachMLI(ctx);
    cpu.reset();

    // Execute a few instructions (JSR trap + NOP)
    cpu.step();
    cpu.step();

    uint8_t slot = prodos8emu::read_u8(mem.constBanks(), param + 1);
    uint8_t a    = cpu.regs().a;
    bool    c    = (cpu.regs().p & 0x01) != 0;

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
    } else {
      std::cout << "PASS: JSR $BF00 MLI trap\n";
    }
  }

  fs::remove_all(tempDir);

  if (failures == 0) {
    std::cout << "\nAll CPU65C02 tests passed!\n";
  } else {
    std::cout << "\n" << failures << " test(s) failed!\n";
  }
  return failures;
}
