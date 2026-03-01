#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <optional>
#include <vector>

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

    // ProDOS directory entry constants
    constexpr uint8_t ENTRY_LENGTH            = 0x27;  // 39 bytes per entry
    constexpr uint8_t ENTRIES_PER_BLOCK       = 0x0D;  // 13 entries per 512-byte block
    constexpr uint8_t STORAGE_TYPE_SEEDLING   = 0x01;
    constexpr uint8_t STORAGE_TYPE_SAPLING    = 0x02;
    constexpr uint8_t STORAGE_TYPE_TREE       = 0x03;
    constexpr uint8_t STORAGE_TYPE_SUBDIR     = 0x0D;
    constexpr uint8_t STORAGE_TYPE_SUBDIR_HDR = 0x0E;
    constexpr uint8_t STORAGE_TYPE_VOLUME_HDR = 0x0F;

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

    /**
     * Write a little-endian 16-bit value to a buffer.
     */
    void writeLittleEndian16(uint8_t* buf, uint16_t value) {
      buf[0] = value & 0xFF;
      buf[1] = (value >> 8) & 0xFF;
    }

    /**
     * Write a little-endian 24-bit value to a buffer.
     */
    void writeLittleEndian24(uint8_t* buf, uint32_t value) {
      buf[0] = value & 0xFF;
      buf[1] = (value >> 8) & 0xFF;
      buf[2] = (value >> 16) & 0xFF;
    }

    /**
     * Struct representing file metadata for directory entry creation.
     */
    struct FileMetadata {
      std::string name;
      uint8_t     storage_type;
      uint8_t     file_type;
      uint16_t    aux_type;
      uint32_t    eof;
      uint16_t    blocks_used;
      time_t      creation_time;
      time_t      mod_time;
      uint8_t     access;
      bool        is_directory;
    };

    /**
     * Create a ProDOS file entry (39 bytes).
     * Returns a 39-byte array with the entry data.
     */
    std::array<uint8_t, 39> createFileEntry(const FileMetadata& meta) {
      std::array<uint8_t, 39> entry;
      std::memset(entry.data(), 0, 39);

      // Storage type and name length (byte 0)
      uint8_t nameLen =
          std::min(static_cast<uint8_t>(meta.name.length()), static_cast<uint8_t>(15));
      entry[0] = (meta.storage_type << 4) | nameLen;

      // File name (bytes 1-15)
      for (uint8_t i = 0; i < nameLen; i++) {
        entry[1 + i] = static_cast<uint8_t>(meta.name[i]);
      }

      // File type (byte 16)
      entry[0x10] = meta.file_type;

      // Key pointer (bytes 17-18) - set to 0 for now
      writeLittleEndian16(&entry[0x11], 0);

      // Blocks used (bytes 19-20)
      writeLittleEndian16(&entry[0x13], meta.blocks_used);

      // EOF (bytes 21-23)
      writeLittleEndian24(&entry[0x15], meta.eof);

      // Creation date/time (bytes 24-27)
      uint16_t createDate = encodeProDOSDate(meta.creation_time);
      uint16_t createTime = encodeProDOSTime(meta.creation_time);
      writeLittleEndian16(&entry[0x18], createDate);
      writeLittleEndian16(&entry[0x1A], createTime);

      // Version / min_version (bytes 28-29)
      entry[0x1C] = 0;
      entry[0x1D] = 0;

      // Access (byte 30)
      entry[0x1E] = meta.access;

      // Aux type (bytes 31-32)
      writeLittleEndian16(&entry[0x1F], meta.aux_type);

      // Last mod date/time (bytes 33-36)
      uint16_t modDate = encodeProDOSDate(meta.mod_time);
      uint16_t modTime = encodeProDOSTime(meta.mod_time);
      writeLittleEndian16(&entry[0x21], modDate);
      writeLittleEndian16(&entry[0x23], modTime);

      // Header pointer (bytes 37-38) - set to 0 for now
      writeLittleEndian16(&entry[0x25], 0);

      return entry;
    }

    /**
     * Create a ProDOS directory header entry (39 bytes).
     * Can be either volume directory header (storage_type 0x0F) or subdirectory header (0x0E).
     */
    std::array<uint8_t, 39> createDirectoryHeaderEntry(const std::string& name, uint16_t fileCount,
                                                       bool isVolume, time_t creationTime,
                                                       uint8_t access) {
      std::array<uint8_t, 39> entry;
      std::memset(entry.data(), 0, 39);

      // Storage type and name length (byte 0)
      uint8_t storage_type = isVolume ? STORAGE_TYPE_VOLUME_HDR : STORAGE_TYPE_SUBDIR_HDR;
      uint8_t nameLen = std::min(static_cast<uint8_t>(name.length()), static_cast<uint8_t>(15));
      entry[0]        = (storage_type << 4) | nameLen;

      // Directory name (bytes 1-15)
      for (uint8_t i = 0; i < nameLen; i++) {
        entry[1 + i] = static_cast<uint8_t>(name[i]);
      }

      // Reserved bytes (bytes 16-23) - set to 0
      std::memset(&entry[0x10], 0, 8);

      // Creation date/time (bytes 24-27)
      uint16_t createDate = encodeProDOSDate(creationTime);
      uint16_t createTime = encodeProDOSTime(creationTime);
      writeLittleEndian16(&entry[0x18], createDate);
      writeLittleEndian16(&entry[0x1A], createTime);

      // Version / min_version (bytes 28-29)
      entry[0x1C] = 0;
      entry[0x1D] = 0;

      // Access (byte 30)
      entry[0x1E] = access;

      // Entry length (byte 31)
      entry[0x1F] = ENTRY_LENGTH;

      // Entries per block (byte 32)
      entry[0x20] = ENTRIES_PER_BLOCK;

      // File count (bytes 33-34)
      writeLittleEndian16(&entry[0x21], fileCount);

      // Bitmap pointer (bytes 35-36) - set to 0
      writeLittleEndian16(&entry[0x23], 0);

      // Total blocks (bytes 37-38) or parent pointer for subdirs
      if (isVolume) {
        writeLittleEndian16(&entry[0x25], 0);  // total_blocks
      } else {
        writeLittleEndian16(&entry[0x25], 0);  // parent_pointer
        // For subdirectory: byte 0x27 = parent_entry_number, 0x28 = parent_entry_length
        // We set these to 0 as they're not critical for read-only use
      }

      return entry;
    }

    /**
     * Build ProDOS directory blocks from a list of file entries.
     * Returns a vector of 512-byte blocks.
     */
    std::vector<std::array<uint8_t, 512>> buildDirectoryBlocks(
        const std::string& dirName, const std::vector<std::array<uint8_t, 39>>& fileEntries,
        bool isVolume, time_t creationTime, uint8_t access) {
      std::vector<std::array<uint8_t, 512>> blocks;

      // Calculate number of blocks needed
      // Key block: header + 12 file entries
      // Subsequent blocks: 13 file entries each
      size_t remainingEntries = fileEntries.size();
      size_t blockCount       = 1;  // at least one block (key block)
      if (remainingEntries > 12) {
        remainingEntries -= 12;
        blockCount += (remainingEntries + 12) / 13;
      }

      // Create blocks
      for (size_t blockIdx = 0; blockIdx < blockCount; blockIdx++) {
        std::array<uint8_t, 512> block;
        std::memset(block.data(), 0, 512);

        // Previous block pointer (bytes 0-1)
        uint16_t prevBlock = (blockIdx > 0) ? static_cast<uint16_t>(blockIdx - 1) : 0;
        writeLittleEndian16(&block[0], prevBlock);

        // Next block pointer (bytes 2-3)
        uint16_t nextBlock = (blockIdx < blockCount - 1) ? static_cast<uint16_t>(blockIdx + 1) : 0;
        writeLittleEndian16(&block[2], nextBlock);

        // Entries start at byte 4
        uint8_t* entryPtr     = &block[4];
        size_t   entryIdx     = 0;
        size_t   entriesInBlk = (blockIdx == 0) ? 12 : 13;  // Key block has header

        // Key block: add header entry first
        if (blockIdx == 0) {
          auto header = createDirectoryHeaderEntry(
              dirName, static_cast<uint16_t>(fileEntries.size()), isVolume, creationTime, access);
          std::memcpy(entryPtr, header.data(), 39);
          entryPtr += 39;
          entryIdx = 1;
        }

        // Add file entries
        size_t globalEntryIdx = (blockIdx == 0) ? 0 : (12 + (blockIdx - 1) * 13);
        while (entryIdx < ENTRIES_PER_BLOCK && globalEntryIdx < fileEntries.size()) {
          std::memcpy(entryPtr, fileEntries[globalEntryIdx].data(), 39);
          entryPtr += 39;
          entryIdx++;
          globalEntryIdx++;
        }

        blocks.push_back(block);
      }

      return blocks;
    }

    /**
     * Synthesize ProDOS directory blocks from a Unix filesystem directory.
     */
    std::vector<std::array<uint8_t, 512>> synthesizeDirectoryBlocks(
        const std::filesystem::path& hostPath, const std::string& dirName, bool isVolume) {
      std::vector<std::array<uint8_t, 39>> fileEntries;

      // Get directory creation time
      time_t      dirCreationTime = ::time(nullptr);
      struct stat dirStat;
      if (::stat(hostPath.string().c_str(), &dirStat) == 0) {
        dirCreationTime = dirStat.st_ctime;
      }

      // Enumerate directory entries
      try {
        for (const auto& entry : std::filesystem::directory_iterator(hostPath)) {
          FileMetadata meta;

          // Get filename (ProDOS uppercase, no path)
          std::string filename = entry.path().filename().string();
          std::transform(filename.begin(), filename.end(), filename.begin(), ::toupper);

          // Skip invalid ProDOS names, limit to 15 chars
          if (filename.empty() || filename.length() > 15) continue;
          meta.name = filename;

          // Get file stats
          struct stat st;
          if (::stat(entry.path().string().c_str(), &st) != 0) continue;

          meta.creation_time = st.st_ctime;
          meta.mod_time      = st.st_mtime;
          meta.is_directory  = std::filesystem::is_directory(entry);

          // Determine storage type and calculate blocks
          if (meta.is_directory) {
            meta.storage_type = STORAGE_TYPE_SUBDIR;
            meta.file_type    = 0x0F;  // Directory file type
            meta.eof          = 512;   // Minimum one block
            meta.blocks_used  = 1;
          } else {
            meta.eof = static_cast<uint32_t>(std::min(st.st_size, static_cast<off_t>(0x00FFFFFF)));
            meta.blocks_used = static_cast<uint16_t>((meta.eof + 511) / 512);

            // Determine storage type based on blocks
            if (meta.blocks_used == 0) {
              meta.storage_type = STORAGE_TYPE_SEEDLING;
              meta.blocks_used =
                  1;  // ProDOS minimum\n            } else if (meta.blocks_used <= 256) {
              meta.storage_type = STORAGE_TYPE_SEEDLING;
            } else {
              meta.storage_type = STORAGE_TYPE_SAPLING;
            }

            // Load file_type and aux_type from xattrs (default to BIN $06 if not found)
            std::string ftypeStr;
            if (prodos8_get_xattr(entry.path().string(), "file_type", ftypeStr) == ERR_NO_ERROR &&
                !ftypeStr.empty()) {
              try {
                meta.file_type = static_cast<uint8_t>(std::stoul(ftypeStr, nullptr, 0));
              } catch (...) {
                meta.file_type = 0x06;  // BIN
              }
            } else {
              meta.file_type = 0x06;  // BIN
            }

            std::string auxStr;
            if (prodos8_get_xattr(entry.path().string(), "aux_type", auxStr) == ERR_NO_ERROR &&
                !auxStr.empty()) {
              try {
                meta.aux_type = static_cast<uint16_t>(std::stoul(auxStr, nullptr, 0));
              } catch (...) {
                meta.aux_type = 0x0000;
              }
            } else {
              meta.aux_type = 0x0000;
            }
          }

          // Load access from xattrs (default to read+write+rename+destroy)
          std::string accessStr;
          if (prodos8_get_xattr(entry.path().string(), "access", accessStr) == ERR_NO_ERROR &&
              !accessStr.empty()) {
            uint8_t accessByte;
            if (parse_access_byte(accessStr, accessByte)) {
              meta.access = accessByte;
            } else {
              meta.access = 0xC3;
            }
          } else {
            meta.access = 0xC3;
          }

          // Create entry and add to list
          fileEntries.push_back(createFileEntry(meta));
        }
      } catch (const std::filesystem::filesystem_error&) {
        // If enumeration fails, return empty directory
      }

      // Sort entries by name (ProDOS convention)
      std::sort(fileEntries.begin(), fileEntries.end(), [](const auto& a, const auto& b) {
        // Compare names (bytes 1-15, length in byte 0 low nibble)
        uint8_t aLen = a[0] & 0x0F;
        uint8_t bLen = b[0] & 0x0F;
        for (uint8_t i = 0; i < std::min(aLen, bLen); i++) {
          if (a[1 + i] != b[1 + i]) {
            return a[1 + i] < b[1 + i];
          }
        }
        return aLen < bLen;
      });

      // Get directory access
      uint8_t     dirAccess = 0xC3;  // default
      std::string dirAccessStr;
      if (prodos8_get_xattr(hostPath.string(), "access", dirAccessStr) == ERR_NO_ERROR &&
          !dirAccessStr.empty()) {
        uint8_t accessByte;
        if (parse_access_byte(dirAccessStr, accessByte)) {
          dirAccess = accessByte;
        }
      }

      // Build directory blocks
      return buildDirectoryBlocks(dirName, fileEntries, isVolume, dirCreationTime, dirAccess);
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

    // Check file exists
    if (!std::filesystem::exists(hostPath)) {
      return ERR_FILE_NOT_FOUND;
    }

    bool isDirectory = std::filesystem::is_directory(hostPath);

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

    // Open file/directory
    int fd;
    if (isDirectory) {
      // Directories are always read-only
      fd = ::open(hostPath.string().c_str(), O_RDONLY);
    } else {
      // Regular files: try R+W first, fall back to R only
      fd = ::open(hostPath.string().c_str(), O_RDWR);
      if (fd < 0) {
        fd = ::open(hostPath.string().c_str(), O_RDONLY);
      }
    }

    if (fd < 0) {
      if (errno == EACCES || errno == EPERM) {
        return ERR_ACCESS_ERROR;
      }
      return ERR_IO_ERROR;
    }

    // Store open file entry
    OpenFile of;
    of.fd             = fd;
    of.mark           = 0;
    of.ioBuffer       = ioBuffer;
    of.newlineEnabled = false;
    of.newlineMask    = 0;
    of.newlineChar    = 0;
    of.isDirectory    = isDirectory;

    // For directories, synthesize ProDOS directory blocks
    if (isDirectory) {
      // Extract directory name from pathname (last component)
      std::string dirName;
      size_t      lastSlash = pathname.find_last_of('/');
      if (lastSlash != std::string::npos && lastSlash + 1 < pathname.length()) {
        dirName = pathname.substr(lastSlash + 1);
      } else {
        dirName = pathname;
      }

      // Determine if this is a volume directory (pathname has only one component after /)
      // For simplicity, we treat all directories as subdirectories
      bool isVolume = false;
      if (lastSlash == 0 && pathname.find('/', 1) == std::string::npos) {
        // Root-level directory (/VOLUME) - treat as volume
        isVolume = true;
      }

      // Synthesize directory blocks
      of.directoryBlocks = synthesizeDirectoryBlocks(hostPath, dirName, isVolume);
    }

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

    OpenFile& of = it->second;

    // For directories, read from synthesized ProDOS directory blocks
    if (of.isDirectory) {
      uint32_t dirEof = static_cast<uint32_t>(of.directoryBlocks.size() * 512);

      // Check if already at EOF
      if (of.mark >= dirEof) {
        return ERR_EOF_ENCOUNTERED;
      }

      // Calculate how many bytes we can read
      uint32_t bytesAvailable = dirEof - of.mark;
      uint16_t bytesToRead =
          static_cast<uint16_t>(std::min(static_cast<uint32_t>(requestCount), bytesAvailable));

      // Read from directory blocks
      uint16_t bytesRead = 0;
      while (bytesRead < bytesToRead) {
        uint32_t blockNum    = of.mark / 512;
        uint32_t offsetInBlk = of.mark % 512;
        uint32_t bytesInBlk  = std::min(static_cast<uint32_t>(512 - offsetInBlk),
                                        static_cast<uint32_t>(bytesToRead - bytesRead));

        // Copy from block to data buffer
        for (uint32_t i = 0; i < bytesInBlk; i++) {
          write_u8(banks, static_cast<uint16_t>(dataBuf + bytesRead + i),
                   of.directoryBlocks[blockNum][offsetInBlk + i]);
        }

        bytesRead += static_cast<uint16_t>(bytesInBlk);
        of.mark += bytesInBlk;
      }

      // Update trans_count
      write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6), bytesRead);

      // Return EOF only if no bytes were read
      if (bytesRead == 0) {
        return ERR_EOF_ENCOUNTERED;
      }

      return ERR_NO_ERROR;
    }

    // Regular file reading
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

    // Directories are read-only
    if (it->second.isDirectory) {
      return ERR_ACCESS_ERROR;
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
