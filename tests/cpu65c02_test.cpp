#include "prodos8emu/cpu65c02.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
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

  fs::remove_all(tempDir);

  if (failures == 0) {
    std::cout << "\nAll CPU65C02 tests passed!\n";
  } else {
    std::cout << "\n" << failures << " test(s) failed!\n";
  }
  return failures;
}
