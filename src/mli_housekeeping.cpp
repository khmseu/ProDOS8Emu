#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include "prodos8emu/access_byte.hpp"
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
     * Convert ProDOS date/time to ISO 8601 UTC string.
     * Returns YYYY-MM-DDTHH:MM:SSZ format.
     * If date is 0, returns current time.
     */
    std::string proDOSDateTimeToISO8601(uint16_t date, uint16_t time) {
      time_t     timestamp = decodeProDOSDateTime(date, time);
      struct tm* t         = gmtime(&timestamp);
      if (!t) {
        // Fallback to current time
        time_t now = ::time(nullptr);
        t          = gmtime(&now);
      }

      std::ostringstream oss;
      oss << std::setfill('0') << std::setw(4) << (t->tm_year + 1900) << '-' << std::setw(2)
          << (t->tm_mon + 1) << '-' << std::setw(2) << t->tm_mday << 'T' << std::setw(2)
          << t->tm_hour << ':' << std::setw(2) << t->tm_min << ':' << std::setw(2) << t->tm_sec
          << 'Z';
      return oss.str();
    }

    /**
     * Parse ISO 8601 UTC string to Unix timestamp.
     * Expected format: YYYY-MM-DDTHH:MM:SSZ (exactly 20 characters).
     * Returns true on success, false if malformed.
     */
    bool parseISO8601(const std::string& str, time_t& result) {
      // Validate format: YYYY-MM-DDTHH:MM:SSZ
      if (str.length() != 20) return false;
      if (str[4] != '-' || str[7] != '-' || str[10] != 'T' || str[13] != ':' || str[16] != ':' ||
          str[19] != 'Z') {
        return false;
      }

      // Parse components
      char* endPtr = nullptr;

      // Year: positions 0-3
      long year = std::strtol(str.substr(0, 4).c_str(), &endPtr, 10);
      if (endPtr == nullptr || *endPtr != '\0' || year < 1900 || year > 3000) return false;

      // Month: positions 5-6
      long month = std::strtol(str.substr(5, 2).c_str(), &endPtr, 10);
      if (endPtr == nullptr || *endPtr != '\0' || month < 1 || month > 12) return false;

      // Day: positions 8-9
      long day = std::strtol(str.substr(8, 2).c_str(), &endPtr, 10);
      if (endPtr == nullptr || *endPtr != '\0' || day < 1 || day > 31) return false;

      // Hour: positions 11-12
      long hour = std::strtol(str.substr(11, 2).c_str(), &endPtr, 10);
      if (endPtr == nullptr || *endPtr != '\0' || hour < 0 || hour > 23) return false;

      // Minute: positions 14-15
      long minute = std::strtol(str.substr(14, 2).c_str(), &endPtr, 10);
      if (endPtr == nullptr || *endPtr != '\0' || minute < 0 || minute > 59) return false;

      // Second: positions 17-18
      long second = std::strtol(str.substr(17, 2).c_str(), &endPtr, 10);
      if (endPtr == nullptr || *endPtr != '\0' || second < 0 || second > 59) return false;

      // Build tm struct for UTC time
      struct tm t = {};
      t.tm_year   = static_cast<int>(year - 1900);
      t.tm_mon    = static_cast<int>(month - 1);
      t.tm_mday   = static_cast<int>(day);
      t.tm_hour   = static_cast<int>(hour);
      t.tm_min    = static_cast<int>(minute);
      t.tm_sec    = static_cast<int>(second);
      t.tm_isdst  = 0;  // UTC has no DST

      // Convert to time_t using timegm (UTC)
      // timegm is available on Linux/macOS; for other platforms we need a portable fallback
      result = ::timegm(&t);

      return (result != static_cast<time_t>(-1));
    }

    /**
     * Format uint8_t as 2 lowercase hex characters.
     */
    std::string formatHexByte(uint8_t value) {
      std::ostringstream oss;
      oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(value);
      return oss.str();
    }

    /**
     * Format uint16_t as 4 lowercase hex characters.
     */
    std::string formatHexWord(uint16_t value) {
      std::ostringstream oss;
      oss << std::hex << std::setfill('0') << std::setw(4) << value;
      return oss.str();
    }

    /**
     * Parse 2 hex characters to uint8_t.
     * Returns true on success, false if invalid.
     */
    bool parseHexByte(const std::string& str, uint8_t& result) {
      if (str.length() != 2) return false;
      char*         endPtr = nullptr;
      unsigned long val    = std::strtoul(str.c_str(), &endPtr, 16);
      if (endPtr == nullptr || *endPtr != '\0' || val > 0xFF) return false;
      result = static_cast<uint8_t>(val);
      return true;
    }

    /**
     * Parse 4 hex characters to uint16_t.
     * Returns true on success, false if invalid.
     */
    bool parseHexWord(const std::string& str, uint16_t& result) {
      if (str.length() != 4) return false;
      char*         endPtr = nullptr;
      unsigned long val    = std::strtoul(str.c_str(), &endPtr, 16);
      if (endPtr == nullptr || *endPtr != '\0' || val > 0xFFFF) return false;
      result = static_cast<uint16_t>(val);
      return true;
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
      // Write separate xattrs for each field
      uint8_t err;

      // access: 8-character string from format_access_byte
      std::string accessStr = format_access_byte(meta.access);
      err                   = prodos8_set_xattr(hostPath.string(), "access", accessStr);
      if (err != ERR_NO_ERROR) return err;

      // file_type: 2 lowercase hex chars
      err = prodos8_set_xattr(hostPath.string(), "file_type", formatHexByte(meta.file_type));
      if (err != ERR_NO_ERROR) return err;

      // aux_type: 4 lowercase hex chars
      err = prodos8_set_xattr(hostPath.string(), "aux_type", formatHexWord(meta.aux_type));
      if (err != ERR_NO_ERROR) return err;

      // storage_type: 2 lowercase hex chars
      err = prodos8_set_xattr(hostPath.string(), "storage_type", formatHexByte(meta.storage_type));
      if (err != ERR_NO_ERROR) return err;

      // created: ISO 8601 UTC string from create_date/create_time
      std::string createdStr = proDOSDateTimeToISO8601(meta.create_date, meta.create_time);
      err                    = prodos8_set_xattr(hostPath.string(), "created", createdStr);
      if (err != ERR_NO_ERROR) return err;

      return ERR_NO_ERROR;
    }

    /**
     * Load ProDOS metadata from xattrs, or provide defaults.
     * Robustly handles malformed xattr data by falling back to stat-based defaults.
     */
    ProDOSMetadata loadMetadata(const std::filesystem::path& hostPath, bool isDirectory) {
      ProDOSMetadata meta = {};

      // Try to read each xattr independently
      std::string value;
      bool        accessLoaded      = false;
      bool        fileTypeLoaded    = false;
      bool        auxTypeLoaded     = false;
      bool        storageTypeLoaded = false;

      // Read access xattr
      if (prodos8_get_xattr(hostPath.string(), "access", value) == ERR_NO_ERROR) {
        uint8_t accessByte;
        if (parse_access_byte(value, accessByte)) {
          meta.access  = accessByte;
          accessLoaded = true;
        }
      }

      // Read file_type xattr
      if (prodos8_get_xattr(hostPath.string(), "file_type", value) == ERR_NO_ERROR) {
        uint8_t fileTypeByte;
        if (parseHexByte(value, fileTypeByte)) {
          meta.file_type = fileTypeByte;
          fileTypeLoaded = true;
        }
      }

      // Read aux_type xattr
      if (prodos8_get_xattr(hostPath.string(), "aux_type", value) == ERR_NO_ERROR) {
        uint16_t auxTypeWord;
        if (parseHexWord(value, auxTypeWord)) {
          meta.aux_type = auxTypeWord;
          auxTypeLoaded = true;
        }
      }

      // Read storage_type xattr
      if (prodos8_get_xattr(hostPath.string(), "storage_type", value) == ERR_NO_ERROR) {
        uint8_t storageTypeByte;
        if (parseHexByte(value, storageTypeByte)) {
          meta.storage_type = storageTypeByte;
          storageTypeLoaded = true;
        }
      }

      // Get filesystem stats for defaults
      struct stat st;
      bool        haveStat = (stat(hostPath.string().c_str(), &st) == 0);

      // Fill in missing fields with defaults
      if (!accessLoaded) {
        meta.access = 0xC3;  // Default: read+write+rename+destroy
        if (haveStat) {
          if (!(st.st_mode & S_IWUSR)) {
            meta.access &= ~0x02;  // Clear write bit
          }
          if (!(st.st_mode & S_IRUSR)) {
            meta.access &= ~0x01;  // Clear read bit
          }
        }
      }

      if (!fileTypeLoaded) {
        meta.file_type = isDirectory ? 0x0F : 0x00;
      }

      if (!auxTypeLoaded) {
        meta.aux_type = 0x0000;
      }

      if (!storageTypeLoaded) {
        meta.storage_type = isDirectory ? 0x0D : 0x01;
      }

      // Set create_date/create_time from "created" xattr if present, otherwise fall back to stat
      time_t createTimestamp;
      bool   haveCreatedXattr = false;

      if (prodos8_get_xattr(hostPath.string(), "created", value) == ERR_NO_ERROR) {
        if (parseISO8601(value, createTimestamp)) {
          meta.create_date = encodeProDOSDate(createTimestamp);
          meta.create_time = encodeProDOSTime(createTimestamp);
          haveCreatedXattr = true;
        }
      }

      // If no valid created xattr, fall back to mtime or now
      if (!haveCreatedXattr) {
        if (haveStat) {
          meta.create_date = encodeProDOSDate(st.st_mtime);
          meta.create_time = encodeProDOSTime(st.st_mtime);
        } else {
          time_t now       = ::time(nullptr);
          meta.create_date = encodeProDOSDate(now);
          meta.create_time = encodeProDOSTime(now);
        }
      }

      // Always set mod_date/mod_time from stat mtime
      if (haveStat) {
        meta.mod_date = encodeProDOSDate(st.st_mtime);
        meta.mod_time = encodeProDOSTime(st.st_mtime);
      } else {
        // If no stat available, use create time
        meta.mod_date = meta.create_date;
        meta.mod_time = meta.create_time;
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
      time_t          mtime = decodeProDOSDateTime(mod_date, mod_time);
      struct timespec times[2];
      times[0].tv_sec  = mtime;  // atime
      times[0].tv_nsec = 0;
      times[1].tv_sec  = mtime;  // mtime
      times[1].tv_nsec = 0;
      if (utimensat(AT_FDCWD, hostPath.string().c_str(), times, 0) == -1) {
        if (errno == EACCES || errno == EPERM) {
          return ERR_ACCESS_ERROR;
        }
        return ERR_IO_ERROR;
      }
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
