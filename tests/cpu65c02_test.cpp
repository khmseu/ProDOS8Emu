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

  fs::remove_all(tempDir);

  if (failures == 0) {
    std::cout << "\nAll CPU65C02 tests passed!\n";
  } else {
    std::cout << "\n" << failures << " test(s) failed!\n";
  }
  return failures;
}
