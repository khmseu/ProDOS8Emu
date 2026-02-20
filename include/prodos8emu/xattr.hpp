#pragma once

#include <cstdint>
#include <string>

namespace prodos8emu {

  /**
   * Linux extended attribute helpers for ProDOS 8 emulation.
   *
   * Uses the user.prodos8.* namespace for storing ProDOS metadata.
   *
   * Returns ProDOS error codes:
   * - ERR_NO_ERROR (0x00) on success
   * - ERR_IO_ERROR (0x27) when xattrs not supported or other I/O error
   * - ERR_ACCESS_ERROR (0x4E) when access is denied
   */

  /**
   * Set an extended attribute with user.prodos8.* prefix.
   *
   * @param path Host file path
   * @param attrName Attribute name (without user.prodos8. prefix)
   * @param value Attribute value
   * @return ProDOS error code
   */
  uint8_t prodos8_set_xattr(const std::string& path, const std::string& attrName,
                            const std::string& value);

  /**
   * Get an extended attribute with user.prodos8.* prefix.
   *
   * @param path Host file path
   * @param attrName Attribute name (without user.prodos8. prefix)
   * @param value Output parameter for attribute value
   * @return ProDOS error code
   */
  uint8_t prodos8_get_xattr(const std::string& path, const std::string& attrName,
                            std::string& value);

  /**
   * Remove an extended attribute with user.prodos8.* prefix.
   *
   * @param path Host file path
   * @param attrName Attribute name (without user.prodos8. prefix)
   * @return ProDOS error code
   */
  uint8_t prodos8_remove_xattr(const std::string& path, const std::string& attrName);

}  // namespace prodos8emu
