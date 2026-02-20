#include <sys/stat.h>
#include <utime.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"
#include "prodos8emu/path.hpp"
#include "prodos8emu/xattr.hpp"

namespace prodos8emu {

  namespace {

    // ProDOS date/time encoding/decoding helpers
    // Date format: bits 0-4: day (1-31), bits 5-8: month (1-12), bits 9-15: year (0-127, offset
    // from 1900) Time format: bits 0-5: minute (0-59), bits 8-12: hour (0-23)

    /**
     * Encode a Unix timestamp to ProDOS date word.
     */
    uint16_t encodeProDOSDate(time_t timestamp) {
      struct tm* t = localtime(&timestamp);
      if (!t) return 0;

      int day   = t->tm_mday;     // 1-31
      int month = t->tm_mon + 1;  // 0-11 -> 1-12
      int year  = t->tm_year;     // years since 1900

      // Clamp values
      if (day < 1) day = 1;
      if (day > 31) day = 31;
      if (month < 1) month = 1;
      if (month > 12) month = 12;
      if (year < 0) year = 0;
      if (year > 127) year = 127;

      return static_cast<uint16_t>((day & 0x1F) | ((month & 0x0F) << 5) | ((year & 0x7F) << 9));
    }

    /**
     * Encode a Unix timestamp to ProDOS time word.
     */
    uint16_t encodeProDOSTime(time_t timestamp) {
      struct tm* t = localtime(&timestamp);
      if (!t) return 0;

      int minute = t->tm_min;   // 0-59
      int hour   = t->tm_hour;  // 0-23

      // Clamp values
      if (minute < 0) minute = 0;
      if (minute > 59) minute = 59;
      if (hour < 0) hour = 0;
      if (hour > 23) hour = 23;

      return static_cast<uint16_t>((minute & 0x3F) | ((hour & 0x1F) << 8));
    }

    /**
     * Decode ProDOS date and time words to Unix timestamp.
     */
    time_t decodeProDOSDateTime(uint16_t date, uint16_t time) {
      if (date == 0) {
        // Use current time as default
        return ::time(nullptr);
      }

      int day   = date & 0x1F;
      int month = (date >> 5) & 0x0F;
      int year  = (date >> 9) & 0x7F;

      int minute = time & 0x3F;
      int hour   = (time >> 8) & 0x1F;

      struct tm t = {};
      t.tm_mday   = day;
      t.tm_mon    = month - 1;  // ProDOS uses 1-12, tm_mon uses 0-11
      t.tm_year   = year;       // ProDOS year is already offset from 1900
      t.tm_hour   = hour;
      t.tm_min    = minute;
      t.tm_sec    = 0;
      t.tm_isdst  = -1;  // Let mktime determine DST

      return mktime(&t);
    }

    /**
     * Metadata structure for ProDOS file attributes.
     */
    struct ProDOSMetadata {
      uint8_t  access;
      uint8_t  file_type;
      uint16_t aux_type;
      uint8_t  storage_type;
      uint16_t create_date;
      uint16_t create_time;
      uint16_t mod_date;
      uint16_t mod_time;
    };

    /**
     * Store ProDOS metadata to xattrs.
     */
    uint8_t storeMetadata(const std::filesystem::path& hostPath, const ProDOSMetadata& meta) {
      std::ostringstream oss;
      oss << std::hex << std::setfill('0');
      oss << std::setw(2) << (int)meta.access << ":" << std::setw(2) << (int)meta.file_type << ":"
          << std::setw(4) << meta.aux_type << ":" << std::setw(2) << (int)meta.storage_type << ":"
          << std::setw(4) << meta.create_date << ":" << std::setw(4) << meta.create_time << ":"
          << std::setw(4) << meta.mod_date << ":" << std::setw(4) << meta.mod_time;

      uint8_t err = prodos8_set_xattr(hostPath.string(), "metadata", oss.str());
      if (err != ERR_NO_ERROR) {
        // If xattrs not supported, we can still proceed for some operations
        // Return error for now as per spec
        return err;
      }

      return ERR_NO_ERROR;
    }

    /**
     * Load ProDOS metadata from xattrs, or provide defaults.
     * Robustly handles malformed xattr data by falling back to stat-based defaults.
     */
    ProDOSMetadata loadMetadata(const std::filesystem::path& hostPath, bool isDirectory) {
      ProDOSMetadata meta           = {};
      bool           metadataLoaded = false;

      std::string value;
      uint8_t     err = prodos8_get_xattr(hostPath.string(), "metadata", value);

      if (err == ERR_NO_ERROR && !value.empty()) {
        // Robust tokenization: split on ':' into exactly 8 fields
        std::vector<std::string> fields;
        std::string::size_type   start = 0;
        std::string::size_type   end;

        while ((end = value.find(':', start)) != std::string::npos) {
          fields.push_back(value.substr(start, end - start));
          start = end + 1;
        }
        fields.push_back(value.substr(start));  // Last field

        // Must have exactly 8 fields
        if (fields.size() == 8) {
          // Parse each field as hex into unsigned long and validate
          unsigned long parsed[8];
          bool          parseSuccess = true;

          for (size_t i = 0; i < 8; i++) {
            char* endPtr = nullptr;
            parsed[i]    = std::strtoul(fields[i].c_str(), &endPtr, 16);
            // Validate: endPtr should point to end of string (full parse)
            if (endPtr == nullptr || *endPtr != '\0' || fields[i].empty()) {
              parseSuccess = false;
              break;
            }
          }

          if (parseSuccess) {
            meta.access       = static_cast<uint8_t>(parsed[0]);
            meta.file_type    = static_cast<uint8_t>(parsed[1]);
            meta.aux_type     = static_cast<uint16_t>(parsed[2]);
            meta.storage_type = static_cast<uint8_t>(parsed[3]);
            meta.create_date  = static_cast<uint16_t>(parsed[4]);
            meta.create_time  = static_cast<uint16_t>(parsed[5]);
            meta.mod_date     = static_cast<uint16_t>(parsed[6]);
            meta.mod_time     = static_cast<uint16_t>(parsed[7]);
            metadataLoaded    = true;
          }
        }
      }

      if (!metadataLoaded) {
        // Provide defaults based on file system attributes
        struct stat st;
        if (stat(hostPath.string().c_str(), &st) == 0) {
          // Derive access from Linux permissions
          meta.access = 0xC3;  // Default: read+write+rename+destroy
          if (!(st.st_mode & S_IWUSR)) {
            meta.access &= ~0x02;  // Clear write bit
          }
          if (!(st.st_mode & S_IRUSR)) {
            meta.access &= ~0x01;  // Clear read bit
          }

          // Set storage type
          if (isDirectory) {
            meta.storage_type = 0x0D;  // Directory
            meta.file_type    = 0x0F;  // Directory file type
          } else {
            meta.storage_type = 0x01;  // Standard file
            meta.file_type    = 0x00;  // Default: typeless file
          }

          meta.aux_type = 0x0000;

          // Use mtime for both create and mod times
          meta.create_date = encodeProDOSDate(st.st_mtime);
          meta.create_time = encodeProDOSTime(st.st_mtime);
          meta.mod_date    = encodeProDOSDate(st.st_mtime);
          meta.mod_time    = encodeProDOSTime(st.st_mtime);
        } else {
          // Fallback defaults
          meta.access       = 0xC3;
          meta.file_type    = isDirectory ? 0x0F : 0x00;
          meta.aux_type     = 0x0000;
          meta.storage_type = isDirectory ? 0x0D : 0x01;

          time_t now       = ::time(nullptr);
          meta.create_date = encodeProDOSDate(now);
          meta.create_time = encodeProDOSTime(now);
          meta.mod_date    = meta.create_date;
          meta.mod_time    = meta.create_time;
        }
      }

      return meta;
    }

    /**
     * Apply access bits to file permissions.
     */
    void applyAccessToPermissions(const std::filesystem::path& hostPath, uint8_t access) {
      struct stat st;
      if (stat(hostPath.string().c_str(), &st) != 0) {
        return;  // Can't get current permissions
      }

      mode_t mode = st.st_mode;

      // ProDOS access bits: bit 0 = read, bit 1 = write
      if (access & 0x01) {
        mode |= S_IRUSR;
      } else {
        mode &= ~S_IRUSR;
      }

      if (access & 0x02) {
        mode |= S_IWUSR;
      } else {
        mode &= ~S_IWUSR;
      }

      chmod(hostPath.string().c_str(), mode);
    }

    /**
     * Read pathname from parameter block with length validation.
     * Validates that the counted string length is <= 64 bytes.
     */
    std::string readPathname(const ConstMemoryBanks& banks, uint16_t paramBlockAddr,
                             uint16_t offset) {
      uint16_t pathnamePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + offset));
      // Check length before reading
      uint8_t length = read_u8(banks, pathnamePtr);
      if (length > 64) {
        return "";  // Signal error with empty string
      }
      return readNormalizedCountedString(banks, pathnamePtr);
    }

    /**
     * Read pathname from parameter block (mutable banks version) with length validation.
     */
    std::string readPathname(const MemoryBanks& banks, uint16_t paramBlockAddr, uint16_t offset) {
      uint16_t pathnamePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + offset));
      // Convert MemoryBanks to ConstMemoryBanks
      ConstMemoryBanks constBanks;
      for (size_t i = 0; i < NUM_BANKS; i++) {
        constBanks[i] = banks[i];
      }
      // Check length before reading
      uint8_t length = read_u8(constBanks, pathnamePtr);
      if (length > 64) {
        return "";  // Signal error with empty string
      }
      return readNormalizedCountedString(constBanks, pathnamePtr);
    }

  }  // anonymous namespace

  uint8_t MLIContext::createCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    // Read parameter block
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 7) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    std::string pathname = readPathname(banks, paramBlockAddr, 1);
    if (pathname.empty()) {
      return ERR_INVALID_PATH_SYNTAX;  // readPathname returns empty on >64 byte input
    }

    uint8_t  access       = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 3));
    uint8_t  file_type    = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 4));
    uint16_t aux_type     = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 5));
    uint8_t  storage_type = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 7));
    uint16_t create_date  = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 8));
    uint16_t create_time  = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 10));

    // Validate storage type
    if (storage_type != 0x01 && storage_type != 0x0D) {
      return ERR_UNSUPPORTED_STOR_TYPE;
    }

    // Validate and resolve path
    if (pathname[0] != '/') {
      // Partial path - resolve with prefix
      pathname = resolveFullPath(pathname, m_prefix);
      if (pathname.empty()) {
        return ERR_INVALID_PATH_SYNTAX;  // Result too long
      }
      // After resolution, must be absolute
      if (pathname[0] != '/') {
        return ERR_INVALID_PATH_SYNTAX;  // Empty prefix + relative = invalid
      }
    }

    if (!isValidPathname(pathname, 128)) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Map to host path
    std::filesystem::path hostPath = mapToHostPath(pathname, m_volumesRoot);

    // Check if already exists
    if (std::filesystem::exists(hostPath)) {
      return ERR_DUPLICATE_FILENAME;
    }

    // Check if parent directory exists
    std::filesystem::path parentPath = hostPath.parent_path();
    if (!std::filesystem::exists(parentPath)) {
      return ERR_PATH_NOT_FOUND;
    }

    // Create file or directory
    std::error_code ec;
    if (storage_type == 0x0D) {
      // Create directory
      std::filesystem::create_directory(hostPath, ec);
      if (ec) {
        if (ec.value() == EACCES || ec.value() == EPERM) {
          return ERR_ACCESS_ERROR;
        } else if (ec.value() == ENOSPC) {
          return ERR_VOLUME_FULL;
        }
        return ERR_IO_ERROR;
      }
    } else {
      // Create empty file
      std::ofstream ofs(hostPath);
      if (!ofs) {
        // Check errno for specific errors
        if (errno == EACCES || errno == EPERM) {
          return ERR_ACCESS_ERROR;
        } else if (errno == ENOSPC) {
          return ERR_VOLUME_FULL;
        }
        return ERR_IO_ERROR;
      }
      ofs.close();
    }

    // Set permissions based on access
    applyAccessToPermissions(hostPath, access);

    // Store metadata
    ProDOSMetadata meta;
    meta.access       = access;
    meta.file_type    = file_type;
    meta.aux_type     = aux_type;
    meta.storage_type = storage_type;

    // Use provided dates or current time
    if (create_date == 0 || create_time == 0) {
      time_t now       = ::time(nullptr);
      meta.create_date = encodeProDOSDate(now);
      meta.create_time = encodeProDOSTime(now);
    } else {
      meta.create_date = create_date;
      meta.create_time = create_time;
    }

    meta.mod_date = meta.create_date;
    meta.mod_time = meta.create_time;

    uint8_t err = storeMetadata(hostPath, meta);
    if (err != ERR_NO_ERROR) {
      // Note: If xattrs fail, we still created the file/dir
      // Per spec, return error if xattr set fails
      return err;
    }

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::destroyCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    // Read parameter block
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 1) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    std::string pathname = readPathname(banks, paramBlockAddr, 1);
    if (pathname.empty()) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Validate and resolve path
    if (pathname[0] != '/') {
      pathname = resolveFullPath(pathname, m_prefix);
      if (pathname.empty()) {
        return ERR_INVALID_PATH_SYNTAX;
      }
      if (pathname[0] != '/') {
        return ERR_INVALID_PATH_SYNTAX;
      }
    }

    if (!isValidPathname(pathname, 128)) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Map to host path
    std::filesystem::path hostPath = mapToHostPath(pathname, m_volumesRoot);

    // Check if exists
    if (!std::filesystem::exists(hostPath)) {
      return ERR_FILE_NOT_FOUND;
    }

    // Check if directory is empty
    std::error_code ec;
    if (std::filesystem::is_directory(hostPath)) {
      if (!std::filesystem::is_empty(hostPath, ec)) {
        // Non-empty directory - map to access error
        return ERR_ACCESS_ERROR;
      }
    }

    // Remove file or directory
    std::filesystem::remove(hostPath, ec);
    if (ec) {
      if (ec.value() == EACCES || ec.value() == EPERM) {
        return ERR_ACCESS_ERROR;
      }
      return ERR_IO_ERROR;
    }

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::renameCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    // Read parameter block
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    std::string pathname    = readPathname(banks, paramBlockAddr, 1);
    std::string newPathname = readPathname(banks, paramBlockAddr, 3);
    if (pathname.empty() || newPathname.empty()) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Validate and resolve paths
    if (pathname[0] != '/') {
      pathname = resolveFullPath(pathname, m_prefix);
      if (pathname.empty() || pathname[0] != '/') {
        return ERR_INVALID_PATH_SYNTAX;
      }
    }

    if (newPathname[0] != '/') {
      newPathname = resolveFullPath(newPathname, m_prefix);
      if (newPathname.empty() || newPathname[0] != '/') {
        return ERR_INVALID_PATH_SYNTAX;
      }
    }

    if (!isValidPathname(pathname, 128) || !isValidPathname(newPathname, 128)) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Check that directory parts match (RENAME restriction)
    size_t lastSlashOld = pathname.rfind('/');
    size_t lastSlashNew = newPathname.rfind('/');

    if (lastSlashOld == std::string::npos || lastSlashNew == std::string::npos) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    std::string oldDir = pathname.substr(0, lastSlashOld);
    std::string newDir = newPathname.substr(0, lastSlashNew);

    if (oldDir != newDir) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Map to host paths
    std::filesystem::path oldHostPath = mapToHostPath(pathname, m_volumesRoot);
    std::filesystem::path newHostPath = mapToHostPath(newPathname, m_volumesRoot);

    // Check if old exists
    if (!std::filesystem::exists(oldHostPath)) {
      return ERR_FILE_NOT_FOUND;
    }

    // Check if new already exists
    if (std::filesystem::exists(newHostPath)) {
      return ERR_DUPLICATE_FILENAME;
    }

    // Perform rename
    std::error_code ec;
    std::filesystem::rename(oldHostPath, newHostPath, ec);
    if (ec) {
      if (ec.value() == EACCES || ec.value() == EPERM) {
        return ERR_ACCESS_ERROR;
      }
      return ERR_IO_ERROR;
    }

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::setFileInfoCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    // Read parameter block
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 7) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    std::string pathname = readPathname(banks, paramBlockAddr, 1);
    if (pathname.empty()) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    uint8_t  access    = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 3));
    uint8_t  file_type = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 4));
    uint16_t aux_type  = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 5));
    // Bytes +7 to +9 are null_field (ignored)
    uint16_t mod_date = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 10));
    uint16_t mod_time = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 12));

    // Validate and resolve path
    if (pathname[0] != '/') {
      pathname = resolveFullPath(pathname, m_prefix);
      if (pathname.empty() || pathname[0] != '/') {
        return ERR_INVALID_PATH_SYNTAX;
      }
    }

    if (!isValidPathname(pathname, 128)) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Map to host path
    std::filesystem::path hostPath = mapToHostPath(pathname, m_volumesRoot);

    // Check if exists
    if (!std::filesystem::exists(hostPath)) {
      return ERR_FILE_NOT_FOUND;
    }

    // Load existing metadata to preserve create date/time and storage_type
    bool           isDir = std::filesystem::is_directory(hostPath);
    ProDOSMetadata meta  = loadMetadata(hostPath, isDir);

    // Update fields
    meta.access    = access;
    meta.file_type = file_type;
    meta.aux_type  = aux_type;
    meta.mod_date  = mod_date;
    meta.mod_time  = mod_time;

    // Apply access to file permissions
    applyAccessToPermissions(hostPath, access);

    // Update mtime if mod_date/time specified
    if (mod_date != 0 && mod_time != 0) {
      time_t         mtime = decodeProDOSDateTime(mod_date, mod_time);
      struct utimbuf times;
      times.actime  = mtime;
      times.modtime = mtime;
      utime(hostPath.string().c_str(), &times);
    }

    // Store updated metadata
    uint8_t err = storeMetadata(hostPath, meta);
    if (err != ERR_NO_ERROR) {
      return err;
    }

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::getFileInfoCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    // Read parameter block
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 0x0A) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    std::string pathname = readPathname(banks, paramBlockAddr, 1);
    if (pathname.empty()) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Validate and resolve path
    if (pathname[0] != '/') {
      pathname = resolveFullPath(pathname, m_prefix);
      if (pathname.empty() || pathname[0] != '/') {
        return ERR_INVALID_PATH_SYNTAX;
      }
    }

    if (!isValidPathname(pathname, 128)) {
      return ERR_INVALID_PATH_SYNTAX;
    }

    // Map to host path
    std::filesystem::path hostPath = mapToHostPath(pathname, m_volumesRoot);

    // Check if exists
    if (!std::filesystem::exists(hostPath)) {
      return ERR_FILE_NOT_FOUND;
    }

    // Get file info
    std::error_code ec;
    bool            isDir    = std::filesystem::is_directory(hostPath, ec);
    auto            fileSize = isDir ? 0 : std::filesystem::file_size(hostPath, ec);

    // Load metadata
    ProDOSMetadata meta = loadMetadata(hostPath, isDir);

    // Calculate blocks used (512 bytes per block)
    uint16_t blocks_used = static_cast<uint16_t>((fileSize + 511) / 512);

    // Check if this is a volume root directory
    // A volume root is a directory that is an immediate child of volumesRoot
    if (isDir) {
      auto            parentPath = hostPath.parent_path();
      std::error_code parentEc;
      if (std::filesystem::equivalent(parentPath, m_volumesRoot, parentEc) && !parentEc) {
        meta.storage_type = 0x0F;  // Volume directory
      }
    }

    // Write results to parameter block
    write_u8(banks, static_cast<uint16_t>(paramBlockAddr + 3), meta.access);
    write_u8(banks, static_cast<uint16_t>(paramBlockAddr + 4), meta.file_type);
    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 5), meta.aux_type);
    write_u8(banks, static_cast<uint16_t>(paramBlockAddr + 7), meta.storage_type);
    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 8), blocks_used);
    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 10), meta.mod_date);
    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 12), meta.mod_time);
    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 14), meta.create_date);
    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 16), meta.create_time);

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::onLineCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    // Read parameter block
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t  unit_num    = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    uint16_t data_buffer = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));

    // Enumerate volumes (immediate subdirectories of volumesRoot)
    std::error_code          ec;
    std::vector<std::string> volumes;

    for (const auto& entry : std::filesystem::directory_iterator(m_volumesRoot, ec)) {
      if (entry.is_directory(ec)) {
        std::string name = entry.path().filename().string();
        // Validate as ProDOS volume name
        if (isValidComponent(name)) {
          volumes.push_back(name);
        }
      }
    }

    // Sort volumes for deterministic order
    std::sort(volumes.begin(), volumes.end());

    // Handle specific unit_num (not 0)
    if (unit_num != 0) {
      // Extract slot and drive from unit_num
      // unit_num format: bits 7=drive, bits 6-4=slot
      int drive = (unit_num >> 7) & 0x01;
      int slot  = (unit_num >> 4) & 0x07;

      // Calculate which volume index this corresponds to
      // Our mapping: slot N drive D -> volume index (N-1)*2 + D
      if (slot < 1 || slot > 7) {
        return ERR_NO_DEVICE;
      }

      int volumeIndex = (slot - 1) * 2 + drive;

      // Check if this volume exists
      if (volumeIndex >= static_cast<int>(volumes.size())) {
        return ERR_NO_DEVICE;
      }

      // Write single record for this volume
      const std::string& volName = volumes[volumeIndex];
      if (volName.length() > 15) {
        return ERR_NO_DEVICE;  // Volume name too long
      }

      uint8_t byte0 = static_cast<uint8_t>((drive << 7) | (slot << 4) | volName.length());
      write_u8(banks, data_buffer, byte0);

      // Write volume name
      for (size_t j = 0; j < volName.length(); j++) {
        write_u8(banks, static_cast<uint16_t>(data_buffer + 1 + j),
                 static_cast<uint8_t>(volName[j]));
      }

      // Pad rest of record with zeros
      for (size_t j = volName.length(); j < 15; j++) {
        write_u8(banks, static_cast<uint16_t>(data_buffer + 1 + j), 0);
      }

      return ERR_NO_ERROR;
    }

    // unit_num == 0: return all volumes (up to 14 + terminator)
    // Write records to data_buffer
    // Each record is 16 bytes
    // Record[0]: bits 7=drive (0/1), 6-4=slot (1-7), 3-0=name_len
    // Record[1-15]: volume name (NOT prefixed with /)

    uint16_t bufferOffset = data_buffer;
    int      recordCount  = 0;
    size_t   maxVolumes   = std::min(
        volumes.size(),
        static_cast<size_t>(14));  // Limit to 14 volumes (slots 1-7, drives 0-1) + terminator

    for (size_t i = 0; i < maxVolumes; i++) {
      const std::string& volName = volumes[i];
      if (volName.length() > 15) continue;  // Skip if too long

      // Synthesize slot/drive: Slots 1-7 with drives 0-1 gives 14 possible combinations
      int slot  = (recordCount / 2) + 1;
      int drive = recordCount % 2;

      uint8_t byte0 = static_cast<uint8_t>((drive << 7) | (slot << 4) | volName.length());
      write_u8(banks, bufferOffset, byte0);

      // Write volume name
      for (size_t j = 0; j < volName.length(); j++) {
        write_u8(banks, static_cast<uint16_t>(bufferOffset + 1 + j),
                 static_cast<uint8_t>(volName[j]));
      }

      // Pad rest of record with zeros
      for (size_t j = volName.length(); j < 15; j++) {
        write_u8(banks, static_cast<uint16_t>(bufferOffset + 1 + j), 0);
      }

      bufferOffset += 16;
      recordCount++;
    }

    // Write terminator record (byte0 = 0)
    write_u8(banks, bufferOffset, 0);

    return ERR_NO_ERROR;
  }

}  // namespace prodos8emu
