#pragma once

#include <cstdint>
#include <filesystem>

#include "apple2mem.hpp"

namespace prodos8emu {

  /**
   * Load a ProDOS system file (type $FF) into Apple II memory.
   *
   * Reads file bytes from the host filesystem and writes them to memory
   * starting at the specified load address. Validates that the first byte
   * is 0x4C (JMP abs instruction) as required for ProDOS system files.
   *
   * @param mem Apple2Memory instance to load the file into.
   * @param filePath Path to the system file on the host filesystem.
   * @param loadAddr Base address to load the file at (default: 0x2000).
   *                 Must be < 0xC000 to avoid I/O address space.
   *
   * @throws std::runtime_error if:
   *         - loadAddr >= 0xC000 (would overlap I/O space)
   *         - File cannot be opened
   *         - File read fails
   *         - File is too large to fit from loadAddr to 0xBFFF
   *         - First byte is not 0x4C (JMP abs)
   */
  void loadSystemFile(Apple2Memory& mem, const std::filesystem::path& filePath,
                      uint16_t loadAddr = 0x2000);

}  // namespace prodos8emu
