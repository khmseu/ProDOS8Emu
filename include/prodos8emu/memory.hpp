#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace prodos8emu {

  /**
   * Memory access helpers for emulated 6502 banked memory
   *
   * MLI calls operate on externally-provided emulated 6502 banked memory
   * (16 × 4KB banks) passed in as an array of bank pointers.
   *
   * Bank mapping:
   *   Bank 0:  0x0000 - 0x0FFF (4096 bytes)
   *   Bank 1:  0x1000 - 0x1FFF (4096 bytes)
   *   ...
   *   Bank 15: 0xF000 - 0xFFFF (4096 bytes)
   *
   * Address translation:
   *   bank = addr >> 12
   *   offset = addr & 0x0FFF
   *
   * Multi-byte reads/writes use little-endian format and wrap around at 0xFFFF.
   */

  constexpr std::size_t BANK_SIZE = 4096;
  constexpr std::size_t NUM_BANKS = 16;

  // Type aliases for bank pointer arrays
  using MemoryBanks      = std::array<std::uint8_t*, NUM_BANKS>;
  using ConstMemoryBanks = std::array<const std::uint8_t*, NUM_BANKS>;

  /**
   * Read a single byte from the given address.
   */
  inline uint8_t read_u8(const MemoryBanks& banks, uint16_t addr) {
    std::size_t bank   = addr >> 12;
    std::size_t offset = addr & 0x0FFF;
    return banks[bank][offset];
  }

  /**
   * Read a single byte from the given address (const version).
   */
  inline uint8_t read_u8(const ConstMemoryBanks& banks, uint16_t addr) {
    std::size_t bank   = addr >> 12;
    std::size_t offset = addr & 0x0FFF;
    return banks[bank][offset];
  }

  /**
   * Write a single byte to the given address.
   */
  inline void write_u8(MemoryBanks& banks, uint16_t addr, uint8_t value) {
    std::size_t bank    = addr >> 12;
    std::size_t offset  = addr & 0x0FFF;
    banks[bank][offset] = value;
  }

  /**
   * Read a 16-bit little-endian value from the given address.
   * Wraps around at 0xFFFF if necessary.
   */
  inline uint16_t read_u16_le(const MemoryBanks& banks, uint16_t addr) {
    uint8_t lo = read_u8(banks, addr);
    uint8_t hi = read_u8(banks, static_cast<uint16_t>(addr + 1));  // Wraps automatically
    return static_cast<uint16_t>((hi << 8) | lo);
  }

  /**
   * Read a 16-bit little-endian value from the given address (const version).
   * Wraps around at 0xFFFF if necessary.
   */
  inline uint16_t read_u16_le(const ConstMemoryBanks& banks, uint16_t addr) {
    uint8_t lo = read_u8(banks, addr);
    uint8_t hi = read_u8(banks, static_cast<uint16_t>(addr + 1));  // Wraps automatically
    return static_cast<uint16_t>((hi << 8) | lo);
  }

  /**
   * Write a 16-bit little-endian value to the given address.
   * Wraps around at 0xFFFF if necessary.
   */
  inline void write_u16_le(MemoryBanks& banks, uint16_t addr, uint16_t value) {
    write_u8(banks, addr, static_cast<uint8_t>(value & 0xFF));
    write_u8(banks, static_cast<uint16_t>(addr + 1), static_cast<uint8_t>((value >> 8) & 0xFF));
  }

  /**
   * Read a 24-bit little-endian value from the given address.
   * Returns a 32-bit value with the high byte zero.
   * Wraps around at 0xFFFF if necessary.
   */
  inline uint32_t read_u24_le(const MemoryBanks& banks, uint16_t addr) {
    uint8_t b0 = read_u8(banks, addr);
    uint8_t b1 = read_u8(banks, static_cast<uint16_t>(addr + 1));
    uint8_t b2 = read_u8(banks, static_cast<uint16_t>(addr + 2));
    return static_cast<uint32_t>((b2 << 16) | (b1 << 8) | b0);
  }

  /**
   * Read a 24-bit little-endian value from the given address (const version).
   * Returns a 32-bit value with the high byte zero.
   * Wraps around at 0xFFFF if necessary.
   */
  inline uint32_t read_u24_le(const ConstMemoryBanks& banks, uint16_t addr) {
    uint8_t b0 = read_u8(banks, addr);
    uint8_t b1 = read_u8(banks, static_cast<uint16_t>(addr + 1));
    uint8_t b2 = read_u8(banks, static_cast<uint16_t>(addr + 2));
    return static_cast<uint32_t>((b2 << 16) | (b1 << 8) | b0);
  }

  /**
   * Write a 24-bit little-endian value to the given address.
   * Only the low 24 bits of the value are used.
   * Wraps around at 0xFFFF if necessary.
   */
  inline void write_u24_le(MemoryBanks& banks, uint16_t addr, uint32_t value) {
    write_u8(banks, addr, static_cast<uint8_t>(value & 0xFF));
    write_u8(banks, static_cast<uint16_t>(addr + 1), static_cast<uint8_t>((value >> 8) & 0xFF));
    write_u8(banks, static_cast<uint16_t>(addr + 2), static_cast<uint8_t>((value >> 16) & 0xFF));
  }

  /**
   * Read a ProDOS counted string from the given address.
   *
   * Format: [count byte][data bytes...]
   *
   * @param banks The memory bank array
   * @param addr Address of the count byte
   * @param maxLen Maximum string length to read (default 64)
   * @return The string data (count byte is not included)
   */
  inline std::string read_counted_string(const MemoryBanks& banks, uint16_t addr,
                                         std::size_t maxLen = 64) {
    uint8_t     count      = read_u8(banks, addr);
    std::size_t actual_len = std::min(static_cast<std::size_t>(count), maxLen);

    std::string result;
    result.reserve(actual_len);

    for (std::size_t i = 0; i < actual_len; i++) {
      result.push_back(static_cast<char>(read_u8(banks, static_cast<uint16_t>(addr + 1 + i))));
    }

    return result;
  }

  /**
   * Read a ProDOS counted string from the given address (const version).
   *
   * Format: [count byte][data bytes...]
   *
   * @param banks The memory bank array
   * @param addr Address of the count byte
   * @param maxLen Maximum string length to read (default 64)
   * @return The string data (count byte is not included)
   */
  inline std::string read_counted_string(const ConstMemoryBanks& banks, uint16_t addr,
                                         std::size_t maxLen = 64) {
    uint8_t     count      = read_u8(banks, addr);
    std::size_t actual_len = std::min(static_cast<std::size_t>(count), maxLen);

    std::string result;
    result.reserve(actual_len);

    for (std::size_t i = 0; i < actual_len; i++) {
      result.push_back(static_cast<char>(read_u8(banks, static_cast<uint16_t>(addr + 1 + i))));
    }

    return result;
  }

}  // namespace prodos8emu
