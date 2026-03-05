#include "prodos8emu/cpu65c02.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>

#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"

namespace prodos8emu {

  namespace {

    constexpr uint16_t VEC_RESET       = 0xFFFC;
    constexpr uint16_t VEC_IRQ         = 0xFFFE;
    constexpr uint16_t COUT_VECTOR_PTR = 0x0036;

    inline uint16_t make_u16(uint8_t lo, uint8_t hi) {
      return static_cast<uint16_t>(static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8));
    }

    const char* mli_call_name(uint8_t callNumber) {
      switch (callNumber) {
        case 0xC0:
          return "CREATE";
        case 0xC1:
          return "DESTROY";
        case 0xC2:
          return "RENAME";
        case 0xC3:
          return "SET_FILE_INFO";
        case 0xC4:
          return "GET_FILE_INFO";
        case 0xC5:
          return "ON_LINE";
        case 0xC6:
          return "SET_PREFIX";
        case 0xC7:
          return "GET_PREFIX";
        case 0xC8:
          return "OPEN";
        case 0xC9:
          return "NEWLINE";
        case 0xCA:
          return "READ";
        case 0xCB:
          return "WRITE";
        case 0xCC:
          return "CLOSE";
        case 0xCD:
          return "FLUSH";
        case 0xCE:
          return "SET_MARK";
        case 0xCF:
          return "GET_MARK";
        case 0xD0:
          return "SET_EOF";
        case 0xD1:
          return "GET_EOF";
        case 0xD2:
          return "SET_BUF";
        case 0xD3:
          return "GET_BUF";
        case 0x65:
          return "QUIT";
        case 0x40:
          return "ALLOC_INTERRUPT";
        case 0x41:
          return "DEALLOC_INTERRUPT";
        case 0x80:
          return "READ_BLOCK";
        case 0x81:
          return "WRITE_BLOCK";
        case 0x82:
          return "GET_TIME";
        default:
          return "UNKNOWN";
      }
    }

    void write_hex(std::ostream& os, uint32_t value, int width) {
      auto flags = os.flags();
      auto fill  = os.fill();
      os << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << value;
      os.flags(flags);
      os.fill(fill);
    }

    void dump_stack(std::ostream& os, const ConstMemoryBanks& banks, uint8_t sp) {
      // 6502 stack is at $0100-$01FF, SP points to next available location
      // Stack grows downward, so used portion is from $0100+SP+1 to $01FF
      uint16_t stackTop = 0x01FF;
      uint16_t stackPtr = 0x0100 + sp;

      os << "\nStack dump (SP=$";
      write_hex(os, sp, 2);
      os << ", used bytes: " << (stackTop - stackPtr) << "):\n";

      if (stackPtr >= stackTop) {
        os << "  (stack empty)\n";
        return;
      }

      // Dump in rows of 16 bytes
      for (uint16_t addr = stackPtr + 1; addr <= stackTop; addr++) {
        if ((addr - 0x0100) % 16 == 0 || addr == stackPtr + 1) {
          if (addr != stackPtr + 1) {
            os << "\n";
          }
          os << "  $";
          write_hex(os, addr, 4);
          os << ":";
        }
        os << " ";
        uint8_t byte = read_u8(banks, addr);
        write_hex(os, byte, 2);
      }
      os << "\n";
    }

    void dump_pc_ring(std::ostream& os, const uint16_t* pcRingFrom, const uint16_t* pcRingTo,
                      const uint32_t* pcRingCount, size_t ringSize, size_t ringIndex) {
      os << "\nPC ring buffer (last " << ringSize << " explicit PC changes, newest first):\n";

      // ringIndex points to the next slot to write, so ringIndex-1 is the most recent
      // We walk backwards through the ring buffer
      size_t count = 0;
      for (size_t i = 0; i < ringSize; i++) {
        // Calculate the index walking backwards from most recent
        size_t   idx     = (ringIndex + ringSize - 1 - i) % ringSize;
        uint16_t fromPC  = pcRingFrom[idx];
        uint16_t toPC    = pcRingTo[idx];
        uint32_t pcCount = pcRingCount[idx];

        // Stop if we hit an uninitialized entry (from=0, to=0 before buffer fills up)
        if (fromPC == 0 && toPC == 0 && i >= ringIndex) {
          break;
        }

        if (count % 4 == 0) {
          if (count > 0) {
            os << "\n";
          }
          os << "  ";
        } else {
          os << " ";
        }
        os << "$";
        write_hex(os, fromPC, 4);
        os << "->$";
        write_hex(os, toPC, 4);
        if (pcCount > 1) {
          os << "x" << pcCount;
        }
        count++;
      }
      if (count > 0) {
        os << "\n";
      } else {
        os << "  (empty)\n";
      }
    }

    const char* error_name(uint8_t errorCode) {
      switch (errorCode) {
        case ERR_NO_ERROR:
          return "";
        case ERR_BAD_CALL_NUMBER:
          return "BAD_CALL_NUMBER";
        case ERR_BAD_CALL_PARAM_COUNT:
          return "BAD_CALL_PARAM_COUNT";
        case ERR_INTERRUPT_TABLE_FULL:
          return "INTERRUPT_TABLE_FULL";
        case ERR_IO_ERROR:
          return "IO_ERROR";
        case ERR_NO_DEVICE:
          return "NO_DEVICE";
        case ERR_WRITE_PROTECTED:
          return "WRITE_PROTECTED";
        case ERR_DISK_SWITCHED:
          return "DISK_SWITCHED";
        case ERR_INVALID_PATH_SYNTAX:
          return "INVALID_PATH_SYNTAX";
        case ERR_TOO_MANY_FILES_OPEN:
          return "TOO_MANY_FILES_OPEN";
        case ERR_BAD_REF_NUM:
          return "BAD_REF_NUM";
        case ERR_PATH_NOT_FOUND:
          return "PATH_NOT_FOUND";
        case ERR_VOL_NOT_FOUND:
          return "VOL_NOT_FOUND";
        case ERR_FILE_NOT_FOUND:
          return "FILE_NOT_FOUND";
        case ERR_DUPLICATE_FILENAME:
          return "DUPLICATE_FILENAME";
        case ERR_VOLUME_FULL:
          return "VOLUME_FULL";
        case ERR_VOL_DIR_FULL:
          return "VOL_DIR_FULL";
        case ERR_INCOMPATIBLE_VERSION:
          return "INCOMPATIBLE_VERSION";
        case ERR_UNSUPPORTED_STOR_TYPE:
          return "UNSUPPORTED_STOR_TYPE";
        case ERR_EOF_ENCOUNTERED:
          return "EOF_ENCOUNTERED";
        case ERR_POSITION_OUT_OF_RANGE:
          return "POSITION_OUT_OF_RANGE";
        case ERR_ACCESS_ERROR:
          return "ACCESS_ERROR";
        case ERR_FILE_OPEN:
          return "FILE_OPEN";
        case ERR_DIR_COUNT_ERROR:
          return "DIR_COUNT_ERROR";
        case ERR_NOT_PRODOS_VOL:
          return "NOT_PRODOS_VOL";
        case ERR_INVALID_PARAMETER:
          return "INVALID_PARAMETER";
        case ERR_VCB_TABLE_FULL:
          return "VCB_TABLE_FULL";
        case ERR_BAD_BUFFER_ADDR:
          return "BAD_BUFFER_ADDR";
        case ERR_DUPLICATE_VOLUME:
          return "DUPLICATE_VOLUME";
        case ERR_FILE_STRUCTURE_DAMAGED:
          return "FILE_STRUCTURE_DAMAGED";
        default:
          return "";
      }
    }

    /**
     * Read a counted string from memory (ProDOS pathname format).
     * First byte is length, followed by that many characters.
     * Returns empty string if length > maxLen or length == 0.
     * If outLength is provided, sets it to the actual length byte read.
     */
    std::string read_pathname(const ConstMemoryBanks& banks, uint16_t pathnamePtr,
                              uint8_t maxLen = 64, uint8_t* outLength = nullptr) {
      uint8_t length = read_u8(banks, pathnamePtr);
      if (outLength != nullptr) {
        *outLength = length;
      }

      if (length == 0 || length > maxLen) {
        return "";
      }

      std::string result;
      result.reserve(length);
      for (uint8_t i = 0; i < length; i++) {
        uint8_t ch = read_u8(banks, static_cast<uint16_t>(pathnamePtr + 1 + i));
        // ProDOS pathnames: high bit clear, uppercase ASCII
        result.push_back(static_cast<char>(ch & 0x7F));
      }
      return result;
    }

    std::string format_counted_path_for_log(const char* fieldName, const std::string& pathname,
                                            uint8_t length, uint8_t maxLen = 64) {
      if (!pathname.empty()) {
        return " " + std::string(fieldName) + "='" + pathname + "'";
      }
      if (length == 0) {
        return " " + std::string(fieldName) + "=<empty>";
      }
      if (length > maxLen) {
        return " " + std::string(fieldName) + "=<invalid:len=" + std::to_string(length) + ">";
      }
      return "";
    }

    std::string read_and_format_counted_path_for_log(const ConstMemoryBanks& banks,
                                                     uint16_t pathnamePtr, const char* fieldName,
                                                     uint8_t maxLen = 64) {
      uint8_t     length   = 0;
      std::string pathname = read_pathname(banks, pathnamePtr, maxLen, &length);
      return format_counted_path_for_log(fieldName, pathname, length, maxLen);
    }

    /**
     * Extract pathname(s) from MLI parameter block for logging.
     * Returns formatted string with pathname info, or empty if call doesn't use pathnames.
     */
    std::string extract_mli_pathnames(const ConstMemoryBanks& banks, MLIContext* mli,
                                      uint8_t callNumber, uint16_t paramBlockAddr, uint8_t err) {
      std::string result;

      switch (callNumber) {
        // Single pathname at offset +1
        case 0xC0:  // CREATE
        case 0xC1:  // DESTROY
        case 0xC3:  // SET_FILE_INFO
        case 0xC4:  // GET_FILE_INFO
        case 0xC8:  // OPEN
        {
          uint16_t pathnamePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result               = read_and_format_counted_path_for_log(banks, pathnamePtr, "path");

          // OPEN: log output refnum on success
          if (callNumber == 0xC8 && err == 0) {
            uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 5));
            result += " ref=" + std::to_string(refNum);
          }
          break;
        }

        // RENAME: old pathname at +1, new pathname at +3
        case 0xC2:  // RENAME
        {
          uint16_t    oldPtr  = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          uint16_t    newPtr  = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 3));
          std::string oldPath = read_pathname(banks, oldPtr);
          std::string newPath = read_pathname(banks, newPtr);
          if (!oldPath.empty() && !newPath.empty()) {
            result = " old='" + oldPath + "' new='" + newPath + "'";
          }
          break;
        }

        // SET_PREFIX: pathname at +1
        case 0xC6:  // SET_PREFIX
        {
          uint16_t pathnamePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result               = read_and_format_counted_path_for_log(banks, pathnamePtr, "prefix");
          break;
        }

        // GET_PREFIX: data_buffer at +1 (pathname returned there, read after call)
        case 0xC7:  // GET_PREFIX
        {
          // Only read buffer if call succeeded
          if (err == 0) {
            uint16_t dataBufferPtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
            result = read_and_format_counted_path_for_log(banks, dataBufferPtr, "prefix");
          }
          break;
        }

        // ON_LINE: data_buffer at +2 (volume names returned there, read after call)
        case 0xC5:  // ON_LINE
        {
          // Only read buffer if call succeeded
          if (err != 0) {
            break;  // Don't parse garbage data on error
          }

          uint8_t  unitNum       = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          uint16_t dataBufferPtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));

          if (unitNum == 0) {
            // Multiple volumes: read all entries until terminator (byte0 == 0)
            result           = " volumes='";
            bool     first   = true;
            uint16_t offset  = dataBufferPtr;
            int      maxRecs = 15;  // Safety limit (14 volumes + terminator)

            for (int i = 0; i < maxRecs; i++) {
              uint8_t byte0 = read_u8(banks, offset);
              if (byte0 == 0) break;  // Terminator

              uint8_t length = byte0 & 0x0F;
              uint8_t slot   = (byte0 >> 4) & 0x07;
              uint8_t drive  = (byte0 >> 7) & 0x01;

              if (length > 0 && length <= 15) {
                if (!first) result += ", ";
                first = false;

                // Read volume name
                std::string volName;
                volName.reserve(length);
                for (uint8_t j = 0; j < length; j++) {
                  uint8_t ch = read_u8(banks, static_cast<uint16_t>(offset + 1 + j));
                  volName.push_back(static_cast<char>(ch & 0x7F));
                }

                result += volName + "[S" + std::to_string(slot) + "D" + std::to_string(drive) + "]";
              }

              offset += 16;
            }
            result += "'";
          } else {
            // Single volume: read first entry
            uint8_t byte0  = read_u8(banks, dataBufferPtr);
            uint8_t length = byte0 & 0x0F;
            uint8_t slot   = (byte0 >> 4) & 0x07;
            uint8_t drive  = (byte0 >> 7) & 0x01;

            if (length > 0 && length <= 15) {
              std::string volName;
              volName.reserve(length);
              for (uint8_t i = 0; i < length; i++) {
                uint8_t ch = read_u8(banks, static_cast<uint16_t>(dataBufferPtr + 1 + i));
                volName.push_back(static_cast<char>(ch & 0x7F));
              }

              result = " volume='" + volName + "[S" + std::to_string(slot) + "D" +
                       std::to_string(drive) + "]'";
            } else if (length == 0) {
              result = " volume=<none>";
            }
          }
          break;
        }

        // READ: data_buffer at +2, might be directory entries
        case 0xCA:  // READ
        {
          // Always log refnum
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);

          uint16_t requestCount = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 4));
          uint16_t transCount   = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6));
          result += " req=" + std::to_string(requestCount);
          result += " trans=" + std::to_string(transCount);

          if (mli != nullptr) {
            uint32_t markAfter  = mli->getMarkForRefNum(refNum);
            uint32_t markBefore = (markAfter >= transCount) ? (markAfter - transCount) : 0;
            uint32_t eof        = mli->getEofForRefNum(refNum);
            result += " mark=$" + std::to_string(markBefore);
            result += " eof=$" + std::to_string(eof);
          }

          // Check if this is a directory read using the MLI context
          bool isDirectoryRead = (mli != nullptr && err == 0 && mli->isDirectoryRefNum(refNum));

          // Only parse directory entries if this is a directory read
          if (!isDirectoryRead) {
            break;
          }

          uint16_t dataBufferPtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));
          uint16_t dirTransCount = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6));

          // Get the MARK (file position) after the READ completes
          // Subtract transCount to get the starting position of this read
          uint32_t markAfter  = mli->getMarkForRefNum(refNum);
          uint32_t markBefore = markAfter - dirTransCount;

          // Calculate position within 512-byte block structure
          uint32_t blockNum    = markBefore / 512;
          uint32_t blockOffset = markBefore % 512;

          // Add MARK and block position to log
          result += " mark=$" + std::to_string(markBefore);
          result += " blk=" + std::to_string(blockNum) + "+" + std::to_string(blockOffset);

          // Skip if no data was read
          if (dirTransCount == 0) {
            break;
          }

          // Identify what's being read based on block position
          // ProDOS directory blocks: 512 bytes each
          // Bytes 0-3: prev/next block pointers
          // Block 0: bytes 4-42 = header entry, bytes 43-510 = up to 12 file entries
          // Block N: bytes 4-510 = up to 13 file entries (no header)
          // Byte 511: unused
          std::string posInfo;
          if (blockOffset <= 3) {
            posInfo = " [ptrs]";
          } else if (blockNum == 0 && blockOffset >= 4 && blockOffset <= 42) {
            posInfo = " [hdr]";
          } else if (blockOffset >= 4 && blockOffset <= 510) {
            // Calculate entry number within this block
            uint32_t entryOffset = blockOffset - 4;
            uint32_t entryNum    = entryOffset / 39;
            posInfo              = " [ent" + std::to_string(entryNum) + "]";
          } else if (blockOffset == 511) {
            posInfo = " [pad]";
          }
          result += posInfo;

          // Parse directory entries from the read buffer
          // EdAsm reads small chunks (39 or 43 bytes), not full 512-byte blocks
          // Each read may contain block pointers (first 4 bytes) + one or more entries
          std::string entries;
          bool        first        = true;
          int         validEntries = 0;

          // Try parsing entries (each entry is 39 bytes / 0x27)
          // Entries in a ProDOS directory block start at offset 4 and then every 39 bytes.
          // Use blockOffset (position of this buffer within the 512-byte block) to align.
          uint32_t blockStartAbs = static_cast<uint32_t>(blockOffset);
          uint32_t bufferEndAbs  = blockStartAbs + static_cast<uint32_t>(transCount);
          uint32_t firstEntryAbs = 0;

          if (bufferEndAbs > 4) {
            if (blockStartAbs <= 4) {
              // Buffer begins before or at the first possible entry; first entry at abs offset 4.
              firstEntryAbs = 4;
            } else {
              // Align the first possible entry at or after blockStartAbs to the 39-byte grid.
              uint32_t relToFirst = blockStartAbs - 4;  // relative to first entry
              uint32_t rem        = relToFirst % 39;
              uint32_t delta      = (rem == 0) ? 0u : (39u - rem);  // bytes to next alignment
              firstEntryAbs       = blockStartAbs + delta;
            }
          }

          // Convert absolute entry start back to buffer-relative offset, if it lies in this buffer
          if (firstEntryAbs >= blockStartAbs && firstEntryAbs + 0x27 <= bufferEndAbs) {
            for (uint16_t offset = static_cast<uint16_t>(firstEntryAbs - blockStartAbs);
                 offset + 0x27 <= transCount; offset = static_cast<uint16_t>(offset + 39)) {
              // Check if this looks like the start of an entry
              // Entry byte 0 = storage_type (high nibble) + name_length (low nibble)
              uint8_t byte0   = read_u8(banks, static_cast<uint16_t>(dataBufferPtr + offset));
              uint8_t nameLen = byte0 & 0x0F;
              // uint8_t storageType = (byte0 >> 4) & 0x0F;

              // Skip inactive entries where both storage_type and name_length are zero
              if (byte0 == 0) {
                continue;
              }

              // Extract the name and verify it looks valid
              std::string entryName;
              bool        allValid = true;
              entryName.reserve(nameLen);

              for (uint8_t i = 0; i < nameLen; i++) {
                if (offset + 1 + i >= transCount) {
                  allValid = false;
                  break;
                }
                uint8_t ch = read_u8(banks, static_cast<uint16_t>(dataBufferPtr + offset + 1 + i));
                ch         = ch & 0x7F;  // Clear high bit

                entryName.push_back(static_cast<char>(ch));
              }

              if (allValid) {
                validEntries++;
                if (first) {
                  entries = " entries='";
                  first   = false;
                } else {
                  entries += ", ";
                }
                entries += entryName;
              }
            }
          }

          // Show entries if we found any
          if (validEntries >= 1) {
            entries += "'";
            result += entries;
          }
          break;
        }

        // NEWLINE: log input refnum
        case 0xC9:  // NEWLINE
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // WRITE: log input refnum
        case 0xCB:  // WRITE
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // CLOSE: log input refnum
        case 0xCC:  // CLOSE
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // FLUSH: log input refnum
        case 0xCD:  // FLUSH
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // SET_MARK: log input refnum
        case 0xCE:  // SET_MARK
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // GET_MARK: log input refnum
        case 0xCF:  // GET_MARK
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // SET_EOF: log input refnum
        case 0xD0:  // SET_EOF
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // GET_EOF: log input refnum
        case 0xD1:  // GET_EOF
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // Calls that don't involve pathnames or where we don't log them
        default:
          break;
      }

      return result;
    }

  }  // namespace

  CPU65C02::CPU65C02(Apple2Memory& mem) : m_mem(mem) {
  }

  void CPU65C02::attachMLI(MLIContext& mli) {
    m_mli = &mli;
  }

  void CPU65C02::detachMLI() {
    m_mli = nullptr;
  }

  void CPU65C02::setDebugLogs(std::ostream* mliLog, std::ostream* coutLog) {
    m_mliLog  = mliLog;
    m_coutLog = coutLog;
  }

  void CPU65C02::setTraceLog(std::ostream* traceLog) {
    m_traceLog = traceLog;
  }

  void CPU65C02::recordPCChange(uint16_t fromPC, uint16_t toPC) {
    // Filter out ROM-internal transitions ($F800-$FFFF -> $F800-$FFFF)
    if (fromPC >= 0xF800 && toPC >= 0xF800) {
      return;
    }

    // Check if this is the same transition as the most recent entry
    if (m_pcRingIndex > 0 || m_pcRingFrom[PC_RING_SIZE - 1] != 0) {
      // Find the most recent entry
      size_t prevIndex = (m_pcRingIndex + PC_RING_SIZE - 1) % PC_RING_SIZE;
      if (m_pcRingFrom[prevIndex] == fromPC && m_pcRingTo[prevIndex] == toPC) {
        // Same transition as last entry, just increment the counter
        m_pcRingCount[prevIndex]++;
        return;
      }
    }

    // New transition: add to ring buffer with count = 1
    m_pcRingFrom[m_pcRingIndex]  = fromPC;
    m_pcRingTo[m_pcRingIndex]    = toPC;
    m_pcRingCount[m_pcRingIndex] = 1;
    m_pcRingIndex                = (m_pcRingIndex + 1) % PC_RING_SIZE;
  }

  uint8_t CPU65C02::read8(uint16_t addr) {
    if (addr >= 0xC080 && addr <= 0xC08F) {
      m_mem.applySoftSwitch(addr, true);
      return 0;
    }
    return read_u8(m_mem.constBanks(), addr);
  }

  void CPU65C02::write8(uint16_t addr, uint8_t value) {
    if (addr >= 0xC080 && addr <= 0xC08F) {
      m_mem.applySoftSwitch(addr, false);
      return;
    }

    write_u8(m_mem.banks(), addr, value);
  }

  uint16_t CPU65C02::read16(uint16_t addr) {
    uint8_t lo = read8(addr);
    uint8_t hi = read8(static_cast<uint16_t>(addr + 1));
    return make_u16(lo, hi);
  }

  uint16_t CPU65C02::read16_zp(uint8_t zpAddr) {
    uint8_t lo = read8(zpAddr);
    uint8_t hi = read8(static_cast<uint8_t>(zpAddr + 1));
    return make_u16(lo, hi);
  }

  void CPU65C02::dummyReadLastInstructionByte() {
    // CMOS 65C02-family quirk: on page-crossing indexed reads, the extra bus read is of the
    // last instruction byte (not an invalid effective address read as on NMOS 6502).
    (void)read8(static_cast<uint16_t>(m_r.pc - 1));
  }

  uint8_t CPU65C02::read8_pageCrossed(uint16_t addr, bool pageCrossed) {
    if (pageCrossed) {
      dummyReadLastInstructionByte();
    }
    return read8(addr);
  }

  uint8_t CPU65C02::fetch8() {
    uint8_t v = read8(m_r.pc);
    m_r.pc    = static_cast<uint16_t>(m_r.pc + 1);
    return v;
  }

  uint16_t CPU65C02::fetch16() {
    uint8_t lo = fetch8();
    uint8_t hi = fetch8();
    return make_u16(lo, hi);
  }

  void CPU65C02::push8(uint8_t v) {
    write8(static_cast<uint16_t>(0x0100 | m_r.sp), v);
    m_r.sp = static_cast<uint8_t>(m_r.sp - 1);
  }

  uint8_t CPU65C02::pull8() {
    m_r.sp = static_cast<uint8_t>(m_r.sp + 1);
    return read8(static_cast<uint16_t>(0x0100 | m_r.sp));
  }

  void CPU65C02::push16(uint16_t v) {
    push8(static_cast<uint8_t>((v >> 8) & 0xFF));
    push8(static_cast<uint8_t>(v & 0xFF));
  }

  uint16_t CPU65C02::pull16() {
    uint8_t lo = pull8();
    uint8_t hi = pull8();
    return make_u16(lo, hi);
  }

  void CPU65C02::setFlag(uint8_t mask, bool v) {
    if (v) {
      m_r.p = static_cast<uint8_t>(m_r.p | mask);
    } else {
      m_r.p = static_cast<uint8_t>(m_r.p & ~mask);
    }
    m_r.p = static_cast<uint8_t>(m_r.p | FLAG_U);
  }

  bool CPU65C02::getFlag(uint8_t mask) const {
    return (m_r.p & mask) != 0;
  }

  void CPU65C02::setNZ(uint8_t v) {
    setFlag(FLAG_Z, v == 0);
    setFlag(FLAG_N, (v & 0x80) != 0);
  }

  void CPU65C02::reset() {
    m_waiting = false;
    m_stopped = false;

    m_r.sp = 0xFF;
    m_r.p  = static_cast<uint8_t>(FLAG_I | FLAG_U);

    uint16_t resetVector = read16(VEC_RESET);
    m_r.pc               = resetVector;
    recordPCChange(0x0000, resetVector);  // from=0 for reset
    m_instructionCount = 0;
  }

  uint64_t CPU65C02::run(uint64_t maxInstructions) {
    uint64_t executed = 0;
    while (executed < maxInstructions && !m_stopped) {
      (void)step();
      executed++;
      if (m_waiting) {
        break;
      }
    }
    return executed;
  }

  void CPU65C02::dumpDebugInfo(std::ostream& os) const {
    dump_stack(os, m_mem.constBanks(), m_r.sp);
    dump_pc_ring(os, m_pcRingFrom, m_pcRingTo, m_pcRingCount, PC_RING_SIZE, m_pcRingIndex);
  }

  uint16_t CPU65C02::addr_zp() {
    return fetch8();
  }

  uint16_t CPU65C02::addr_zpx() {
    return static_cast<uint8_t>(fetch8() + m_r.x);
  }

  uint16_t CPU65C02::addr_zpy() {
    return static_cast<uint8_t>(fetch8() + m_r.y);
  }

  uint16_t CPU65C02::addr_abs() {
    return fetch16();
  }

  uint16_t CPU65C02::addr_absx(bool& pageCrossed) {
    uint16_t base = fetch16();
    uint16_t addr = static_cast<uint16_t>(base + m_r.x);
    pageCrossed   = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
  }

  uint16_t CPU65C02::addr_absy(bool& pageCrossed) {
    uint16_t base = fetch16();
    uint16_t addr = static_cast<uint16_t>(base + m_r.y);
    pageCrossed   = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
  }

  uint16_t CPU65C02::addr_ind() {
    uint16_t ptr = fetch16();
    // 65C02 fixes the 6502 page-wrap bug for JMP (abs).
    return read16(ptr);
  }

  uint16_t CPU65C02::addr_indx() {
    uint8_t zp = static_cast<uint8_t>(fetch8() + m_r.x);
    return read16_zp(zp);
  }

  uint16_t CPU65C02::addr_indy(bool& pageCrossed) {
    uint8_t  zp   = fetch8();
    uint16_t base = read16_zp(zp);
    uint16_t addr = static_cast<uint16_t>(base + m_r.y);
    pageCrossed   = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
  }

  uint16_t CPU65C02::addr_zpind() {
    uint8_t zp = fetch8();
    return read16_zp(zp);
  }

  uint16_t CPU65C02::addr_absind_x() {
    uint16_t base = fetch16();
    uint16_t ptr  = static_cast<uint16_t>(base + m_r.x);
    return read16(ptr);
  }

  int8_t CPU65C02::rel8() {
    return static_cast<int8_t>(fetch8());
  }

  uint8_t CPU65C02::adc(uint8_t a, uint8_t b) {
    uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(b) + (getFlag(FLAG_C) ? 1 : 0);

    bool v = (~(a ^ b) & (a ^ static_cast<uint8_t>(sum)) & 0x80) != 0;

    if (getFlag(FLAG_D)) {
      uint16_t lo = (a & 0x0F) + (b & 0x0F) + (getFlag(FLAG_C) ? 1 : 0);
      uint16_t hi = (a & 0xF0) + (b & 0xF0);

      if (lo > 0x09) {
        lo += 0x06;
      }
      if (lo > 0x0F) {
        hi += 0x10;
      }
      if ((hi & 0x1F0) > 0x90) {
        hi += 0x60;
      }

      uint16_t bcd = (lo & 0x0F) | (hi & 0xF0);
      setFlag(FLAG_C, (hi & 0xFF00) != 0);
      setFlag(FLAG_V, v);
      uint8_t r = static_cast<uint8_t>(bcd & 0xFF);
      setNZ(r);
      return r;
    }

    setFlag(FLAG_C, sum > 0xFF);
    setFlag(FLAG_V, v);
    uint8_t r = static_cast<uint8_t>(sum & 0xFF);
    setNZ(r);
    return r;
  }

  uint8_t CPU65C02::sbc(uint8_t a, uint8_t b) {
    uint16_t diff = static_cast<uint16_t>(a) - static_cast<uint16_t>(b) - (getFlag(FLAG_C) ? 0 : 1);

    bool v = ((a ^ b) & (a ^ static_cast<uint8_t>(diff)) & 0x80) != 0;

    if (getFlag(FLAG_D)) {
      int16_t al = static_cast<int16_t>((a & 0x0F) - (b & 0x0F) - (getFlag(FLAG_C) ? 0 : 1));
      int16_t ah = static_cast<int16_t>((a & 0xF0) - (b & 0xF0));

      if (al < 0) {
        al -= 0x06;
        ah -= 0x10;
      }
      if (ah < 0) {
        ah -= 0x60;
      }

      uint16_t bcd = static_cast<uint16_t>((static_cast<uint16_t>(al) & 0x0F) |
                                           (static_cast<uint16_t>(ah) & 0xF0));
      setFlag(FLAG_C, diff < 0x100);
      setFlag(FLAG_V, v);
      uint8_t r = static_cast<uint8_t>(bcd & 0xFF);
      setNZ(r);
      return r;
    }

    setFlag(FLAG_C, diff < 0x100);
    setFlag(FLAG_V, v);
    uint8_t r = static_cast<uint8_t>(diff & 0xFF);
    setNZ(r);
    return r;
  }

  uint8_t CPU65C02::cmp(uint8_t r, uint8_t v) {
    uint16_t diff = static_cast<uint16_t>(r) - static_cast<uint16_t>(v);
    setFlag(FLAG_C, diff < 0x100);
    setNZ(static_cast<uint8_t>(diff & 0xFF));
    return static_cast<uint8_t>(diff & 0xFF);
  }

  void CPU65C02::tsb(uint16_t addr) {
    uint8_t m = read8(addr);
    setFlag(FLAG_Z, (m & m_r.a) == 0);
    write8(addr, static_cast<uint8_t>(m | m_r.a));
  }

  void CPU65C02::trb(uint16_t addr) {
    uint8_t m = read8(addr);
    setFlag(FLAG_Z, (m & m_r.a) == 0);
    write8(addr, static_cast<uint8_t>(m & static_cast<uint8_t>(~m_r.a)));
  }

  void CPU65C02::branch(bool cond) {
    int8_t rel = rel8();
    if (!cond) {
      return;
    }
    uint16_t from        = m_r.pc;
    uint16_t to          = static_cast<uint16_t>(from + rel);
    bool     pageCrossed = (from & 0xFF00) != (to & 0xFF00);
    if (pageCrossed) {
      // Model the 65C02 page-cross extra read.
      dummyReadLastInstructionByte();
    }
    m_r.pc = to;
    recordPCChange(from, to);
  }

  uint32_t CPU65C02::handle_mli_jsr_trap() {
    // ProDOS MLI calling convention:
    //   JSR $BF00
    //   .byte callNumber
    //   .word paramBlockAddr
    // On return, execution continues after these 3 bytes.
    uint16_t callPC     = m_r.pc;  // PC at MLI call (points to call number)
    uint8_t  callNumber = read8(m_r.pc);
    uint16_t paramBlock = read16(static_cast<uint16_t>(m_r.pc + 1));
    uint16_t returnPC   = static_cast<uint16_t>(m_r.pc + 3);
    m_r.pc              = returnPC;
    recordPCChange(0xBF00, returnPC);  // from=$BF00 (MLI entry point)

    uint8_t err = mli_dispatch(*m_mli, m_mem.banks(), callNumber, paramBlock);

    if (m_mliLog != nullptr || m_traceLog != nullptr) {
      std::ostringstream mli_msg;
      mli_msg << "@" << m_instructionCount << " PC=$";
      write_hex(mli_msg, callPC, 4);
      mli_msg << " MLI call=$";
      write_hex(mli_msg, callNumber, 2);
      mli_msg << " (" << mli_call_name(callNumber) << ") param=$";
      write_hex(mli_msg, paramBlock, 4);

      // Extract and log pathnames if applicable
      std::string pathInfo =
          extract_mli_pathnames(m_mem.constBanks(), m_mli, callNumber, paramBlock, err);
      if (!pathInfo.empty()) {
        mli_msg << pathInfo;
      }

      mli_msg << " result=$";
      write_hex(mli_msg, err, 2);
      if (err == 0) {
        mli_msg << " OK\n";
      } else {
        const char* errName = error_name(err);
        if (errName[0] != '\0') {
          mli_msg << " ERROR (" << errName << ")\n";
        } else {
          mli_msg << " ERROR\n";
        }
      }

      // Write to both logs
      if (m_mliLog != nullptr) {
        *m_mliLog << mli_msg.str();
        // Dump stack and PC ring on UNSUPPORTED_STOR_TYPE error
        if (err == ERR_UNSUPPORTED_STOR_TYPE) {
          dump_stack(*m_mliLog, m_mem.constBanks(), m_r.sp);
          dump_pc_ring(*m_mliLog, m_pcRingFrom, m_pcRingTo, m_pcRingCount, PC_RING_SIZE,
                       m_pcRingIndex);
        }
        m_mliLog->flush();
      }
      if (m_traceLog != nullptr) {
        *m_traceLog << mli_msg.str();
      }
    }

    // ProDOS convention: Carry set on error, A holds error code.
    m_r.a = err;
    setFlag(FLAG_C, err != 0);
    setNZ(m_r.a);

    // ProDOS MLI returns with decimal mode cleared.
    setFlag(FLAG_D, false);

    // QUIT ($65): stop emulation after standard MLI handling/logging.
    if (callNumber == 0x65 && err == ERR_NO_ERROR) {
      m_stopped = true;
      m_waiting = false;
    }

    return 6;
  }

  uint32_t CPU65C02::jsr_abs(uint16_t target) {
    // Special trap: JSR $BF00 invokes ProDOS MLI dispatch rather than changing PC.
    if (target == 0xBF00 && m_mli != nullptr) {
      return handle_mli_jsr_trap();
    }

    // Special trap: JSR $DCB8 redirects to $DCBD with A=$A0.
    if (target == 0xDCB8) {
      m_r.a  = 0xA0;
      target = 0xDCBD;
    }

    // Normal JSR behavior.
    // After operand fetch, PC points at the next instruction; JSR pushes (PC-1).
    uint16_t ret   = static_cast<uint16_t>(m_r.pc - 1);
    uint16_t jsrPC = static_cast<uint16_t>(ret - 2);  // JSR instruction address
    push16(ret);
    m_r.pc = target;
    recordPCChange(jsrPC, target);
    return 6;
  }

  uint32_t CPU65C02::execute(uint8_t op) {
    // Rockwell/WDC 65C02 bit manipulation/branch opcodes.
    // RMBn: 07,17,27,37,47,57,67,77 (clear bit n in zp)
    // SMBn: 87,97,A7,B7,C7,D7,E7,F7 (set bit n in zp)
    if ((op & 0x0F) == 0x07) {
      uint8_t bit = static_cast<uint8_t>((op >> 4) & 0x07);
      uint8_t zp  = fetch8();
      uint8_t m   = read8(zp);
      if ((op & 0x80) != 0) {
        m = static_cast<uint8_t>(m | static_cast<uint8_t>(1u << bit));
      } else {
        m = static_cast<uint8_t>(m & static_cast<uint8_t>(~static_cast<uint8_t>(1u << bit)));
      }
      write8(zp, m);
      return 5;
    }

    // BBRn: 0F,1F,2F,3F,4F,5F,6F,7F (branch if bit n clear)
    // BBSn: 8F,9F,AF,BF,CF,DF,EF,FF (branch if bit n set)
    if ((op & 0x0F) == 0x0F) {
      uint8_t bit   = static_cast<uint8_t>((op >> 4) & 0x07);
      bool    isBBS = (op & 0x80) != 0;
      uint8_t zp    = fetch8();
      int8_t  rel   = static_cast<int8_t>(fetch8());

      uint8_t m      = read8(zp);
      bool    bitSet = (m & static_cast<uint8_t>(1u << bit)) != 0;
      bool    take   = isBBS ? bitSet : !bitSet;
      if (take) {
        uint16_t from        = m_r.pc;
        uint16_t to          = static_cast<uint16_t>(from + rel);
        bool     pageCrossed = (from & 0xFF00) != (to & 0xFF00);
        if (pageCrossed) {
          dummyReadLastInstructionByte();
        }
        m_r.pc = to;
        recordPCChange(from, to);
      }
      return 5;
    }

    // Default for reserved/unknown opcodes: treat as 1-byte NOP.
    // Many 65C02 implementations treat undefined opcodes as NOP.
    switch (op) {
      case 0x00: {  // BRK
        // BRK is treated as a 2-byte instruction; PC is incremented once more.
        uint16_t brkPC = static_cast<uint16_t>(m_r.pc - 1);  // BRK instruction address
        m_r.pc         = static_cast<uint16_t>(m_r.pc + 1);
        push16(m_r.pc);
        push8(static_cast<uint8_t>(m_r.p | FLAG_B | FLAG_U));
        setFlag(FLAG_I, true);
        setFlag(FLAG_D, false);  // 65C02 clears D on interrupt
        uint16_t irqVector = read16(VEC_IRQ);
        m_r.pc             = irqVector;
        recordPCChange(brkPC, irqVector);
        return 7;
      }

      case 0xEA:  // NOP
        return 2;

      case 0xDB:  // STP
        m_stopped = true;
        return 3;

      case 0xCB:  // WAI
        m_waiting = true;
        return 3;

      // Flag operations
      case 0x18:
        setFlag(FLAG_C, false);
        return 2;
      case 0x38:
        setFlag(FLAG_C, true);
        return 2;
      case 0x58:
        setFlag(FLAG_I, false);
        return 2;
      case 0x78:
        setFlag(FLAG_I, true);
        return 2;
      case 0xD8:
        setFlag(FLAG_D, false);
        return 2;
      case 0xF8:
        setFlag(FLAG_D, true);
        return 2;
      case 0xB8:
        setFlag(FLAG_V, false);
        return 2;

      // Transfers
      case 0xAA:
        m_r.x = m_r.a;
        setNZ(m_r.x);
        return 2;
      case 0x8A:
        m_r.a = m_r.x;
        setNZ(m_r.a);
        return 2;
      case 0xA8:
        m_r.y = m_r.a;
        setNZ(m_r.y);
        return 2;
      case 0x98:
        m_r.a = m_r.y;
        setNZ(m_r.a);
        return 2;
      case 0xBA:
        m_r.x = m_r.sp;
        setNZ(m_r.x);
        return 2;
      case 0x9A:
        m_r.sp = m_r.x;
        return 2;

      // INC/DEC registers
      case 0xE8:
        m_r.x = static_cast<uint8_t>(m_r.x + 1);
        setNZ(m_r.x);
        return 2;
      case 0xCA:
        m_r.x = static_cast<uint8_t>(m_r.x - 1);
        setNZ(m_r.x);
        return 2;
      case 0xC8:
        m_r.y = static_cast<uint8_t>(m_r.y + 1);
        setNZ(m_r.y);
        return 2;
      case 0x88:
        m_r.y = static_cast<uint8_t>(m_r.y - 1);
        setNZ(m_r.y);
        return 2;

      // INC/DEC accumulator (65C02)
      case 0x1A:
        m_r.a = static_cast<uint8_t>(m_r.a + 1);
        setNZ(m_r.a);
        return 2;
      case 0x3A:
        m_r.a = static_cast<uint8_t>(m_r.a - 1);
        setNZ(m_r.a);
        return 2;

      // Stack
      case 0x48:
        push8(m_r.a);
        return 3;
      case 0x68:
        m_r.a = pull8();
        setNZ(m_r.a);
        return 4;
      case 0x08:
        push8(static_cast<uint8_t>(m_r.p | FLAG_B | FLAG_U));
        return 3;
      case 0x28:
        m_r.p = static_cast<uint8_t>(pull8() | FLAG_U);
        return 4;
      case 0xDA:  // PHX
        push8(m_r.x);
        return 3;
      case 0xFA:  // PLX
        m_r.x = pull8();
        setNZ(m_r.x);
        return 4;
      case 0x5A:  // PHY
        push8(m_r.y);
        return 3;
      case 0x7A:  // PLY
        m_r.y = pull8();
        setNZ(m_r.y);
        return 4;

      // Jumps/returns
      case 0x4C: {                                            // JMP abs
        uint16_t jmpPC  = static_cast<uint16_t>(m_r.pc - 1);  // JMP instruction address
        uint16_t target = fetch16();
        m_r.pc          = target;
        recordPCChange(jmpPC, target);
        return 3;
      }
      case 0x6C: {                                            // JMP (abs)
        uint16_t jmpPC  = static_cast<uint16_t>(m_r.pc - 1);  // JMP instruction address
        uint16_t ptr    = fetch16();
        uint16_t target = read16(ptr);
        if (ptr == COUT_VECTOR_PTR && m_coutLog != nullptr) {
          uint8_t ch = static_cast<uint8_t>(m_r.a & 0x7F);

          // ProDOS convention: 0x0D (CR) -> output newline
          if (ch == 0x0D) {
            *m_coutLog << '\n';
          }
          // Printable ASCII: output as-is
          else if (ch >= 0x20 && ch <= 0x7E) {
            *m_coutLog << static_cast<char>(ch);
          }
          // Control characters: output C-style escape sequences
          else {
            switch (ch) {
              case 0x00:
                *m_coutLog << "\\0";
                break;
              case 0x07:
                *m_coutLog << "\\a";
                break;
              case 0x08:
                *m_coutLog << "\\b";
                break;
              case 0x09:
                *m_coutLog << "\\t";
                break;
              case 0x0A:
                *m_coutLog << "\\n";
                break;
              case 0x0B:
                *m_coutLog << "\\v";
                break;
              case 0x0C:
                *m_coutLog << "\\f";
                break;
              case 0x1B:
                *m_coutLog << "\\e";
                break;
              case 0x7F:
                *m_coutLog << "\\x7f";
                break;
              default:
                // Other control characters: use \xHH notation
                *m_coutLog << "\\x";
                write_hex(*m_coutLog, ch, 2);
                break;
            }
          }
          m_coutLog->flush();
        }
        m_r.pc = target;
        recordPCChange(jmpPC, target);
        return 5;
      }
      case 0x7C: {                                            // JMP (abs,X)
        uint16_t jmpPC  = static_cast<uint16_t>(m_r.pc - 1);  // JMP instruction address
        uint16_t target = addr_absind_x();
        m_r.pc          = target;
        recordPCChange(jmpPC, target);
        return 6;
      }
      case 0x20: {  // JSR abs
        uint16_t target = fetch16();
        return jsr_abs(target);
      }
      case 0x60: {                                                // RTS
        uint16_t rtsPC      = static_cast<uint16_t>(m_r.pc - 1);  // RTS instruction address
        uint16_t returnAddr = static_cast<uint16_t>(pull16() + 1);
        m_r.pc              = returnAddr;
        recordPCChange(rtsPC, returnAddr);
        return 6;
      }
      case 0x40: {                                              // RTI
        uint16_t rtiPC    = static_cast<uint16_t>(m_r.pc - 1);  // RTI instruction address
        m_r.p             = static_cast<uint8_t>(pull8() | FLAG_U);
        uint16_t returnPC = pull16();
        m_r.pc            = returnPC;
        recordPCChange(rtiPC, returnPC);
        return 6;
      }

      // Branches
      case 0x80:  // BRA
        branch(true);
        return 3;
      case 0x10:
        branch(!getFlag(FLAG_N));
        return 2;
      case 0x30:
        branch(getFlag(FLAG_N));
        return 2;
      case 0x50:
        branch(!getFlag(FLAG_V));
        return 2;
      case 0x70:
        branch(getFlag(FLAG_V));
        return 2;
      case 0x90:
        branch(!getFlag(FLAG_C));
        return 2;
      case 0xB0:
        branch(getFlag(FLAG_C));
        return 2;
      case 0xD0:
        branch(!getFlag(FLAG_Z));
        return 2;
      case 0xF0:
        branch(getFlag(FLAG_Z));
        return 2;

      // Loads
      case 0xA9:  // LDA #imm
        m_r.a = fetch8();
        setNZ(m_r.a);
        return 2;
      case 0xA5:
        m_r.a = read8(addr_zp());
        setNZ(m_r.a);
        return 3;
      case 0xB5:
        m_r.a = read8(addr_zpx());
        setNZ(m_r.a);
        return 4;
      case 0xAD:
        m_r.a = read8(addr_abs());
        setNZ(m_r.a);
        return 4;
      case 0xBD: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xB9: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xA1:
        m_r.a = read8(addr_indx());
        setNZ(m_r.a);
        return 6;
      case 0xB1: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0xB2:  // LDA (zp)
        m_r.a = read8(addr_zpind());
        setNZ(m_r.a);
        return 5;

      case 0xA2:  // LDX #imm
        m_r.x = fetch8();
        setNZ(m_r.x);
        return 2;
      case 0xA6:
        m_r.x = read8(addr_zp());
        setNZ(m_r.x);
        return 3;
      case 0xB6:
        m_r.x = read8(addr_zpy());
        setNZ(m_r.x);
        return 4;
      case 0xAE:
        m_r.x = read8(addr_abs());
        setNZ(m_r.x);
        return 4;
      case 0xBE: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.x       = read8_pageCrossed(a, pc);
        setNZ(m_r.x);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }

      case 0xA0:  // LDY #imm
        m_r.y = fetch8();
        setNZ(m_r.y);
        return 2;
      case 0xA4:
        m_r.y = read8(addr_zp());
        setNZ(m_r.y);
        return 3;
      case 0xB4:
        m_r.y = read8(addr_zpx());
        setNZ(m_r.y);
        return 4;
      case 0xAC:
        m_r.y = read8(addr_abs());
        setNZ(m_r.y);
        return 4;
      case 0xBC: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.y       = read8_pageCrossed(a, pc);
        setNZ(m_r.y);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }

      // Stores
      case 0x85:
        write8(addr_zp(), m_r.a);
        return 3;
      case 0x95:
        write8(addr_zpx(), m_r.a);
        return 4;
      case 0x8D:
        write8(addr_abs(), m_r.a);
        return 4;
      case 0x9D: {
        bool pc = false;
        write8(addr_absx(pc), m_r.a);
        return 5;
      }
      case 0x99: {
        bool pc = false;
        write8(addr_absy(pc), m_r.a);
        return 5;
      }
      case 0x81:
        write8(addr_indx(), m_r.a);
        return 6;
      case 0x91: {
        bool pc = false;
        write8(addr_indy(pc), m_r.a);
        return 6;
      }
      case 0x92:  // STA (zp)
        write8(addr_zpind(), m_r.a);
        return 5;

      case 0x86:
        write8(addr_zp(), m_r.x);
        return 3;
      case 0x96:
        write8(addr_zpy(), m_r.x);
        return 4;
      case 0x8E:
        write8(addr_abs(), m_r.x);
        return 4;

      case 0x84:
        write8(addr_zp(), m_r.y);
        return 3;
      case 0x94:
        write8(addr_zpx(), m_r.y);
        return 4;
      case 0x8C:
        write8(addr_abs(), m_r.y);
        return 4;

      // STZ
      case 0x64:
        write8(addr_zp(), 0);
        return 3;
      case 0x74:
        write8(addr_zpx(), 0);
        return 4;
      case 0x9C:
        write8(addr_abs(), 0);
        return 4;
      case 0x9E: {
        bool pc = false;
        write8(addr_absx(pc), 0);
        return 5;
      }

      // Logical
      // ORA
      case 0x09:
        m_r.a = static_cast<uint8_t>(m_r.a | fetch8());
        setNZ(m_r.a);
        return 2;
      case 0x05:
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_zp()));
        setNZ(m_r.a);
        return 3;
      case 0x15:
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_zpx()));
        setNZ(m_r.a);
        return 4;
      case 0x0D:
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_abs()));
        setNZ(m_r.a);
        return 4;
      case 0x1D: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a | read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x19: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a | read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x01:
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_indx()));
        setNZ(m_r.a);
        return 6;
      case 0x11: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a | read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0x12:  // ORA (zp)
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_zpind()));
        setNZ(m_r.a);
        return 5;

      // AND
      case 0x29:
        m_r.a = static_cast<uint8_t>(m_r.a & fetch8());
        setNZ(m_r.a);
        return 2;
      case 0x25:
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_zp()));
        setNZ(m_r.a);
        return 3;
      case 0x35:
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_zpx()));
        setNZ(m_r.a);
        return 4;
      case 0x2D:
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_abs()));
        setNZ(m_r.a);
        return 4;
      case 0x3D: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a & read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x39: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a & read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x21:
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_indx()));
        setNZ(m_r.a);
        return 6;
      case 0x31: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a & read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0x32:  // AND (zp)
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_zpind()));
        setNZ(m_r.a);
        return 5;

      // EOR
      case 0x49:
        m_r.a = static_cast<uint8_t>(m_r.a ^ fetch8());
        setNZ(m_r.a);
        return 2;
      case 0x45:
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_zp()));
        setNZ(m_r.a);
        return 3;
      case 0x55:
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_zpx()));
        setNZ(m_r.a);
        return 4;
      case 0x4D:
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_abs()));
        setNZ(m_r.a);
        return 4;
      case 0x5D: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a ^ read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x59: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a ^ read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x41:
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_indx()));
        setNZ(m_r.a);
        return 6;
      case 0x51: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a ^ read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0x52:  // EOR (zp)
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_zpind()));
        setNZ(m_r.a);
        return 5;

      // ADC/SBC
      case 0x69:
        m_r.a = adc(m_r.a, fetch8());
        return 2;
      case 0x65:
        m_r.a = adc(m_r.a, read8(addr_zp()));
        return 3;
      case 0x75:
        m_r.a = adc(m_r.a, read8(addr_zpx()));
        return 4;
      case 0x6D:
        m_r.a = adc(m_r.a, read8(addr_abs()));
        return 4;
      case 0x7D: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = adc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x79: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = adc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x61:
        m_r.a = adc(m_r.a, read8(addr_indx()));
        return 6;
      case 0x71: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = adc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0x72:
        m_r.a = adc(m_r.a, read8(addr_zpind()));
        return 5;

      case 0xE9:
        m_r.a = sbc(m_r.a, fetch8());
        return 2;
      case 0xE5:
        m_r.a = sbc(m_r.a, read8(addr_zp()));
        return 3;
      case 0xF5:
        m_r.a = sbc(m_r.a, read8(addr_zpx()));
        return 4;
      case 0xED:
        m_r.a = sbc(m_r.a, read8(addr_abs()));
        return 4;
      case 0xFD: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = sbc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xF9: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = sbc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xE1:
        m_r.a = sbc(m_r.a, read8(addr_indx()));
        return 6;
      case 0xF1: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = sbc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0xF2:
        m_r.a = sbc(m_r.a, read8(addr_zpind()));
        return 5;

      // Compare
      case 0xC9:
        (void)cmp(m_r.a, fetch8());
        return 2;
      case 0xC5:
        (void)cmp(m_r.a, read8(addr_zp()));
        return 3;
      case 0xD5:
        (void)cmp(m_r.a, read8(addr_zpx()));
        return 4;
      case 0xCD:
        (void)cmp(m_r.a, read8(addr_abs()));
        return 4;
      case 0xDD: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        (void)cmp(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xD9: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        (void)cmp(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xC1:
        (void)cmp(m_r.a, read8(addr_indx()));
        return 6;
      case 0xD1: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        (void)cmp(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0xD2:
        (void)cmp(m_r.a, read8(addr_zpind()));
        return 5;

      case 0xE0:
        (void)cmp(m_r.x, fetch8());
        return 2;
      case 0xE4:
        (void)cmp(m_r.x, read8(addr_zp()));
        return 3;
      case 0xEC:
        (void)cmp(m_r.x, read8(addr_abs()));
        return 4;
      case 0xC0:
        (void)cmp(m_r.y, fetch8());
        return 2;
      case 0xC4:
        (void)cmp(m_r.y, read8(addr_zp()));
        return 3;
      case 0xCC:
        (void)cmp(m_r.y, read8(addr_abs()));
        return 4;

      // INC/DEC memory
      case 0xE6: {
        uint16_t a = addr_zp();
        uint8_t  v = static_cast<uint8_t>(read8(a) + 1);
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0xF6: {
        uint16_t a = addr_zpx();
        uint8_t  v = static_cast<uint8_t>(read8(a) + 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0xEE: {
        uint16_t a = addr_abs();
        uint8_t  v = static_cast<uint8_t>(read8(a) + 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0xFE: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = static_cast<uint8_t>(read8(a) + 1);
        write8(a, v);
        setNZ(v);
        return 7;
      }
      case 0xC6: {
        uint16_t a = addr_zp();
        uint8_t  v = static_cast<uint8_t>(read8(a) - 1);
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0xD6: {
        uint16_t a = addr_zpx();
        uint8_t  v = static_cast<uint8_t>(read8(a) - 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0xCE: {
        uint16_t a = addr_abs();
        uint8_t  v = static_cast<uint8_t>(read8(a) - 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0xDE: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = static_cast<uint8_t>(read8(a) - 1);
        write8(a, v);
        setNZ(v);
        return 7;
      }

      // Shifts/rotates
      case 0x0A: {  // ASL A
        setFlag(FLAG_C, (m_r.a & 0x80) != 0);
        m_r.a = static_cast<uint8_t>(m_r.a << 1);
        setNZ(m_r.a);
        return 2;
      }
      case 0x06: {
        uint16_t a = addr_zp();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>(v << 1);
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0x16: {
        uint16_t a = addr_zpx();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>(v << 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x0E: {
        uint16_t a = addr_abs();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>(v << 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x1E: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8(a);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>(v << 1);
        write8(a, v);
        setNZ(v);
        return 7;
      }

      case 0x4A: {  // LSR A
        setFlag(FLAG_C, (m_r.a & 0x01) != 0);
        m_r.a = static_cast<uint8_t>(m_r.a >> 1);
        setNZ(m_r.a);
        return 2;
      }
      case 0x46: {
        uint16_t a = addr_zp();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>(v >> 1);
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0x56: {
        uint16_t a = addr_zpx();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>(v >> 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x4E: {
        uint16_t a = addr_abs();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>(v >> 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x5E: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8(a);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>(v >> 1);
        write8(a, v);
        setNZ(v);
        return 7;
      }

      case 0x2A: {  // ROL A
        bool c = getFlag(FLAG_C);
        setFlag(FLAG_C, (m_r.a & 0x80) != 0);
        m_r.a = static_cast<uint8_t>((m_r.a << 1) | (c ? 1 : 0));
        setNZ(m_r.a);
        return 2;
      }
      case 0x26: {
        uint16_t a = addr_zp();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0x36: {
        uint16_t a = addr_zpx();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x2E: {
        uint16_t a = addr_abs();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x3E: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8(a);
        bool     c  = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
        write8(a, v);
        setNZ(v);
        return 7;
      }

      case 0x6A: {  // ROR A
        bool c = getFlag(FLAG_C);
        setFlag(FLAG_C, (m_r.a & 0x01) != 0);
        m_r.a = static_cast<uint8_t>((m_r.a >> 1) | (c ? 0x80 : 0));
        setNZ(m_r.a);
        return 2;
      }
      case 0x66: {
        uint16_t a = addr_zp();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0x76: {
        uint16_t a = addr_zpx();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x6E: {
        uint16_t a = addr_abs();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x7E: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8(a);
        bool     c  = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
        write8(a, v);
        setNZ(v);
        return 7;
      }

      // BIT
      case 0x89: {  // BIT #imm (65C02)
        uint8_t v = fetch8();
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        return 2;
      }
      case 0x24: {
        uint8_t v = read8(addr_zp());
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        setFlag(FLAG_N, (v & 0x80) != 0);
        setFlag(FLAG_V, (v & 0x40) != 0);
        return 3;
      }
      case 0x2C: {
        uint8_t v = read8(addr_abs());
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        setFlag(FLAG_N, (v & 0x80) != 0);
        setFlag(FLAG_V, (v & 0x40) != 0);
        return 4;
      }
      case 0x34: {
        uint8_t v = read8(addr_zpx());
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        setFlag(FLAG_N, (v & 0x80) != 0);
        setFlag(FLAG_V, (v & 0x40) != 0);
        return 4;
      }
      case 0x3C: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8_pageCrossed(a, pc);
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        setFlag(FLAG_N, (v & 0x80) != 0);
        setFlag(FLAG_V, (v & 0x40) != 0);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }

      // TSB/TRB
      case 0x04:
        tsb(addr_zp());
        return 5;
      case 0x0C:
        tsb(addr_abs());
        return 6;
      case 0x14:
        trb(addr_zp());
        return 5;
      case 0x1C:
        trb(addr_abs());
        return 6;

      // Unused opcodes on WDC 65C02: documented as NOP variants.
      // See: 6502.org 65C02 opcodes, section "Unused opcodes (undocumented NOPs)".
      // 1-byte, 1-cycle NOPs (no operand)
      case 0x03:
      case 0x0B:
      case 0x13:
      case 0x1B:
      case 0x23:
      case 0x2B:
      case 0x33:
      case 0x3B:
      case 0x43:
      case 0x4B:
      case 0x53:
      case 0x5B:
      case 0x63:
      case 0x6B:
      case 0x73:
      case 0x7B:
      case 0x83:
      case 0x8B:
      case 0x93:
      case 0x9B:
      case 0xA3:
      case 0xAB:
      case 0xB3:
      case 0xBB:
      case 0xC3:
      case 0xD3:
      case 0xE3:
      case 0xEB:
      case 0xF3:
      case 0xFB:
        return 1;

      // 2-byte, 2-cycle NOP immediate
      case 0x02:
      case 0x22:
      case 0x42:
      case 0x62:
      case 0x82:
      case 0xC2:
      case 0xE2:
        (void)fetch8();
        return 2;

      // 2-byte NOP with zp read
      case 0x44: {
        uint8_t zp = fetch8();
        (void)read8(zp);
        return 3;
      }

      // 2-byte NOP with zp,X read
      case 0x54:
      case 0xD4:
      case 0xF4: {
        uint8_t zp = fetch8();
        (void)read8(static_cast<uint8_t>(zp + m_r.x));
        return 4;
      }

      // 3-byte NOP with absolute read
      case 0xDC:
      case 0xFC: {
        uint16_t a = fetch16();
        (void)read8(a);
        return 4;
      }

      // 3-byte NOP with unusual read behavior (treat as absolute read for now)
      case 0x5C: {
        uint16_t a = fetch16();
        (void)read8(a);
        return 8;
      }

      default:
        return 2;
    }
  }

  uint32_t CPU65C02::step() {
    if (m_stopped) {
      return 0;
    }
    if (m_waiting) {
      return 0;
    }

    m_instructionCount++;

    // Trace execution in key PC ranges when trace log is enabled
    if (m_traceLog != nullptr) {
      uint16_t pc = m_r.pc;

      // Log known key entry points always
      switch (pc) {
        case 0x7800:  // EdAsm.Asm entry point
          *m_traceLog << "@" << m_instructionCount << " PC=$7800 >>> ENTER EdAsm.Asm\n";
          break;
        case 0x7816:  // ExecAsm (Asm2.S line 17)
          *m_traceLog << "@" << m_instructionCount << " PC=$7816 >>> ENTER ExecAsm";
          *m_traceLog << " PassNbr(ZP$67)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0x67)) << " GenF(ZP$BF)=$"
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0x7E30:  // DoPass1 (Asm2.S line 1015)
          *m_traceLog << "@" << m_instructionCount << " PC=$7E30 >>> ENTER DoPass1";
          *m_traceLog << " PassNbr(ZP$67)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0x67)) << " GenF(ZP$BF)=$"
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0x7F0F:  // DoPass2 (Asm2.S line 1164)
          *m_traceLog << "@" << m_instructionCount << " PC=$7F0F >>> ENTER DoPass2";
          *m_traceLog << " PassNbr(ZP$67)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0x67)) << " GenF(ZP$BF)=$"
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0xD000:  // DoPass3 (Asm1.S line 25)
          *m_traceLog << "@" << m_instructionCount << " PC=$D000 >>> ENTER DoPass3";
          *m_traceLog << " PassNbr(ZP$67)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0x67)) << " GenF(ZP$BF)=$"
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0x7E45:  // FlushObj (Asm2.S line 46)
          *m_traceLog << "@" << m_instructionCount << " PC=$7E45 >>> FlushObj";
          *m_traceLog << " GenF(ZP$BF)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0x99DF:  // L99DF - actual flush routine (ASM3.S line 2640)
          *m_traceLog << "@" << m_instructionCount << " PC=$99DF >>> L99DF (flush obj code)";
          *m_traceLog << " GenF(ZP$BF)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0x8A82:  // L8A82 - ORG directive entry (ASM3.S line 62)
          *m_traceLog << "@" << m_instructionCount << " PC=$8A82 >>> ORG directive";
          *m_traceLog << " PassNbr(ZP$67)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0x67)) << " GenF(ZP$BF)=$"
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0x8A9A:  // L8A9A - ORG checks GenF for disk write (ASM3.S line ~72)
          *m_traceLog << "@" << m_instructionCount << " PC=$8A9A >>> ORG GenF check";
          *m_traceLog << " GenF(ZP$BF)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0x8AAE:  // L8AAE - ORG opens file and clears suppression (ASM3.S line ~81)
          *m_traceLog << "@" << m_instructionCount << " PC=$8AAE >>> ORG open file path";
          *m_traceLog << " GenF(ZP$BF)=$" << std::hex << std::uppercase << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(read8(0xBF)) << std::dec << "\n";
          break;
        case 0x9918:  // Open4RW (ASM3.S line 2478)
          *m_traceLog << "@" << m_instructionCount << " PC=$9918 >>> Open4RW\n";
          break;
        case 0x7C98:  // PrtSetup (Asm2.S line 840)
          *m_traceLog << "@" << m_instructionCount << " PC=$7C98 >>> PrtSetup\n";
          break;
        case 0x7D07:  // ParseDCS (Asm2.S line 907)
          *m_traceLog << "@" << m_instructionCount << " PC=$7D07 >>> ParseDCS\n";
          break;
        case 0x7D2E:  // IsFileLst (Asm2.S line 926)
          *m_traceLog << "@" << m_instructionCount << " PC=$7D2E >>> IsFileLst\n";
          break;
        case 0x7D3A:  // Lst2File (Asm2.S line 938)
          *m_traceLog << "@" << m_instructionCount << " PC=$7D3A >>> Lst2File\n";
          break;
        case 0xA70B:  // XA70B - command line input (EI)
          *m_traceLog << "@" << m_instructionCount << " PC=$A70B >>> XA70B (get user cmd)\n";
          break;
        case 0xB6E6:  // DoAsmbly - prepare for assembly (EI line 759)
          *m_traceLog << "@" << m_instructionCount << " PC=$B6E6 >>> DoAsmbly (prep for ASM)\n";
          break;
      }
    }

    uint8_t op = fetch8();

    auto passnbr67_mutator_name = [](uint8_t opcode) -> const char* {
      switch (opcode) {
        case 0x85:  // STA zp
        case 0x95:  // STA zp,X
        case 0x8D:  // STA abs
        case 0x9D:  // STA abs,X
        case 0x99:  // STA abs,Y
        case 0x81:  // STA (zp,X)
        case 0x91:  // STA (zp),Y
        case 0x92:  // STA (zp)
          return "STA";
        case 0xE6:  // INC zp
        case 0xF6:  // INC zp,X
        case 0xEE:  // INC abs
        case 0xFE:  // INC abs,X
          return "INC";
        default:
          return nullptr;
      }
    };

    // Track GenF ($BF), ListingF ($68), DskListF ($90), and PassNbr ($67)
    // for STA/INC-driven transitions.
    uint8_t old_genf = 0, old_listingf = 0, old_dsklistf = 0, old_passnbr = 0;
    bool    track_flags = (m_traceLog != nullptr);
    if (track_flags) {
      old_genf     = read8(0xBF);
      old_listingf = read8(0x68);
      old_dsklistf = read8(0x90);
      old_passnbr  = read8(0x67);
    }

    uint32_t cycles = execute(op);

    // Log if GenF, ListingF, DskListF, or PassNbr changed
    if (track_flags) {
      uint8_t new_genf     = read8(0xBF);
      uint8_t new_listingf = read8(0x68);
      uint8_t new_dsklistf = read8(0x90);
      uint8_t new_passnbr  = read8(0x67);

      const char* passnbr_mutator = passnbr67_mutator_name(op);
      if (passnbr_mutator != nullptr && new_passnbr != old_passnbr) {
        *m_traceLog << "@" << m_instructionCount << " PC=$" << std::hex << std::uppercase
                    << std::setfill('0') << std::setw(4) << m_r.pc << " " << passnbr_mutator
                    << " PassNbr($67): $" << std::setw(2) << static_cast<unsigned>(old_passnbr)
                    << " -> $" << std::setw(2) << static_cast<unsigned>(new_passnbr) << std::dec
                    << "\n";
      }

      if (new_genf != old_genf) {
        *m_traceLog << "@" << m_instructionCount << " PC=$" << std::hex << std::uppercase
                    << std::setfill('0') << std::setw(4) << m_r.pc << " GenF($BF): $"
                    << std::setw(2) << static_cast<unsigned>(old_genf) << " -> $" << std::setw(2)
                    << static_cast<unsigned>(new_genf) << std::dec << "\n";
      }

      if (new_listingf != old_listingf) {
        *m_traceLog << "@" << m_instructionCount << " PC=$" << std::hex << std::uppercase
                    << std::setfill('0') << std::setw(4) << m_r.pc << " ListingF($68): $"
                    << std::setw(2) << static_cast<unsigned>(old_listingf) << " -> $"
                    << std::setw(2) << static_cast<unsigned>(new_listingf) << std::dec << "\n";
      }

      if (new_dsklistf != old_dsklistf) {
        *m_traceLog << "@" << m_instructionCount << " PC=$" << std::hex << std::uppercase
                    << std::setfill('0') << std::setw(4) << m_r.pc << " DskListF($90): $"
                    << std::setw(2) << static_cast<unsigned>(old_dsklistf) << " -> $"
                    << std::setw(2) << static_cast<unsigned>(new_dsklistf) << std::dec << "\n";
      }
    }

    return cycles;
  }

}  // namespace prodos8emu
