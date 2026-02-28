#include "prodos8emu/system_loader.hpp"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "prodos8emu/memory.hpp"

namespace prodos8emu {

  void loadSystemFile(Apple2Memory& mem, const std::filesystem::path& filePath, uint16_t loadAddr) {
    // Validate load address is in safe range (below I/O space at $C000)
    if (loadAddr >= 0xC000) {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
                    "Invalid load address: 0x%04X. Must be < 0xC000 to avoid I/O space.", loadAddr);
      throw std::runtime_error(buf);
    }

    // Open the file
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open system file: " + filePath.string());
    }

    // Get file size
    std::streampos pos = file.tellg();
    if (pos < 0) {
      throw std::runtime_error("Failed to get file size: " + filePath.string());
    }
    std::streamsize fileSize = pos;
    file.seekg(0, std::ios::beg);

    // Check if file fits in memory from loadAddr to 0xBFFF
    // Maximum usable address is 0xBFFF, so max size is 0xC000 - loadAddr
    // (Safe from underflow: we validated loadAddr < 0xC000 above)
    const uint32_t maxSize = 0xC000 - loadAddr;
    if (static_cast<uint32_t>(fileSize) > maxSize) {
      char buf[256];
      std::snprintf(
          buf, sizeof(buf),
          "System file too large: %zu bytes exceeds maximum of %u bytes for load address 0x%04X",
          static_cast<size_t>(fileSize), maxSize, loadAddr);
      throw std::runtime_error(buf);
    }

    // Read file into buffer
    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
      throw std::runtime_error("Failed to read system file: " + filePath.string());
    }

    // Validate file is not empty
    if (buffer.empty()) {
      throw std::runtime_error("System file is empty: " + filePath.string());
    }

    // Write bytes to memory
    // (Safe from wraparound: we validated file size fits from loadAddr to 0xBFFF)
    auto& banks = mem.banks();
    for (size_t i = 0; i < buffer.size(); i++) {
      write_u8(banks, static_cast<uint16_t>(loadAddr + i), buffer[i]);
    }
  }

  void initWarmStartVector(Apple2Memory& mem, uint16_t entryAddr) {
    auto& banks = mem.banks();

    // Write entry address to $03F2/$03F3 (little-endian)
    write_u16_le(banks, 0x03F2, entryAddr);

    // Set power-up byte at $03F4 to $A5 (valid marker)
    write_u8(banks, 0x03F4, 0xA5);
  }

}  // namespace prodos8emu
