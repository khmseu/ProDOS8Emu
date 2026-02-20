#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

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

    /**
     * MLI Call: OPEN ($C8)
     *
     * Open a file and allocate a reference number.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 3
     *   +1: pathname pointer (2 bytes, little-endian)
     *   +3: io_buffer pointer (2 bytes, little-endian)
     *   +5: ref_num (1 byte) - result
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t openCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: NEWLINE ($C9)
     *
     * Set newline mode for an open file.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 3
     *   +1: ref_num (1 byte)
     *   +2: enable_mask (1 byte) - 0x00 disables newline mode
     *   +3: newline_char (1 byte) - newline character (matched with mask)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t newlineCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: READ ($CA)
     *
     * Read data from an open file.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 4
     *   +1: ref_num (1 byte)
     *   +2: data_buffer pointer (2 bytes, little-endian)
     *   +4: request_count (2 bytes, little-endian)
     *   +6: trans_count (2 bytes, little-endian) - result
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t readCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: WRITE ($CB)
     *
     * Write data to an open file.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 4
     *   +1: ref_num (1 byte)
     *   +2: data_buffer pointer (2 bytes, little-endian)
     *   +4: request_count (2 bytes, little-endian)
     *   +6: trans_count (2 bytes, little-endian) - result
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t writeCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: CLOSE ($CC)
     *
     * Close an open file (or all open files if ref_num is 0).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 1
     *   +1: ref_num (1 byte) - 0 closes all open files
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t closeCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: FLUSH ($CD)
     *
     * Flush an open file (or all open files if ref_num is 0).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 1
     *   +1: ref_num (1 byte) - 0 flushes all open files
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t flushCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: SET_MARK ($CE)
     *
     * Set the current file position (mark).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: ref_num (1 byte)
     *   +2: position (3 bytes, little-endian 24-bit)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t setMarkCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: GET_MARK ($CF)
     *
     * Get the current file position (mark).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: ref_num (1 byte)
     *   +2: position (3 bytes, little-endian 24-bit) - result
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t getMarkCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: SET_EOF ($D0)
     *
     * Set the end-of-file marker (resizes the file).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: ref_num (1 byte)
     *   +2: eof (3 bytes, little-endian 24-bit)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t setEofCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: GET_EOF ($D1)
     *
     * Get the end-of-file marker (file size).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: ref_num (1 byte)
     *   +2: eof (3 bytes, little-endian 24-bit) - result
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t getEofCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: SET_BUF ($D2)
     *
     * Set the I/O buffer address for an open file.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: ref_num (1 byte)
     *   +2: io_buffer pointer (2 bytes, little-endian)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t setBufCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: GET_BUF ($D3)
     *
     * Get the I/O buffer address for an open file.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: ref_num (1 byte)
     *   +2: io_buffer pointer (2 bytes, little-endian) - result
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t getBufCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: GET_TIME ($82)
     *
     * Read the system clock and update the ProDOS global time locations
     * at $BF90 (date) and $BF92 (time) in emulated memory.
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 0
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t getTimeCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: ALLOC_INTERRUPT ($40)
     *
     * Allocate an interrupt handler slot (stub - validates parameters only).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 2
     *   +1: int_num (1 byte) - result: assigned interrupt number (1-4)
     *   +2: int_code pointer (2 bytes, little-endian) - pointer to handler routine
     *
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t allocInterruptCall(MemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: DEALLOC_INTERRUPT ($41)
     *
     * Free an interrupt handler slot (stub - validates parameters only).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 1
     *   +1: int_num (1 byte) - interrupt number to free (1-4)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t deallocInterruptCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: READ_BLOCK ($80)
     *
     * Block-level read (not supported - returns ERR_IO_ERROR until a disk
     * backend exists).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 3
     *   +1: unit_num (1 byte)
     *   +2: data_buffer pointer (2 bytes, little-endian)
     *   +4: block_num (2 bytes, little-endian)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t readBlockCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

    /**
     * MLI Call: WRITE_BLOCK ($81)
     *
     * Block-level write (not supported - returns ERR_IO_ERROR until a disk
     * backend exists).
     *
     * Parameter block:
     *   +0: param_count (1 byte) - must be 3
     *   +1: unit_num (1 byte)
     *   +2: data_buffer pointer (2 bytes, little-endian)
     *   +4: block_num (2 bytes, little-endian)
     *
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t writeBlockCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);

   private:
    struct OpenFile {
      int      fd;
      uint32_t mark;      // current file position (24-bit, max 0x00FFFFFF)
      uint16_t ioBuffer;  // io_buffer pointer in emulated memory
      bool     newlineEnabled;
      uint8_t  newlineMask;
      uint8_t  newlineChar;
    };

    bool                                  m_initialized;
    std::string                           m_prefix;
    std::filesystem::path                 m_volumesRoot;
    std::unordered_map<uint8_t, OpenFile> m_openFiles;
    uint16_t                              m_interruptHandlers[4] = {0, 0, 0, 0};
  };

  /**
   * Get library version string.
   */
  std::string getVersion();

}  // namespace prodos8emu
