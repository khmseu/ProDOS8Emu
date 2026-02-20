#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "memory.hpp"

namespace prodos8emu {

  /**
   * ProDOS pathname parsing and validation utilities.
   *
   * ProDOS pathnames follow specific rules:
   * - Counted string format (length byte + data)
   * - Max 64 bytes for individual pathname input or stored prefix
   * - Max 128 bytes for full resolved path (after prefix + partial resolution)
   * - Components separated by '/'
   * - Full path starts with '/', partial path does not
   * - Component naming rules:
   *   - Must start with A-Z (after normalization)
   *   - Can contain A-Z, 0-9, '.' only
   *   - Length 1-15 characters
   * - Input normalization:
   *   - Clear high bit (ch & 0x7F)
   *   - Uppercase a-z to A-Z
   */

  /**
   * Normalize a ProDOS character: clear high bit and uppercase.
   *
   * @param ch Character to normalize
   * @return Normalized character
   */
  inline char normalizeChar(char ch) {
    char normalized = ch & 0x7F;  // Clear high bit
    if (normalized >= 'a' && normalized <= 'z') {
      normalized = normalized - 'a' + 'A';  // Uppercase
    }
    return normalized;
  }

  /**
   * Read and normalize a counted string from memory.
   *
   * @param banks Memory banks
   * @param addr Address of counted string
   * @return Normalized string (count byte not included)
   */
  std::string readNormalizedCountedString(const ConstMemoryBanks& banks, uint16_t addr);

  /**
   * Validate a ProDOS pathname component.
   *
   * Rules:
   * - Length 1-15 characters
   * - First character must be A-Z
   * - Remaining characters: A-Z, 0-9, '.'
   *
   * @param component Component to validate
   * @return true if valid, false otherwise
   */
  bool isValidComponent(const std::string& component);

  /**
   * Validate a ProDOS pathname.
   *
   * @param pathname Pathname to validate (already normalized)
   * @param maxLength Maximum allowed length (64 for single pathname, 128 for full path)
   * @return true if valid, false otherwise
   */
  bool isValidPathname(const std::string& pathname, size_t maxLength = 64);

  /**
   * Resolve a pathname to a full path.
   *
   * - If pathname starts with '/', it's a full path - use as-is
   * - Otherwise, prepend the prefix
   *
   * @param pathname Input pathname (normalized)
   * @param prefix Current prefix
   * @return Resolved full path, or empty string if result would exceed 128 bytes
   */
  std::string resolveFullPath(const std::string& pathname, const std::string& prefix);

  /**
   * Map a ProDOS path to host filesystem path.
   *
   * Example: /VOLUME/DIR/FILE -> volumesRoot/VOLUME/DIR/FILE
   *
   * @param prodosPath Full ProDOS path (must start with /)
   * @param volumesRoot Root directory for volumes
   * @return Host filesystem path
   */
  std::filesystem::path mapToHostPath(const std::string&           prodosPath,
                                      const std::filesystem::path& volumesRoot);

}  // namespace prodos8emu
