#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "memory.hpp"

namespace prodos8emu {

  /**
   * MLIContext - Main context for ProDOS 8 MLI emulation.
   *
   * This class manages the emulator state including:
   * - Open file table (ref_num management)
   * - I/O buffer pointers
   * - Current prefix
   * - Memory bank access
   * - Volume root mapping
   *
   * Future phases will add MLI call methods like:
   * - uint8_t create(const uint8_t* memoryBanks, uint16_t paramBlockOffset);
   * - uint8_t open(const uint8_t* memoryBanks, uint16_t paramBlockOffset);
   * etc.
   */
  class MLIContext {
   public:
    /**
     * Default constructor - uses current directory as volumes root.
     */
    MLIContext();

    /**
     * Constructor with volumes root path.
     *
     * @param volumesRoot Root directory where ProDOS volumes are mapped
     */
    explicit MLIContext(const std::filesystem::path& volumesRoot);

    ~MLIContext();

    // Smoke test method to verify instantiation
    bool isInitialized() const;

    /**
     * MLI Call: SET_PREFIX ($C6)
     *
     * Set the current pathname prefix.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 1
     *   +1: pathname pointer (2 bytes, little-endian) - pointer to counted string
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t setPrefixCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: GET_PREFIX ($C7)
     *
     * Get the current pathname prefix.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 1
     *   +1: data_buffer pointer (2 bytes, little-endian) - where to write counted string
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t getPrefixCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * Get the current prefix (for testing).
     *
     * @return Current prefix string
     */
    const std::string& getPrefix() const {
      return m_prefix;
    }

    /**
     * Get the volumes root path (for testing).
     *
     * @return Volumes root path
     */
    const std::filesystem::path& getVolumesRoot() const {
      return m_volumesRoot;
    }

    /**
     * MLI Call: CREATE ($C0)
     *
     * Create a new file or directory.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 7
     *   +1: pathname pointer (2 bytes, little-endian)
     *   +3: access (1 byte)
     *   +4: file_type (1 byte)
     *   +5: aux_type (2 bytes, little-endian)
     *   +7: storage_type (1 byte) - 0x01 standard file, 0x0D directory
     *   +8: create_date (2 bytes, little-endian)
     *   +10: create_time (2 bytes, little-endian)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t createCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: DESTROY ($C1)
     *
     * Delete a file or empty directory.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 1
     *   +1: pathname pointer (2 bytes, little-endian)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t destroyCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: RENAME ($C2)
     *
     * Rename a file or directory (must be in same directory).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: pathname pointer (2 bytes, little-endian)
     *   +3: new_pathname pointer (2 bytes, little-endian)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t renameCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: SET_FILE_INFO ($C3)
     *
     * Set file information (attributes, type, dates).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 7
     *   +1: pathname pointer (2 bytes, little-endian)
     *   +3: access (1 byte)
     *   +4: file_type (1 byte)
     *   +5: aux_type (2 bytes, little-endian)
     *   +7: null_field (3 bytes) - ignored
     *   +10: mod_date (2 bytes, little-endian)
     *   +12: mod_time (2 bytes, little-endian)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t setFileInfoCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: GET_FILE_INFO ($C4)
     *
     * Get file information (attributes, type, dates, size).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 0x0A
     *   +1: pathname pointer (2 bytes, little-endian)
     *   +3: access (1 byte) - result
     *   +4: file_type (1 byte) - result
     *   +5: aux_type (2 bytes, little-endian) - result
     *   +7: storage_type (1 byte) - result
     *   +8: blocks_used (2 bytes, little-endian) - result
     *   +10: mod_date (2 bytes, little-endian) - result
     *   +12: mod_time (2 bytes, little-endian) - result
     *   +14: create_date (2 bytes, little-endian) - result
     *   +16: create_time (2 bytes, little-endian) - result
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t getFileInfoCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: ON_LINE ($C5)
     *
     * Get list of online volumes.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: unit_num (1 byte) - 0 for all volumes
     *   +2: data_buffer pointer (2 bytes, little-endian)
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t onLineCall(MemoryBanks& banks, uint16_t paramBlockAddr);

   private:
    bool                  m_initialized;
    std::string           m_prefix;
    std::filesystem::path m_volumesRoot;
  };

  /**
   * Get library version string.
   */
  std::string getVersion();

}  // namespace prodos8emu
