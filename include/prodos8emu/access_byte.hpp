#pragma once

#include <cstdint>
#include <string>

namespace prodos8emu {

  /**
   * ProDOS 8 Access Byte Codec
   *
   * The access byte controls file/directory permissions in ProDOS.
   * This codec converts between binary form and an 8-character string representation.
   *
   * Bit mapping (MSB to LSB):
   *   Bit 7: destroy   (d)
   *   Bit 6: rename    (n)
   *   Bit 5: backup    (b)
   *   Bit 4: reserved  (.) - always displayed as '.', cleared when parsing
   *   Bit 3: reserved  (.) - always displayed as '.', cleared when parsing
   *   Bit 2: invisible (i)
   *   Bit 1: write     (w)
   *   Bit 0: read      (r)
   *
   * String format: 8 characters, one per bit from MSB to LSB
   *   - When bit is set: corresponding letter (d, n, b, i, w, r)
   *   - When bit is clear: '-'
   *   - Reserved bits (4, 3): always '.' in output
   *
   * Examples:
   *   0xC3 -> "dn-..-wr" (destroy, rename, write, read)
   *   0xE3 -> "dnb..-wr" (destroy, rename, backup, write, read)
   *   0x00 -> "---..---" (all permissions off)
   *   0xE7 -> "dnb..iwr" (all defined bits set)
   */

  /**
   * Format an access byte as an 8-character string.
   *
   * @param accessByte The ProDOS access byte to format
   * @return 8-character string representation (e.g., "dn-..-wr")
   */
  inline std::string format_access_byte(uint8_t accessByte) {
    char result[9];                               // 8 chars + null terminator
    result[0] = (accessByte & 0x80) ? 'd' : '-';  // bit 7: destroy
    result[1] = (accessByte & 0x40) ? 'n' : '-';  // bit 6: rename
    result[2] = (accessByte & 0x20) ? 'b' : '-';  // bit 5: backup
    result[3] = '.';                              // bit 4: reserved
    result[4] = '.';                              // bit 3: reserved
    result[5] = (accessByte & 0x04) ? 'i' : '-';  // bit 2: invisible
    result[6] = (accessByte & 0x02) ? 'w' : '-';  // bit 1: write
    result[7] = (accessByte & 0x01) ? 'r' : '-';  // bit 0: read
    result[8] = '\0';
    return std::string(result);
  }

  /**
   * Parse an 8-character string into an access byte.
   *
   * Accepts exactly 8 characters. For defined bit positions, accepts
   * the expected letter (bit set) or '-' (bit clear). For reserved
   * positions (bits 4 and 3), requires '.' (bits are always cleared).
   *
   * @param str The 8-character string to parse
   * @param out Output parameter for the parsed access byte (only valid if return is true)
   * @return true if parse succeeded, false if invalid format
   */
  inline bool parse_access_byte(const std::string& str, uint8_t& out) {
    // Must be exactly 8 characters
    if (str.length() != 8) {
      return false;
    }

    uint8_t result = 0;

    // Bit 7: destroy (d or -)
    if (str[0] == 'd') {
      result |= 0x80;
    } else if (str[0] != '-') {
      return false;
    }

    // Bit 6: rename (n or -)
    if (str[1] == 'n') {
      result |= 0x40;
    } else if (str[1] != '-') {
      return false;
    }

    // Bit 5: backup (b or -)
    if (str[2] == 'b') {
      result |= 0x20;
    } else if (str[2] != '-') {
      return false;
    }

    // Bit 4: reserved (must be '.')
    if (str[3] != '.') {
      return false;
    }

    // Bit 3: reserved (must be '.')
    if (str[4] != '.') {
      return false;
    }

    // Bit 2: invisible (i or -)
    if (str[5] == 'i') {
      result |= 0x04;
    } else if (str[5] != '-') {
      return false;
    }

    // Bit 1: write (w or -)
    if (str[6] == 'w') {
      result |= 0x02;
    } else if (str[6] != '-') {
      return false;
    }

    // Bit 0: read (r or -)
    if (str[7] == 'r') {
      result |= 0x01;
    } else if (str[7] != '-') {
      return false;
    }

    out = result;
    return true;
  }

}  // namespace prodos8emu
