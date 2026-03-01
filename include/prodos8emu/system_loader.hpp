#pragma once

#include <cstdint>
#include <filesystem>

#include "apple2mem.hpp"

namespace prodos8emu {

  /**
   * Load a ProDOS system file (type $FF) into Apple II memory.
   *
   * Reads file bytes from the host filesystem and writes them to memory
   * starting at the specified load address.
   *
   * Note: ProDOS system files do NOT need to start with 0x4C (JMP). ProDOS
   * unconditionally jumps to the load address after loading. The 0x4C check
   * is only used by some selector programs to detect if an interpreter
   * supports the startup-program-passing protocol.
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
   */
  void loadSystemFile(Apple2Memory& mem, const std::filesystem::path& filePath,
                      uint16_t loadAddr = 0x2000);

  /**
   * Initialize the Apple II Control-Reset warm start vector.
   *
   * Sets up the warm restart vector used by ProDOS system programs at
   * Control-Reset. Writes the entry address to $03F2/$03F3 (little-endian)
   * and sets the power-up byte at $03F4 to $A5 to mark the vector as valid.
   *
   * From ProDOS 8 Technical Reference, system programs should initialize
   * this vector on startup and can invalidate it on quit by modifying
   * the power-up byte.
   *
   * @param mem Apple2Memory instance to initialize.
   * @param entryAddr Entry point address to jump to on warm restart.
   */
  void initWarmStartVector(Apple2Memory& mem, uint16_t entryAddr);

  /**
   * Initialize the system program name at $280.
   *
   * Writes the system program pathname at $280 as a counted string (length byte
   * followed by pathname characters). The pathname is constructed as a full ProDOS
   * path including volume name, relative to the volume root.
   *
   * From ProDOS 8 Technical Reference, Section 5.1.2:
   * "The complete or partial pathname of the system program is stored at $280,
   * starting with a length byte. The string is a full pathname if it starts with
   * a slash."
   *
   * Example:
   *   systemFilePath: /path/to/volumes/EDASM/EDASM.SYSTEM
   *   volumeRoot:     /path/to/volumes
   *   Result at $280: counted string "/EDASM/EDASM.SYSTEM"
   *
   * @param mem Apple2Memory instance to initialize.
   * @param systemFilePath Full filesystem path to the system file.
   * @param volumeRoot Base directory containing ProDOS volumes.
   *
   * @throws std::runtime_error if:
   *         - systemFilePath is not within volumeRoot
   *         - Resulting ProDOS path exceeds 64 bytes
   */
  void initSystemProgramName(Apple2Memory&                mem,
                             const std::filesystem::path& systemFilePath,
                             const std::filesystem::path& volumeRoot);

}  // namespace prodos8emu
