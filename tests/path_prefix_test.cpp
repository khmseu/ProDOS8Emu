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

  // Test 1: SET_PREFIX with full pathname (starts with /)
  {
    std::cout << "Test 1: SET_PREFIX with full pathname\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    // Write parameter block: param_count=1 at offset 0x0300, pathname pointer at offset 0x0301
    mem.writeCountedString(0x0400, "/TESTVOLUME/MYDIR");
    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer

    uint8_t err = ctx.setPrefixCall(mem.constBanks(), 0x0300);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: SET_PREFIX returned error " << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::string prefix = ctx.getPrefix();
      if (prefix != "/TESTVOLUME/MYDIR") {
        std::cerr << "FAIL: Expected prefix '/TESTVOLUME/MYDIR', got '" << prefix << "'\n";
        failures++;
      } else {
        std::cout << "PASS: SET_PREFIX with full pathname\n";
      }
    }
  }

  // Test 2: GET_PREFIX writes counted string to buffer
  {
    std::cout << "Test 2: GET_PREFIX writes counted string\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    // Set prefix first
    mem.writeCountedString(0x0400, "/VOL1/DIR1");
    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer
    ctx.setPrefixCall(mem.constBanks(), 0x0300);

    // GET_PREFIX with data_buffer at 0x0500
    prodos8emu::write_u8(mem.banks(), 0x0310, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0311, 0x0500);  // data_buffer pointer

    uint8_t err = ctx.getPrefixCall(mem.banks(), 0x0310);
    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: GET_PREFIX returned error " << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::string result = mem.readCountedString(0x0500);
      if (result != "/VOL1/DIR1") {
        std::cerr << "FAIL: Expected '/VOL1/DIR1', got '" << result << "'\n";
        failures++;
      } else {
        std::cout << "PASS: GET_PREFIX writes counted string\n";
      }
    }
  }

  // Test 3: SET_PREFIX with partial pathname uses existing prefix
  {
    std::cout << "Test 3: SET_PREFIX with partial pathname\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    // Set initial prefix
    mem.writeCountedString(0x0400, "/MYVOL");
    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer
    ctx.setPrefixCall(mem.constBanks(), 0x0300);

    // Set partial path
    mem.writeCountedString(0x0400, "SUBDIR");
    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer
    uint8_t err = ctx.setPrefixCall(mem.constBanks(), 0x0300);

    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: SET_PREFIX with partial path returned error " << std::hex << (int)err
                << "\n";
      failures++;
    } else {
      std::string prefix = ctx.getPrefix();
      if (prefix != "/MYVOL/SUBDIR") {
        std::cerr << "FAIL: Expected '/MYVOL/SUBDIR', got '" << prefix << "'\n";
        failures++;
      } else {
        std::cout << "PASS: SET_PREFIX with partial pathname\n";
      }
    }
  }

  // Test 4: Invalid pathname syntax returns ERR_INVALID_PATH_SYNTAX
  {
    std::cout << "Test 4: Invalid pathname syntax\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    // Invalid: contains invalid character '*'
    mem.writeCountedString(0x0400, "/VOL/BAD*NAME");
    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer

    uint8_t err = ctx.setPrefixCall(mem.constBanks(), 0x0300);
    if (err != prodos8emu::ERR_INVALID_PATH_SYNTAX) {
      std::cerr << "FAIL: Expected ERR_INVALID_PATH_SYNTAX (0x40), got " << std::hex << (int)err
                << "\n";
      failures++;
    } else {
      std::cout << "PASS: Invalid pathname returns ERR_INVALID_PATH_SYNTAX\n";
    }
  }

  // Test 5: Pathname too long returns ERR_INVALID_PATH_SYNTAX
  {
    std::cout << "Test 5: Pathname too long\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    // Create a pathname > 64 bytes
    std::string longPath = "/" + std::string(70, 'A');
    mem.writeCountedString(0x0400, longPath);
    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer

    uint8_t err = ctx.setPrefixCall(mem.constBanks(), 0x0300);
    if (err != prodos8emu::ERR_INVALID_PATH_SYNTAX) {
      std::cerr << "FAIL: Expected ERR_INVALID_PATH_SYNTAX for long path, got " << std::hex
                << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: Pathname too long returns ERR_INVALID_PATH_SYNTAX\n";
    }
  }

  // Test 6: Component starts with digit returns ERR_INVALID_PATH_SYNTAX
  {
    std::cout << "Test 6: Component starts with digit\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    mem.writeCountedString(0x0400, "/VOL/9BADNAME");
    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer

    uint8_t err = ctx.setPrefixCall(mem.constBanks(), 0x0300);
    if (err != prodos8emu::ERR_INVALID_PATH_SYNTAX) {
      std::cerr << "FAIL: Expected ERR_INVALID_PATH_SYNTAX for digit start, got " << std::hex
                << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: Component starts with digit returns ERR_INVALID_PATH_SYNTAX\n";
    }
  }

  // Test 7: xattr roundtrip (or returns ERR_IO_ERROR if unsupported)
  {
    std::cout << "Test 7: xattr roundtrip\n";

    // Create a temporary file
    fs::path tempFile = fs::temp_directory_path() / "prodos8emu_xattr_test.tmp";
    std::ofstream(tempFile) << "test";

    uint8_t err = prodos8emu::prodos8_set_xattr(tempFile.string(), "test.attr", "test_value");
    if (err == prodos8emu::ERR_IO_ERROR) {
      std::cout
          << "PASS: xattr operations return ERR_IO_ERROR (filesystem doesn't support xattrs)\n";
    } else if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: prodos8_set_xattr returned unexpected error " << std::hex << (int)err
                << "\n";
      failures++;
    } else {
      // Try to read it back
      std::string value;
      err = prodos8emu::prodos8_get_xattr(tempFile.string(), "test.attr", value);
      if (err != prodos8emu::ERR_NO_ERROR) {
        std::cerr << "FAIL: prodos8_get_xattr returned error " << std::hex << (int)err << "\n";
        failures++;
      } else if (value != "test_value") {
        std::cerr << "FAIL: Expected 'test_value', got '" << value << "'\n";
        failures++;
      } else {
        std::cout << "PASS: xattr roundtrip successful\n";
      }
    }

    fs::remove(tempFile);
  }

  // Test 8: High-bit stripping and case normalization
  {
    std::cout << "Test 8: High-bit stripping and case normalization\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    // Manually write pathname with high bits set and lowercase
    // String should be "/vol/dir" with high bits on 'v', 'o', 'l', 'd', 'i', 'r'
    uint8_t count = 8;
    prodos8emu::write_u8(mem.banks(), 0x0400, count);
    prodos8emu::write_u8(mem.banks(), 0x0401, '/');
    prodos8emu::write_u8(mem.banks(), 0x0402, 0x80 | 'v');  // v with high bit
    prodos8emu::write_u8(mem.banks(), 0x0403, 0x80 | 'o');  // o with high bit
    prodos8emu::write_u8(mem.banks(), 0x0404, 0x80 | 'l');  // l with high bit
    prodos8emu::write_u8(mem.banks(), 0x0405, '/');
    prodos8emu::write_u8(mem.banks(), 0x0406, 0x80 | 'd');  // d with high bit
    prodos8emu::write_u8(mem.banks(), 0x0407, 0x80 | 'i');  // i with high bit
    prodos8emu::write_u8(mem.banks(), 0x0408, 0x80 | 'r');  // r with high bit

    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer
    uint8_t err = ctx.setPrefixCall(mem.constBanks(), 0x0300);

    if (err != prodos8emu::ERR_NO_ERROR) {
      std::cerr << "FAIL: SET_PREFIX with high-bit returned error " << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::string prefix = ctx.getPrefix();
      // Should be normalized to uppercase with high bits stripped
      if (prefix != "/VOL/DIR") {
        std::cerr << "FAIL: Expected '/VOL/DIR', got '" << prefix << "'\n";
        failures++;
      } else {
        std::cout << "PASS: High-bit stripping and case normalization\n";
      }
    }
  }

  // Test 9: SET_PREFIX with param_count != 1 returns ERR_BAD_CALL_PARAM_COUNT
  {
    std::cout << "Test 9: SET_PREFIX with invalid param_count\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    mem.writeCountedString(0x0400, "/SOMEVOL");
    prodos8emu::write_u8(mem.banks(), 0x0300, 2);           // param_count = 2 (invalid!)
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer

    uint8_t err = ctx.setPrefixCall(mem.constBanks(), 0x0300);
    if (err != prodos8emu::ERR_BAD_CALL_PARAM_COUNT) {
      std::cerr << "FAIL: Expected ERR_BAD_CALL_PARAM_COUNT (0x04), got " << std::hex << (int)err
                << "\n";
      failures++;
    } else {
      std::cout << "PASS: SET_PREFIX with invalid param_count returns ERR_BAD_CALL_PARAM_COUNT\n";
    }
  }

  // Test 10: GET_PREFIX with param_count != 1 returns ERR_BAD_CALL_PARAM_COUNT
  {
    std::cout << "Test 10: GET_PREFIX with invalid param_count\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");

    prodos8emu::write_u8(mem.banks(), 0x0310, 0);           // param_count = 0 (invalid!)
    prodos8emu::write_u16_le(mem.banks(), 0x0311, 0x0500);  // data_buffer pointer

    uint8_t err = ctx.getPrefixCall(mem.banks(), 0x0310);
    if (err != prodos8emu::ERR_BAD_CALL_PARAM_COUNT) {
      std::cerr << "FAIL: Expected ERR_BAD_CALL_PARAM_COUNT (0x04), got " << std::hex << (int)err
                << "\n";
      failures++;
    } else {
      std::cout << "PASS: GET_PREFIX with invalid param_count returns ERR_BAD_CALL_PARAM_COUNT\n";
    }
  }

  // Test 11: Partial pathname with empty prefix returns ERR_INVALID_PATH_SYNTAX
  {
    std::cout << "Test 11: Partial pathname with empty prefix\n";
    TestMemory             mem;
    prodos8emu::MLIContext ctx("/tmp/prodos8emu_test_volumes");
    // Context starts with empty prefix

    mem.writeCountedString(0x0400, "PARTIALPATH");          // No leading '/'
    prodos8emu::write_u8(mem.banks(), 0x0300, 1);           // param_count = 1
    prodos8emu::write_u16_le(mem.banks(), 0x0301, 0x0400);  // pathname pointer

    uint8_t err = ctx.setPrefixCall(mem.constBanks(), 0x0300);
    if (err != prodos8emu::ERR_INVALID_PATH_SYNTAX) {
      std::cerr << "FAIL: Expected ERR_INVALID_PATH_SYNTAX for partial path with empty prefix, got "
                << std::hex << (int)err << "\n";
      failures++;
    } else {
      std::cout << "PASS: Partial pathname with empty prefix returns ERR_INVALID_PATH_SYNTAX\n";
    }
  }

  if (failures == 0) {
    std::cout << "\nAll tests passed!\n";
    return EXIT_SUCCESS;
  } else {
    std::cerr << "\n" << failures << " test(s) failed!\n";
    return EXIT_FAILURE;
  }
}
