#include "prodos8emu/mli.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/path.hpp"

namespace prodos8emu {

  MLIContext::MLIContext()
      : m_initialized(true), m_prefix(""), m_volumesRoot(std::filesystem::current_path()) {
  }

  MLIContext::MLIContext(const std::filesystem::path& volumesRoot)
      : m_initialized(true), m_prefix(""), m_volumesRoot(volumesRoot) {
  }

  MLIContext::~MLIContext() {
    // Close all open files
    for (auto& [rn, of] : m_openFiles) {
      ::close(of.fd);
    }
  }

  bool MLIContext::isInitialized() const {
    return m_initialized;
  }

  uint8_t MLIContext::setPrefixCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    // Read parameter block: +0 = param_count, +1 = pathname pointer
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 1) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint16_t pathnamePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));

    // Read and normalize the pathname
    // First check the count byte to reject overly long paths
    uint8_t pathLength = read_u8(banks, pathnamePtr);
    if (pathLength > 64) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    std::string pathname = readNormalizedCountedString(banks, pathnamePtr);

    // Empty pathname clears the prefix (ProDOS convention)
    if (pathname.empty()) {
      m_prefix.clear();
      return ERR_NO_ERROR;
    }

    // Reject partial pathname with empty prefix
    if (pathname[0] != '/' && m_prefix.empty()) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Resolve to full path
    std::string fullPath = resolveFullPath(pathname, m_prefix);

    // Check if resolved path is valid
    if (fullPath.empty()) {
      return ERR_INVALID_PATH_SYNTAX;  // Too long after resolution
    }

    // Validate the syntax and length (prefix must be <= 64)
    if (!isValidPathname(fullPath, 64)) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Update prefix
    m_prefix = fullPath;

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::getPrefixCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    // Read parameter block: +0 = param_count, +1 = data_buffer pointer
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 1) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint16_t dataBufferPtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));

    // Write counted string to buffer
    write_u8(banks, dataBufferPtr, static_cast<uint8_t>(m_prefix.length()));

    for (size_t i = 0; i < m_prefix.length(); i++) {
      write_u8(banks, static_cast<uint16_t>(dataBufferPtr + 1 + i),
               static_cast<uint8_t>(m_prefix[i]));
    }

    return ERR_NO_ERROR;
  }

  bool MLIContext::isDirectoryRefNum(uint8_t refNum) const {
    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return false;
    }
    return it->second.isDirectory;
  }

  uint32_t MLIContext::getMarkForRefNum(uint8_t refNum) const {
    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return 0;
    }
    return it->second.mark;
  }

  uint32_t MLIContext::getEofForRefNum(uint8_t refNum) const {
    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return 0;
    }

    const OpenFile& of = it->second;

    if (of.isDirectory) {
      uint64_t eof = static_cast<uint64_t>(of.directoryBlocks.size()) * 512ull;
      if (eof > 0x00FFFFFFull) {
        return 0x00FFFFFFu;
      }
      return static_cast<uint32_t>(eof);
    }

    struct stat st;
    if (::fstat(of.fd, &st) != 0) {
      return 0;
    }

    if (st.st_size < 0) {
      return 0;
    }

    uint64_t eof = static_cast<uint64_t>(st.st_size);
    if (eof > 0x00FFFFFFull) {
      return 0x00FFFFFFu;
    }
    return static_cast<uint32_t>(eof);
  }

  std::string getVersion() {
    return PRODOS8EMU_VERSION;
  }

}  // namespace prodos8emu
