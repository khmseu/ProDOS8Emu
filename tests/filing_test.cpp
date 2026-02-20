#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"
#include "prodos8emu/xattr.hpp"

namespace fs = std::filesystem;

// Helper to set up memory banks
class TestMemory {
 public:
  TestMemory() {
    for (std::size_t i = 0; i < prodos8emu::NUM_BANKS; i++) {
      m_data[i].fill(0);
      m_banks[i]      = m_data[i].data();
      m_constBanks[i] = m_data[i].data();
    }
  }

  prodos8emu::MemoryBanks& banks() {
    return m_banks;
  }

  prodos8emu::ConstMemoryBanks& constBanks() {
    return m_constBanks;
  }

  // Helper to write a counted string to memory
  void writeCountedString(uint16_t addr, const std::string& str) {
    prodos8emu::write_u8(m_banks, addr, static_cast<uint8_t>(str.length()));
    for (size_t i = 0; i < str.length(); i++) {
      prodos8emu::write_u8(m_banks, addr + 1 + i, static_cast<uint8_t>(str[i]));
    }
  }

  // Helper to read a counted string from memory
  std::string readCountedString(uint16_t addr) {
    return prodos8emu::read_counted_string(m_banks, addr);
  }

 private:
  std::array<std::array<uint8_t, prodos8emu::BANK_SIZE>, prodos8emu::NUM_BANKS> m_data;
  prodos8emu::MemoryBanks                                                       m_banks;
  prodos8emu::ConstMemoryBanks                                                  m_constBanks;
};

int main() {
  int failures = 0;

  // Set up test environment
  fs::path tempDir = fs::temp_directory_path() / "prodos8emu_filing_test";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir);

  // Create a test volume
  fs::path volume1 = tempDir / "V1";
  fs::create_directories(volume1);

  std::cout << "Test environment: " << tempDir << "\n";

  // Test 1: OPEN and CLOSE - Create a file, open it, verify ref_num=1, close it
  {
    std::cout << "Test 1: OPEN and CLOSE\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create a test file first
    fs::path testFile = volume1 / "TESTFILE";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "Hello World!";
    }

    // OPEN the file
    mem.writeCountedString(0x0400, "/V1/TESTFILE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);               // param_count
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);  // pathname ptr
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);  // io_buffer ptr

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);
      if (refNum != 1) {
        std::cerr << "FAIL: Expected ref_num=1, got " << (int)refNum << "\n";
        failures++;
      } else {
        // Now CLOSE the file
        prodos8emu::write_u8(mem.banks(), paramBlock, 1);      // param_count
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, 1);  // ref_num

        err = ctx.closeCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
          std::cerr << "FAIL: CLOSE returned error 0x" << std::hex << (int)err << "\n";
          failures++;
        } else {
          std::cout << "PASS: OPEN and CLOSE\n";
        }
      }
    }
  }

  // Test 2: WRITE and READ - Write data to a file, close and reopen, read back and verify
  {
    std::cout << "Test 2: WRITE and READ\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create empty test file
    fs::path testFile = volume1 / "WRITEFILE";
    { std::ofstream out(testFile, std::ios::binary); }

    // OPEN the file
    mem.writeCountedString(0x0400, "/V1/WRITEFILE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN for WRITE returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // Write "HELLO" to the file
      const char* testData = "HELLO";
      for (int i = 0; i < 5; i++) {
        prodos8emu::write_u8(mem.banks(), 0x0500 + i, testData[i]);
      }

      prodos8emu::write_u8(mem.banks(), paramBlock, 4);  // param_count
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x0500);  // data_buffer
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 4, 5);       // request_count

      err = ctx.writeCall(mem.banks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: WRITE returned error 0x" << std::hex << (int)err << "\n";
        failures++;
      } else {
        uint16_t transCount = prodos8emu::read_u16_le(mem.banks(), paramBlock + 6);
        if (transCount != 5) {
          std::cerr << "FAIL: Expected trans_count=5, got " << transCount << "\n";
          failures++;
        } else {
          // Close the file
          prodos8emu::write_u8(mem.banks(), paramBlock, 1);
          prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
          ctx.closeCall(mem.constBanks(), paramBlock);

          // Reopen and read back
          prodos8emu::write_u8(mem.banks(), paramBlock, 3);
          prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
          prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

          err    = ctx.openCall(mem.banks(), paramBlock);
          refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

          // Clear buffer
          for (int i = 0; i < 5; i++) {
            prodos8emu::write_u8(mem.banks(), 0x0500 + i, 0);
          }

          // Read
          prodos8emu::write_u8(mem.banks(), paramBlock, 4);
          prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
          prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x0500);
          prodos8emu::write_u16_le(mem.banks(), paramBlock + 4, 5);

          err = ctx.readCall(mem.banks(), paramBlock);
          if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: READ returned error 0x" << std::hex << (int)err << "\n";
            failures++;
          } else {
            transCount = prodos8emu::read_u16_le(mem.banks(), paramBlock + 6);
            if (transCount != 5) {
              std::cerr << "FAIL: READ trans_count expected 5, got " << transCount << "\n";
              failures++;
            } else {
              bool match = true;
              for (int i = 0; i < 5; i++) {
                if (prodos8emu::read_u8(mem.banks(), 0x0500 + i) != testData[i]) {
                  match = false;
                  break;
                }
              }
              if (!match) {
                std::cerr << "FAIL: READ data mismatch\n";
                failures++;
              } else {
                std::cout << "PASS: WRITE and READ\n";
              }
            }
          }

          // Close
          prodos8emu::write_u8(mem.banks(), paramBlock, 1);
          prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
          ctx.closeCall(mem.constBanks(), paramBlock);
        }
      }
    }
  }

  // Test 3: NEWLINE mode - Write lines separated by 0x0D, enable newline mode, read line by line
  {
    std::cout << "Test 3: NEWLINE mode\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create file with lines
    fs::path testFile = volume1 / "LINEFILE";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "LINE1\rLINE2\rLINE3";  // \r = 0x0D
    }

    // OPEN
    mem.writeCountedString(0x0400, "/V1/LINEFILE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN for NEWLINE returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // Enable NEWLINE mode: mask=0xFF, char=0x0D (carriage return)
      prodos8emu::write_u8(mem.banks(), paramBlock, 3);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u8(mem.banks(), paramBlock + 2, 0xFF);  // enable_mask
      prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0x0D);  // newline_char

      err = ctx.newlineCall(mem.constBanks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: NEWLINE returned error 0x" << std::hex << (int)err << "\n";
        failures++;
      } else {
        // Read first line (should stop at \r)
        prodos8emu::write_u8(mem.banks(), paramBlock, 4);
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x0500);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 4, 100);  // request more than needed

        err = ctx.readCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
          std::cerr << "FAIL: READ line 1 returned error 0x" << std::hex << (int)err << "\n";
          failures++;
        } else {
          uint16_t transCount = prodos8emu::read_u16_le(mem.banks(), paramBlock + 6);
          if (transCount != 6) {  // "LINE1\r" = 6 bytes
            std::cerr << "FAIL: Expected trans_count=6 for line 1, got " << transCount << "\n";
            failures++;
          } else {
            std::cout << "PASS: NEWLINE mode\n";
          }
        }
      }

      // Close
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      ctx.closeCall(mem.constBanks(), paramBlock);
    }
  }

  // Test 4: SET_MARK / GET_MARK - Seek to different positions, verify GET_MARK
  {
    std::cout << "Test 4: SET_MARK / GET_MARK\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create test file
    fs::path testFile = volume1 / "MARKFILE";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "0123456789ABCDEF";
    }

    // OPEN
    mem.writeCountedString(0x0400, "/V1/MARKFILE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN for MARK returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // SET_MARK to 10
      prodos8emu::write_u8(mem.banks(), paramBlock, 2);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u24_le(mem.banks(), paramBlock + 2, 10);

      err = ctx.setMarkCall(mem.constBanks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: SET_MARK returned error 0x" << std::hex << (int)err << "\n";
        failures++;
      } else {
        // GET_MARK
        prodos8emu::write_u8(mem.banks(), paramBlock, 2);
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);

        err = ctx.getMarkCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
          std::cerr << "FAIL: GET_MARK returned error 0x" << std::hex << (int)err << "\n";
          failures++;
        } else {
          uint32_t mark = prodos8emu::read_u24_le(mem.banks(), paramBlock + 2);
          if (mark != 10) {
            std::cerr << "FAIL: Expected mark=10, got " << mark << "\n";
            failures++;
          } else {
            std::cout << "PASS: SET_MARK / GET_MARK\n";
          }
        }
      }

      // Close
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      ctx.closeCall(mem.constBanks(), paramBlock);
    }
  }

  // Test 5: SET_EOF / GET_EOF - Set EOF to 10 bytes, verify GET_EOF
  {
    std::cout << "Test 5: SET_EOF / GET_EOF\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create test file
    fs::path testFile = volume1 / "EOFFILE";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "0123456789ABCDEFGHIJ";  // 20 bytes
    }

    // OPEN
    mem.writeCountedString(0x0400, "/V1/EOFFILE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN for EOF returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // SET_EOF to 10
      prodos8emu::write_u8(mem.banks(), paramBlock, 2);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u24_le(mem.banks(), paramBlock + 2, 10);

      err = ctx.setEofCall(mem.constBanks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: SET_EOF returned error 0x" << std::hex << (int)err << "\n";
        failures++;
      } else {
        // GET_EOF
        prodos8emu::write_u8(mem.banks(), paramBlock, 2);
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);

        err = ctx.getEofCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
          std::cerr << "FAIL: GET_EOF returned error 0x" << std::hex << (int)err << "\n";
          failures++;
        } else {
          uint32_t eof = prodos8emu::read_u24_le(mem.banks(), paramBlock + 2);
          if (eof != 10) {
            std::cerr << "FAIL: Expected eof=10, got " << eof << "\n";
            failures++;
          } else {
            std::cout << "PASS: SET_EOF / GET_EOF\n";
          }
        }
      }

      // Close
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      ctx.closeCall(mem.constBanks(), paramBlock);
    }
  }

  // Test 6: EOF error when reading past end
  {
    std::cout << "Test 6: EOF error when reading past end\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create small test file
    fs::path testFile = volume1 / "SMALLFILE";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "ABC";  // 3 bytes
    }

    // OPEN
    mem.writeCountedString(0x0400, "/V1/SMALLFILE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN for EOF test returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // SET_MARK to EOF (3)
      prodos8emu::write_u8(mem.banks(), paramBlock, 2);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u24_le(mem.banks(), paramBlock + 2, 3);
      ctx.setMarkCall(mem.constBanks(), paramBlock);

      // Try to READ
      prodos8emu::write_u8(mem.banks(), paramBlock, 4);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x0500);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 4, 10);

      err = ctx.readCall(mem.banks(), paramBlock);
      if (err != prodos8emu::ERR_EOF_ENCOUNTERED) {
        std::cerr << "FAIL: Expected ERR_EOF_ENCOUNTERED, got 0x" << std::hex << (int)err << "\n";
        failures++;
      } else {
        uint16_t transCount = prodos8emu::read_u16_le(mem.banks(), paramBlock + 6);
        if (transCount != 0) {
          std::cerr << "FAIL: Expected trans_count=0 at EOF, got " << transCount << "\n";
          failures++;
        } else {
          std::cout << "PASS: EOF error when reading past end\n";
        }
      }

      // Close
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      ctx.closeCall(mem.constBanks(), paramBlock);
    }
  }

  // Test 7: TOO_MANY_FILES_OPEN - Open 8 files, 9th returns error
  {
    std::cout << "Test 7: TOO_MANY_FILES_OPEN\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create 9 test files
    for (int i = 1; i <= 9; i++) {
      fs::path      testFile = volume1 / ("FILE" + std::to_string(i));
      std::ofstream out(testFile, std::ios::binary);
      out << "test";
    }

    uint8_t refNums[8] = {0};
    bool    success    = true;

    // Open 8 files
    for (int i = 1; i <= 8; i++) {
      std::string filename = "/V1/FILE" + std::to_string(i);
      mem.writeCountedString(0x0400, filename);
      uint16_t paramBlock = 0x0300;
      prodos8emu::write_u8(mem.banks(), paramBlock, 3);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

      uint8_t err = ctx.openCall(mem.banks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: OPEN file " << i << " returned error 0x" << std::hex << (int)err
                  << "\n";
        success = false;
        break;
      }
      refNums[i - 1] = prodos8emu::read_u8(mem.banks(), paramBlock + 5);
    }

    if (success) {
      // Try to open 9th file
      mem.writeCountedString(0x0400, "/V1/FILE9");
      uint16_t paramBlock = 0x0300;
      prodos8emu::write_u8(mem.banks(), paramBlock, 3);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

      uint8_t err = ctx.openCall(mem.banks(), paramBlock);
      if (err != prodos8emu::ERR_TOO_MANY_FILES_OPEN) {
        std::cerr << "FAIL: Expected ERR_TOO_MANY_FILES_OPEN, got 0x" << std::hex << (int)err
                  << "\n";
        failures++;
      } else {
        std::cout << "PASS: TOO_MANY_FILES_OPEN\n";
      }
    } else {
      failures++;
    }

    // Close all files
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 1);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0);  // ref_num=0 closes all
    ctx.closeCall(mem.constBanks(), paramBlock);
  }

  // Test 8: BAD_REF_NUM - Call with invalid ref_num
  {
    std::cout << "Test 8: BAD_REF_NUM\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;

    // Try READ with bad ref_num
    prodos8emu::write_u8(mem.banks(), paramBlock, 4);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 99);  // invalid ref_num
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x0500);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 4, 10);

    uint8_t err = ctx.readCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_BAD_REF_NUM) {
      std::cerr << "FAIL: Expected ERR_BAD_REF_NUM, got 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: BAD_REF_NUM\n";
    }
  }

  // Test 9: FLUSH - Flush a file successfully
  {
    std::cout << "Test 9: FLUSH\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create test file
    fs::path testFile = volume1 / "FLUSHFILE";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "test";
    }

    // OPEN
    mem.writeCountedString(0x0400, "/V1/FLUSHFILE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN for FLUSH returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // FLUSH
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);

      err = ctx.flushCall(mem.constBanks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: FLUSH returned error 0x" << std::hex << (int)err << "\n";
        failures++;
      } else {
        std::cout << "PASS: FLUSH\n";
      }

      // Close
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      ctx.closeCall(mem.constBanks(), paramBlock);
    }
  }

  // Test 10: CLOSE all (ref_num=0) - Open multiple files, close all
  {
    std::cout << "Test 10: CLOSE all (ref_num=0)\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create 3 test files
    for (int i = 1; i <= 3; i++) {
      fs::path      testFile = volume1 / ("CLOSE" + std::to_string(i));
      std::ofstream out(testFile, std::ios::binary);
      out << "test";
    }

    // Open 3 files
    for (int i = 1; i <= 3; i++) {
      std::string filename = "/V1/CLOSE" + std::to_string(i);
      mem.writeCountedString(0x0400, filename);
      uint16_t paramBlock = 0x0300;
      prodos8emu::write_u8(mem.banks(), paramBlock, 3);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

      uint8_t err = ctx.openCall(mem.banks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: OPEN file " << i << " for CLOSE all returned error 0x" << std::hex
                  << (int)err << "\n";
        failures++;
      }
    }

    // Close all with ref_num=0
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 1);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0);  // ref_num=0

    uint8_t err = ctx.closeCall(mem.constBanks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: CLOSE all returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      // Verify all closed by trying to close ref_num=1 (should fail)
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, 1);
      err = ctx.closeCall(mem.constBanks(), paramBlock);
      if (err != prodos8emu::ERR_BAD_REF_NUM) {
        std::cerr << "FAIL: Expected ERR_BAD_REF_NUM after CLOSE all, got 0x" << std::hex
                  << (int)err << "\n";
        failures++;
      } else {
        std::cout << "PASS: CLOSE all (ref_num=0)\n";
      }
    }
  }

  // Test 11: OPEN non-existent file
  {
    std::cout << "Test 11: OPEN non-existent file\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    mem.writeCountedString(0x0400, "/V1/NONEXISTENT");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_FILE_NOT_FOUND) {
      std::cerr << "FAIL: Expected ERR_FILE_NOT_FOUND, got 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: OPEN non-existent file\n";
    }
  }

  // Test 12: param_count validation
  {
    std::cout << "Test 12: param_count validation\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;

    // OPEN with wrong param_count
    mem.writeCountedString(0x0400, "/V1/TESTFILE");
    prodos8emu::write_u8(mem.banks(), paramBlock, 2);  // wrong: should be 3
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_BAD_CALL_PARAM_COUNT) {
      std::cerr << "FAIL: Expected ERR_BAD_CALL_PARAM_COUNT, got 0x" << std::hex << (int)err
                << "\n";
      failures++;
    } else {
      std::cout << "PASS: param_count validation\n";
    }
  }

  // Test 13: SET_MARK out of range
  {
    std::cout << "Test 13: SET_MARK out of range\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create test file
    fs::path testFile = volume1 / "MARKRANGE";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "ABC";  // 3 bytes
    }

    // OPEN
    mem.writeCountedString(0x0400, "/V1/MARKRANGE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN for MARK range returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // SET_MARK beyond EOF
      prodos8emu::write_u8(mem.banks(), paramBlock, 2);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u24_le(mem.banks(), paramBlock + 2, 100);  // beyond EOF

      err = ctx.setMarkCall(mem.constBanks(), paramBlock);
      if (err != prodos8emu::ERR_POSITION_OUT_OF_RANGE) {
        std::cerr << "FAIL: Expected ERR_POSITION_OUT_OF_RANGE, got 0x" << std::hex << (int)err
                  << "\n";
        failures++;
      } else {
        std::cout << "PASS: SET_MARK out of range\n";
      }

      // Close
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      ctx.closeCall(mem.constBanks(), paramBlock);
    }
  }

  // Test 14: Read partial at EOF
  {
    std::cout << "Test 14: Read partial at EOF\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create test file
    fs::path testFile = volume1 / "PARTIAL";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "12345";  // 5 bytes
    }

    // OPEN
    mem.writeCountedString(0x0400, "/V1/PARTIAL");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN for partial read returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // SET_MARK to 3
      prodos8emu::write_u8(mem.banks(), paramBlock, 2);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u24_le(mem.banks(), paramBlock + 2, 3);
      ctx.setMarkCall(mem.constBanks(), paramBlock);

      // Try to READ 10 bytes (only 2 available)
      prodos8emu::write_u8(mem.banks(), paramBlock, 4);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x0500);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 4, 10);

      err                 = ctx.readCall(mem.banks(), paramBlock);
      uint16_t transCount = prodos8emu::read_u16_le(mem.banks(), paramBlock + 6);

      if (transCount != 2) {
        std::cerr << "FAIL: Expected trans_count=2, got " << transCount << "\n";
        failures++;
      } else if (err != prodos8emu::ERR_EOF_ENCOUNTERED) {
        std::cerr << "FAIL: Expected ERR_EOF_ENCOUNTERED, got 0x" << std::hex << (int)err << "\n";
        failures++;
      } else {
        std::cout << "PASS: Read partial at EOF\n";
      }

      // Close
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      ctx.closeCall(mem.constBanks(), paramBlock);
    }
  }

  // Clean up
  fs::remove_all(tempDir);

  // Summary
  if (failures == 0) {
    std::cout << "\nAll tests passed!\n";
  } else {
    std::cout << "\n" << failures << " test(s) failed!\n";
  }

  return failures;
}
