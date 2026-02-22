#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "prodos8emu/apple2mem.hpp"
#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"
#include "prodos8emu/system_loader.hpp"

namespace fs = std::filesystem;

// Helper to set up memory banks (shared with other test files)
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

  void writeCountedString(uint16_t addr, const std::string& str) {
    prodos8emu::write_u8(m_banks, addr, static_cast<uint8_t>(str.length()));
    for (size_t i = 0; i < str.length(); i++) {
      prodos8emu::write_u8(m_banks, addr + 1 + i, static_cast<uint8_t>(str[i]));
    }
  }

 private:
  std::array<std::array<uint8_t, prodos8emu::BANK_SIZE>, prodos8emu::NUM_BANKS> m_data;
  prodos8emu::MemoryBanks                                                       m_banks;
  prodos8emu::ConstMemoryBanks                                                  m_constBanks;
};

int main() {
  int failures = 0;

  // Set up test environment
  fs::path tempDir = fs::temp_directory_path() / "prodos8emu_system_test";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir);

  // Create a test volume
  fs::path volume1 = tempDir / "V1";
  fs::create_directories(volume1);

  std::cout << "Test environment: " << tempDir << "\n";

  // Test 1: SET_BUF and GET_BUF - open file, change buffer pointer, read it back
  {
    std::cout << "Test 1: SET_BUF and GET_BUF\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Create a test file
    fs::path testFile = volume1 / "BUFFILE";
    {
      std::ofstream out(testFile, std::ios::binary);
      out << "data";
    }

    // OPEN the file
    mem.writeCountedString(0x0400, "/V1/BUFFILE");
    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0800);  // initial io_buffer

    uint8_t err = ctx.openCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: OPEN returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t refNum = prodos8emu::read_u8(mem.banks(), paramBlock + 5);

      // GET_BUF - verify initial buffer pointer
      prodos8emu::write_u8(mem.banks(), paramBlock, 2);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0);  // clear result

      err = ctx.getBufCall(mem.banks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: GET_BUF (initial) returned error 0x" << std::hex << (int)err << "\n";
        failures++;
      } else {
        uint16_t gotBuf = prodos8emu::read_u16_le(mem.banks(), paramBlock + 2);
        if (gotBuf != 0x0800) {
          std::cerr << "FAIL: Expected initial io_buffer=0x0800, got 0x" << std::hex << gotBuf
                    << "\n";
          failures++;
        } else {
          // SET_BUF - change buffer pointer to 0x1000
          prodos8emu::write_u8(mem.banks(), paramBlock, 2);
          prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
          prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x1000);

          err = ctx.setBufCall(mem.constBanks(), paramBlock);
          if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: SET_BUF returned error 0x" << std::hex << (int)err << "\n";
            failures++;
          } else {
            // GET_BUF - verify new buffer pointer
            prodos8emu::write_u8(mem.banks(), paramBlock, 2);
            prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
            prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0);

            err = ctx.getBufCall(mem.banks(), paramBlock);
            if (err != prodos8emu::ERR_NO_ERROR) {
              std::cerr << "FAIL: GET_BUF (after SET) returned error 0x" << std::hex << (int)err
                        << "\n";
              failures++;
            } else {
              gotBuf = prodos8emu::read_u16_le(mem.banks(), paramBlock + 2);
              if (gotBuf != 0x1000) {
                std::cerr << "FAIL: Expected io_buffer=0x1000, got 0x" << std::hex << gotBuf
                          << "\n";
                failures++;
              } else {
                std::cout << "PASS: SET_BUF and GET_BUF\n";
              }
            }
          }
        }
      }

      // Close
      prodos8emu::write_u8(mem.banks(), paramBlock, 1);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, refNum);
      ctx.closeCall(mem.constBanks(), paramBlock);
    }
  }

  // Test 2: SET_BUF with bad ref_num
  {
    std::cout << "Test 2: SET_BUF with bad ref_num\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 2);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 99);  // invalid ref_num
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x1000);

    uint8_t err = ctx.setBufCall(mem.constBanks(), paramBlock);
    if (err != prodos8emu::ERR_BAD_REF_NUM) {
      std::cerr << "FAIL: Expected ERR_BAD_REF_NUM, got 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: SET_BUF with bad ref_num\n";
    }
  }

  // Test 3: GET_BUF with bad ref_num
  {
    std::cout << "Test 3: GET_BUF with bad ref_num\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 2);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 99);  // invalid ref_num

    uint8_t err = ctx.getBufCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_BAD_REF_NUM) {
      std::cerr << "FAIL: Expected ERR_BAD_REF_NUM, got 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: GET_BUF with bad ref_num\n";
    }
  }

  // Test 4: GET_TIME writes $BF90-$BF93
  {
    std::cout << "Test 4: GET_TIME writes $BF90-$BF93\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    // Clear the time/date locations
    prodos8emu::write_u16_le(mem.banks(), 0xBF90, 0);
    prodos8emu::write_u16_le(mem.banks(), 0xBF92, 0);

    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 0);  // param_count = 0

    time_t  before = ::time(nullptr);
    uint8_t err    = ctx.getTimeCall(mem.banks(), paramBlock);
    time_t  after  = ::time(nullptr);

    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: GET_TIME returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint16_t dateWord = prodos8emu::read_u16_le(mem.banks(), 0xBF90);
      uint16_t timeWord = prodos8emu::read_u16_le(mem.banks(), 0xBF92);

      // Date and time words should be non-zero (unless midnight Jan 0 1900)
      // Verify the encoded date is within the range of before/after timestamps
      // Decode date fields
      int day   = dateWord & 0x1F;
      int month = (dateWord >> 5) & 0x0F;
      int year  = (dateWord >> 9) & 0x7F;  // offset from 1900

      int minute = timeWord & 0x3F;
      int hour   = (timeWord >> 8) & 0x1F;

      // Basic sanity checks
      bool dateOk = (day >= 1 && day <= 31) && (month >= 1 && month <= 12) && (year >= 100);
      bool timeOk = (minute >= 0 && minute <= 59) && (hour >= 0 && hour <= 23);

      if (!dateOk) {
        std::cerr << "FAIL: GET_TIME date fields out of range: day=" << day << " month=" << month
                  << " year+1900=" << (year + 1900) << "\n";
        failures++;
      } else if (!timeOk) {
        std::cerr << "FAIL: GET_TIME time fields out of range: hour=" << hour
                  << " minute=" << minute << "\n";
        failures++;
      } else {
        std::cout << "PASS: GET_TIME writes $BF90-$BF93\n";
      }

      (void)before;
      (void)after;
    }
  }

  // Test 5: GET_TIME param_count validation
  {
    std::cout << "Test 5: GET_TIME param_count validation\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 1);  // wrong: should be 0

    uint8_t err = ctx.getTimeCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_BAD_CALL_PARAM_COUNT) {
      std::cerr << "FAIL: Expected ERR_BAD_CALL_PARAM_COUNT, got 0x" << std::hex << (int)err
                << "\n";
      failures++;
    } else {
      std::cout << "PASS: GET_TIME param_count validation\n";
    }
  }

  // Test 6: ALLOC_INTERRUPT and DEALLOC_INTERRUPT
  {
    std::cout << "Test 6: ALLOC_INTERRUPT and DEALLOC_INTERRUPT\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;

    // ALLOC_INTERRUPT
    prodos8emu::write_u8(mem.banks(), paramBlock, 2);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0);           // int_num (result)
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x2000);  // int_code pointer

    uint8_t err = ctx.allocInterruptCall(mem.banks(), paramBlock);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: ALLOC_INTERRUPT returned error 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      uint8_t intNum = prodos8emu::read_u8(mem.banks(), paramBlock + 1);
      if (intNum < 1 || intNum > 4) {
        std::cerr << "FAIL: Expected int_num in 1-4, got " << (int)intNum << "\n";
        failures++;
      } else {
        // DEALLOC_INTERRUPT
        prodos8emu::write_u8(mem.banks(), paramBlock, 1);
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, intNum);

        err = ctx.deallocInterruptCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
          std::cerr << "FAIL: DEALLOC_INTERRUPT returned error 0x" << std::hex << (int)err << "\n";
          failures++;
        } else {
          std::cout << "PASS: ALLOC_INTERRUPT and DEALLOC_INTERRUPT\n";
        }
      }
    }
  }

  // Test 7: ALLOC_INTERRUPT table full - allocate 4 slots, 5th returns error
  {
    std::cout << "Test 7: ALLOC_INTERRUPT table full\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;
    bool     success    = true;

    for (int i = 0; i < 4; i++) {
      prodos8emu::write_u8(mem.banks(), paramBlock, 2);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 2,
                               static_cast<uint16_t>(0x2000 + i * 0x10));

      uint8_t err = ctx.allocInterruptCall(mem.banks(), paramBlock);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: ALLOC_INTERRUPT slot " << (i + 1) << " returned error 0x" << std::hex
                  << (int)err << "\n";
        success = false;
        break;
      }
    }

    if (success) {
      // 5th alloc should fail
      prodos8emu::write_u8(mem.banks(), paramBlock, 2);
      prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0);
      prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x2040);

      uint8_t err = ctx.allocInterruptCall(mem.banks(), paramBlock);
      if (err != prodos8emu::ERR_INTERRUPT_TABLE_FULL) {
        std::cerr << "FAIL: Expected ERR_INTERRUPT_TABLE_FULL, got 0x" << std::hex << (int)err
                  << "\n";
        failures++;
      } else {
        std::cout << "PASS: ALLOC_INTERRUPT table full\n";
      }
    } else {
      failures++;
    }
  }

  // Test 8: DEALLOC_INTERRUPT with invalid int_num
  {
    std::cout << "Test 8: DEALLOC_INTERRUPT with invalid int_num\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 1);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 5);  // invalid: must be 1-4

    uint8_t err = ctx.deallocInterruptCall(mem.constBanks(), paramBlock);
    if (err != prodos8emu::ERR_INVALID_PARAMETER) {
      std::cerr << "FAIL: Expected ERR_INVALID_PARAMETER, got 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: DEALLOC_INTERRUPT with invalid int_num\n";
    }
  }

  // Test 9: READ_BLOCK returns ERR_IO_ERROR (not supported)
  {
    std::cout << "Test 9: READ_BLOCK returns ERR_IO_ERROR\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0x60);        // unit_num
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x0800);  // data_buffer
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 4, 0);       // block_num

    uint8_t err = ctx.readBlockCall(mem.constBanks(), paramBlock);
    if (err != prodos8emu::ERR_IO_ERROR) {
      std::cerr << "FAIL: Expected ERR_IO_ERROR, got 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: READ_BLOCK returns ERR_IO_ERROR\n";
    }
  }

  // Test 10: WRITE_BLOCK returns ERR_IO_ERROR (not supported)
  {
    std::cout << "Test 10: WRITE_BLOCK returns ERR_IO_ERROR\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 3);
    prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0x60);        // unit_num
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, 0x0800);  // data_buffer
    prodos8emu::write_u16_le(mem.banks(), paramBlock + 4, 0);       // block_num

    uint8_t err = ctx.writeBlockCall(mem.constBanks(), paramBlock);
    if (err != prodos8emu::ERR_IO_ERROR) {
      std::cerr << "FAIL: Expected ERR_IO_ERROR, got 0x" << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: WRITE_BLOCK returns ERR_IO_ERROR\n";
    }
  }

  // Test 11: READ_BLOCK/WRITE_BLOCK param_count validation
  {
    std::cout << "Test 11: Block call param_count validation\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx(tempDir);

    uint16_t paramBlock = 0x0300;
    prodos8emu::write_u8(mem.banks(), paramBlock, 2);  // wrong: should be 3

    uint8_t errR = ctx.readBlockCall(mem.constBanks(), paramBlock);
    uint8_t errW = ctx.writeBlockCall(mem.constBanks(), paramBlock);

    if (errR != prodos8emu::ERR_BAD_CALL_PARAM_COUNT) {
      std::cerr << "FAIL: READ_BLOCK expected ERR_BAD_CALL_PARAM_COUNT, got 0x" << std::hex
                << (int)errR << "\n";
      failures++;
    } else if (errW != prodos8emu::ERR_BAD_CALL_PARAM_COUNT) {
      std::cerr << "FAIL: WRITE_BLOCK expected ERR_BAD_CALL_PARAM_COUNT, got 0x" << std::hex
                << (int)errW << "\n";
      failures++;
    } else {
      std::cout << "PASS: Block call param_count validation\n";
    }
  }

  // Test 12: System file loader - valid system file
  {
    std::cout << "Test 12: System file loader - valid system file\n";
    prodos8emu::Apple2Memory mem;

    // Create a minimal system file: starts with 0x4C (JMP abs)
    fs::path sysFile = tempDir / "TESTSYS";
    {
      std::ofstream out(sysFile, std::ios::binary);
      out.put(0x4C);  // JMP abs
      out.put(0x00);  // target low
      out.put(0x20);  // target high (JMP $2000)
      out.put(0x60);  // RTS for testing
      for (int i = 0; i < 96; i++) {
        out.put(0xEA);  // NOPs
      }
    }

    try {
      prodos8emu::loadSystemFile(mem, sysFile, 0x2000);

      // Verify bytes were loaded at $2000
      auto& banks = mem.banks();
      if (prodos8emu::read_u8(banks, 0x2000) != 0x4C) {
        std::cerr << "FAIL: Expected 0x4C at $2000\n";
        failures++;
      } else if (prodos8emu::read_u8(banks, 0x2001) != 0x00) {
        std::cerr << "FAIL: Expected 0x00 at $2001\n";
        failures++;
      } else if (prodos8emu::read_u8(banks, 0x2002) != 0x20) {
        std::cerr << "FAIL: Expected 0x20 at $2002\n";
        failures++;
      } else if (prodos8emu::read_u8(banks, 0x2003) != 0x60) {
        std::cerr << "FAIL: Expected 0x60 at $2003\n";
        failures++;
      } else {
        std::cout << "PASS: System file loaded correctly\n";
      }
    } catch (const std::exception& e) {
      std::cerr << "FAIL: Unexpected exception: " << e.what() << "\n";
      failures++;
    }
  }

  // Test 13: System file loader - validation (missing JMP)
  {
    std::cout << "Test 13: System file loader - validation\n";
    prodos8emu::Apple2Memory mem;

    // Create a file that doesn't start with 0x4C
    fs::path badFile = tempDir / "BADSYS";
    {
      std::ofstream out(badFile, std::ios::binary);
      out.put(0x60);  // RTS instead of JMP
      for (int i = 0; i < 99; i++) {
        out.put(0x00);
      }
    }

    bool threw = false;
    try {
      prodos8emu::loadSystemFile(mem, badFile, 0x2000);
    } catch (const std::runtime_error& e) {
      threw = true;
      std::string msg(e.what());
      if (msg.find("0x4C") == std::string::npos && msg.find("JMP") == std::string::npos) {
        std::cerr << "FAIL: Exception message should mention 0x4C or JMP, got: " << msg << "\n";
        failures++;
      }
    } catch (...) {
      threw = true;
      std::cerr << "FAIL: Wrong exception type thrown\n";
      failures++;
    }

    if (!threw) {
      std::cerr << "FAIL: Expected exception for invalid system file\n";
      failures++;
    } else {
      std::cout << "PASS: Invalid system file rejected\n";
    }
  }

  // Test 14: System file loader - file too large
  {
    std::cout << "Test 14: System file loader - file too large\n";
    prodos8emu::Apple2Memory mem;

    // Create a file that's too large to fit from $2000 to $BFFF
    // Max size should be $C000 - $2000 = 40960 bytes
    fs::path tooBigFile = tempDir / "BIGSYS";
    {
      std::ofstream out(tooBigFile, std::ios::binary);
      out.put(0x4C);  // Valid JMP
      out.put(0x00);
      out.put(0x20);
      // Write 40960 more bytes (exceeds $BFFF)
      for (int i = 0; i < 40960; i++) {
        out.put(0xEA);
      }
    }

    bool threw = false;
    try {
      prodos8emu::loadSystemFile(mem, tooBigFile, 0x2000);
    } catch (const std::runtime_error& e) {
      threw = true;
      std::string msg(e.what());
      if (msg.find("large") == std::string::npos && msg.find("size") == std::string::npos) {
        std::cerr << "FAIL: Exception message should mention file size, got: " << msg << "\n";
        failures++;
      }
    } catch (...) {
      threw = true;
      std::cerr << "FAIL: Wrong exception type thrown\n";
      failures++;
    }

    if (!threw) {
      std::cerr << "FAIL: Expected exception for oversized file\n";
      failures++;
    } else {
      std::cout << "PASS: Oversized file rejected\n";
    }
  }

  // Test 15: System file loader - invalid load address
  {
    std::cout << "Test 15: System file loader - invalid load address\n";
    prodos8emu::Apple2Memory mem;

    // Create a valid system file
    fs::path sysFile = tempDir / "VALIDSYS";
    {
      std::ofstream out(sysFile, std::ios::binary);
      out.put(0x4C);  // Valid JMP
      out.put(0x00);
      out.put(0x20);
      for (int i = 0; i < 100; i++) {
        out.put(0xEA);
      }
    }

    // Try to load at 0xC000 (I/O space)
    bool threw = false;
    try {
      prodos8emu::loadSystemFile(mem, sysFile, 0xC000);
    } catch (const std::runtime_error& e) {
      threw = true;
      std::string msg(e.what());
      if (msg.find("0xC000") == std::string::npos && msg.find("address") == std::string::npos) {
        std::cerr << "FAIL: Exception message should mention address/0xC000, got: " << msg << "\n";
        failures++;
      }
    } catch (...) {
      threw = true;
      std::cerr << "FAIL: Wrong exception type thrown\n";
      failures++;
    }

    if (!threw) {
      std::cerr << "FAIL: Expected exception for invalid load address\n";
      failures++;
    }

    // Try to load at 0xF000 (also invalid)
    threw = false;
    try {
      prodos8emu::loadSystemFile(mem, sysFile, 0xF000);
    } catch (const std::runtime_error& e) {
      threw = true;
    } catch (...) {
      threw = true;
    }

    if (!threw) {
      std::cerr << "FAIL: Expected exception for invalid load address 0xF000\n";
      failures++;
    } else {
      std::cout << "PASS: Invalid load addresses rejected\n";
    }
  }

  // Test 16: Warm restart vector initialization
  {
    std::cout << "Test 16: Warm restart vector initialization\n";
    prodos8emu::Apple2Memory mem;

    // Initialize restart vector with entry address 0x2000
    uint16_t entryAddr = 0x2000;
    prodos8emu::initWarmStartVector(mem, entryAddr);

    // Verify $03F2/$03F3 contain the entry address (little-endian)
    auto&    banks      = mem.banks();
    uint16_t vectorAddr = prodos8emu::read_u16_le(banks, 0x03F2);
    if (vectorAddr != entryAddr) {
      std::cerr << "FAIL: Expected restart vector at $03F2 to be 0x" << std::hex << entryAddr
                << ", got 0x" << vectorAddr << "\n";
      failures++;
    }

    // Verify $03F4 contains power-up byte 0xA5
    uint8_t powerUpByte = prodos8emu::read_u8(banks, 0x03F4);
    if (powerUpByte != 0xA5) {
      std::cerr << "FAIL: Expected power-up byte at $03F4 to be 0xA5, got 0x" << std::hex
                << (int)powerUpByte << "\n";
      failures++;
    }

    if (vectorAddr == entryAddr && powerUpByte == 0xA5) {
      std::cout << "PASS: Warm restart vector initialized correctly\n";
    }

    // Test with different entry address
    entryAddr = 0x1000;
    prodos8emu::initWarmStartVector(mem, entryAddr);
    vectorAddr = prodos8emu::read_u16_le(banks, 0x03F2);
    if (vectorAddr != entryAddr) {
      std::cerr << "FAIL: Expected restart vector to be 0x" << std::hex << entryAddr << ", got 0x"
                << vectorAddr << " (second test)\n";
      failures++;
    } else {
      std::cout << "PASS: Restart vector can be reinitialized\n";
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
