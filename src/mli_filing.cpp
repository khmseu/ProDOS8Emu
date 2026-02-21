#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <optional>

#include "prodos8emu/access_byte.hpp"
#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"
#include "prodos8emu/path.hpp"
#include "prodos8emu/xattr.hpp"

namespace prodos8emu {

  namespace {

    // Maximum ProDOS open files
    constexpr uint8_t MAX_REF_NUM = 8;

    /**
     * Get the current file size (eof) using fstat.
     * Returns nullopt on error, otherwise the file size capped at the 24-bit max.
     */
    std::optional<uint32_t> getFileEof(int fd) {
      struct stat st;
      if (::fstat(fd, &st) != 0) {
        return std::nullopt;
      }
      if (st.st_size > 0x00FFFFFF) return 0x00FFFFFFu;
      return static_cast<uint32_t>(st.st_size);
    }

  }  // anonymous namespace

  uint8_t MLIContext::openCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 3) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint16_t pathnamePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    uint16_t ioBuffer    = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 3));

    // Check pathname length
    uint8_t pathLen = read_u8(banks, pathnamePtr);
    if (pathLen > 64) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Build const view for path helpers
    ConstMemoryBanks constBanks;
    for (size_t i = 0; i < NUM_BANKS; i++) {
      constBanks[i] = banks[i];
    }

    std::string pathname = readNormalizedCountedString(constBanks, pathnamePtr);
    if (pathname.empty()) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Resolve path
    if (pathname[0] != '/') {
      pathname = resolveFullPath(pathname, m_prefix);
      if (pathname.empty() || pathname[0] != '/') {
        return ERR_INVALID_PATH_SYNTAX;
      }
    }

    if (!isValidPathname(pathname, 128)) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Map to host
    std::filesystem::path hostPath = mapToHostPath(pathname, m_volumesRoot);

    // Check file exists and is not a directory
    if (!std::filesystem::exists(hostPath)) {
      return ERR_FILE_NOT_FOUND;
    }
    if (std::filesystem::is_directory(hostPath)) {
      return ERR_UNSUPPORTED_STOR_TYPE;
    }

    // Load metadata to check access bits
    std::string metaValue;
    uint8_t     access = 0xC3;  // default: read+write+rename+destroy
    if (prodos8_get_xattr(hostPath.string(), "access", metaValue) == ERR_NO_ERROR &&
        !metaValue.empty()) {
      uint8_t accessByte;
      if (parse_access_byte(metaValue, accessByte)) {
        access = accessByte;
      }
    }

    // Check read access (bit 0)
    if (!(access & 0x01)) {
      return ERR_ACCESS_ERROR;
    }

    // Allocate lowest free ref_num (1-8)
    uint8_t refNum = 0;
    for (uint8_t r = 1; r <= MAX_REF_NUM; r++) {
      if (m_openFiles.find(r) == m_openFiles.end()) {
        refNum = r;
        break;
      }
    }
    if (refNum == 0) {
      return ERR_TOO_MANY_FILES_OPEN;
    }

    // Open file: try R+W first, fall back to R only
    int fd = ::open(hostPath.string().c_str(), O_RDWR);
    if (fd < 0) {
      fd = ::open(hostPath.string().c_str(), O_RDONLY);
      if (fd < 0) {
        if (errno == EACCES || errno == EPERM) {
          return ERR_ACCESS_ERROR;
        }
        return ERR_IO_ERROR;
      }
    }

    // Store open file entry
    OpenFile of;
    of.fd               = fd;
    of.mark             = 0;
    of.ioBuffer         = ioBuffer;
    of.newlineEnabled   = false;
    of.newlineMask      = 0;
    of.newlineChar      = 0;
    m_openFiles[refNum] = of;

    // Write ref_num back to parameter block
    write_u8(banks, static_cast<uint16_t>(paramBlockAddr + 5), refNum);

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::newlineCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 3) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t refNum     = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    uint8_t enableMask = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 2));
    uint8_t nlChar     = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 3));

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    it->second.newlineEnabled = (enableMask != 0);
    it->second.newlineMask    = enableMask;
    it->second.newlineChar    = nlChar;

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::readCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 4) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t  refNum       = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    uint16_t dataBuf      = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));
    uint16_t requestCount = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 4));

    // Initialize trans_count to 0
    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6), 0);

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    OpenFile&               of     = it->second;
    std::optional<uint32_t> eofOpt = getFileEof(of.fd);
    if (!eofOpt) {
      return ERR_IO_ERROR;
    }
    uint32_t eof = *eofOpt;

    if (of.mark >= eof) {
      return ERR_EOF_ENCOUNTERED;
    }

    uint16_t transCount = 0;
    uint8_t  retErr     = ERR_NO_ERROR;

    for (uint16_t i = 0; i < requestCount; i++) {
      if (of.mark >= eof) {
        retErr = ERR_EOF_ENCOUNTERED;
        break;
      }

      uint8_t byte;
      ssize_t n = ::pread(of.fd, &byte, 1, static_cast<off_t>(of.mark));
      if (n <= 0) {
        retErr = (n == 0) ? ERR_EOF_ENCOUNTERED : ERR_IO_ERROR;
        break;
      }

      write_u8(banks, static_cast<uint16_t>(dataBuf + i), byte);
      of.mark++;
      transCount++;

      // Check newline mode: stop after byte that matches newline condition
      if (of.newlineEnabled && (byte & of.newlineMask) == (of.newlineChar & of.newlineMask)) {
        break;
      }
    }

    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6), transCount);
    return retErr;
  }

  uint8_t MLIContext::writeCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 4) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t  refNum       = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    uint16_t dataBuf      = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));
    uint16_t requestCount = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 4));

    // Initialize trans_count to 0
    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6), 0);

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    OpenFile& of = it->second;

    uint16_t transCount = 0;

    for (uint16_t i = 0; i < requestCount; i++) {
      if (of.mark > 0x00FFFFFF) {
        break;
      }

      uint8_t byte = read_u8(banks, static_cast<uint16_t>(dataBuf + i));

      ssize_t n = ::pwrite(of.fd, &byte, 1, static_cast<off_t>(of.mark));
      if (n < 0) {
        if (errno == EACCES || errno == EPERM) {
          write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6), transCount);
          return ERR_ACCESS_ERROR;
        } else if (errno == ENOSPC) {
          write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6), transCount);
          return ERR_VOLUME_FULL;
        }
        write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6), transCount);
        return ERR_IO_ERROR;
      }

      of.mark++;
      transCount++;
    }

    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6), transCount);
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::closeCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 1) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));

    if (refNum == 0) {
      for (auto& [rn, of] : m_openFiles) {
        ::close(of.fd);
      }
      m_openFiles.clear();
      return ERR_NO_ERROR;
    }

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    ::close(it->second.fd);
    m_openFiles.erase(it);
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::flushCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 1) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));

    if (refNum == 0) {
      for (auto& [rn, of] : m_openFiles) {
        ::fsync(of.fd);
      }
      return ERR_NO_ERROR;
    }

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    if (::fsync(it->second.fd) != 0) {
      return ERR_IO_ERROR;
    }
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::setMarkCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t  refNum   = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    uint32_t position = read_u24_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    std::optional<uint32_t> eofOpt = getFileEof(it->second.fd);
    if (!eofOpt) {
      return ERR_IO_ERROR;
    }

    if (position > *eofOpt) {
      return ERR_POSITION_OUT_OF_RANGE;
    }

    it->second.mark = position;
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::getMarkCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    write_u24_le(banks, static_cast<uint16_t>(paramBlockAddr + 2), it->second.mark);
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::setEofCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t  refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    uint32_t newEof = read_u24_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    OpenFile& of = it->second;

    if (::ftruncate(of.fd, static_cast<off_t>(newEof)) != 0) {
      if (errno == EACCES || errno == EPERM) {
        return ERR_ACCESS_ERROR;
      } else if (errno == ENOSPC) {
        return ERR_VOLUME_FULL;
      }
      return ERR_IO_ERROR;
    }

    if (of.mark > newEof) {
      of.mark = newEof;
    }

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::getEofCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    std::optional<uint32_t> eofOpt = getFileEof(it->second.fd);
    if (!eofOpt) {
      return ERR_IO_ERROR;
    }
    write_u24_le(banks, static_cast<uint16_t>(paramBlockAddr + 2), *eofOpt);
    return ERR_NO_ERROR;
  }

}  // namespace prodos8emu
