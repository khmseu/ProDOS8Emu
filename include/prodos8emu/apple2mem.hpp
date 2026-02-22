#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

#include "memory.hpp"

namespace prodos8emu {

  /**
   * Apple2Memory - Owner of emulated Apple II memory with Language Card support.
   *
   * Models the Apple II 64KB address space and the Language Card (LC) bank-switching
   * hardware. Memory is organized as 16 banks × 4096 bytes.
   *
   * Apple II Memory Map:
   *   $0000-$BFFF  (banks  0-11): Main RAM (48KB, always read/write)
   *   $C000-$CFFF  (bank  12):    I/O area (treated as RAM in this model)
   *   $D000-$DFFF  (bank  13):    ROM or LC bank 1 / LC bank 2 RAM
   *   $E000-$EFFF  (bank  14):    ROM or LC high RAM
   *   $F000-$FFFF  (bank  15):    ROM or LC high RAM
   *
   * Language Card (LC):
   *   The LC adds 16KB of extra RAM that overlays the ROM region ($D000-$FFFF).
   *   The $D000-$DFFF range is double-banked (bank 1 and bank 2); $E000-$FFFF
   *   has a single LC bank.
   *
   *   - LC read enabled:  $D000-$FFFF reads come from LC RAM (bank-selected).
  *   - LC read disabled: $D000-$FFFF reads come from the ROM area (loaded via loadROM() or
  *     zero-filled if not loaded).
   *   - LC write enabled: $D000-$FFFF writes go to LC RAM (bank-selected).
   *   - LC write disabled: writes to $D000-$FFFF are ignored.
   *
   * Read-vs-write mapping note:
   *   Real hardware can be in ROMIN mode where reads come from ROM but writes go
   *   to language-card RAM. To model this, Apple2Memory exposes two bank views:
   *
   *   - constBanks(): read mapping ($D000-$FFFF reads follow LC read state)
   *   - banks():      write mapping ($D000-$FFFF writes follow LC write state)
   *
   *   In ROMIN/RDROM modes, do not use banks() to perform reads from
   *   $D000-$FFFF; use constBanks() instead.
   *
   * On construction and after reset():
  *   - All RAM is zeroed.
   *   - LC read and write are disabled (ROM mode).
   *   - LC bank 1 is selected.
   */
  class Apple2Memory {
   public:
    /**
     * Construct with all memory zeroed, LC disabled, bank 1 selected.
     */
    Apple2Memory();

    /**
     * Reset RAM to zero and restore initial LC state (disabled, bank 1).
     * Preserves any loaded ROM content.
     */
    void reset();

    /**
     * Load ROM image from file into the ROM area ($D000-$FFFF).
     *
     * Reads exactly 12KB (0x3000 bytes) from the specified file and populates
     * the internal ROM area. The ROM content becomes visible when LC read is
     * disabled.
     *
     * @param path Path to the ROM image file (must be exactly 12KB).
     * @throws std::runtime_error if file cannot be opened, is wrong size, or
     *         read fails.
     */
    void loadROM(const std::filesystem::path& path);

    /**
     * Get mutable memory banks for use with MLI calls.
     *
     * @return Reference to the current MemoryBanks array.
     */
    MemoryBanks& banks() {
      return m_banks;
    }

    /**
     * Get const memory banks for use with MLI calls.
     *
     * @return Reference to the current ConstMemoryBanks array.
     */
    const ConstMemoryBanks& constBanks() const {
      return m_constBanks;
    }

    /**
     * Enable or disable Language Card read.
     *
     * When enabled, reads from $D000-$FFFF come from the LC RAM selected by
     * the current bank setting. When disabled, reads come from the ROM area
     * (loaded via loadROM(), or zero-filled if no ROM was loaded).
     *
     * @param enable True to read from LC RAM; false to read from ROM area.
     */
    void setLCReadEnabled(bool enable);

    /**
     * Enable or disable Language Card write.
     *
     * When disabled, writes to $D000-$FFFF are redirected to an internal
     * write-sink buffer so they do not modify ROM or LC RAM.
     *
     * @param enable True to enable writes to LC RAM; false to protect LC RAM.
     */
    void setLCWriteEnabled(bool enable);

    /**
     * Select the active Language Card $D000-$DFFF bank.
     *
     * The LC has two independently writable 4KB banks at $D000-$DFFF.
     * The $E000-$FFFF region is a single LC bank and is unaffected by this call.
     *
     * @param bank1 True to select LC bank 1; false to select LC bank 2.
     */
    void setLCBank1(bool bank1);

    /**
     * Returns true if LC read is currently enabled.
     */
    bool isLCReadEnabled() const {
      return m_lcReadEnabled;
    }

    /**
     * Returns true if LC write is currently enabled.
     */
    bool isLCWriteEnabled() const {
      return m_lcWriteEnabled;
    }

    /**
     * Returns true if LC bank 1 is currently selected for $D000-$DFFF.
     */
    bool isLCBank1() const {
      return m_lcBank1;
    }

    /**
     * Process a Language Card soft-switch access at $C080–$C08F.
     *
     * Emulates the 16 LC soft switches that the Apple II maps to $C080–$C08F.
     * Each access (read or write) updates the LC read, write, and bank state
     * according to Apple II hardware behavior, including the two-read
     * write-enable pre-qualification protocol.
     *
     * Address encoding (bits 3–0 of the address):
     *   Bit 3:  bank select – 0 = LC bank 2, 1 = LC bank 1 ($D000-$DFFF)
     *   Bits 1-0: command:
     *     00: LC read enabled,  write protected
     *     01: ROM read (LC disabled), write-enable (requires 2 consecutive reads)
     *     10: ROM read (LC disabled), write protected
     *     11: LC read enabled, write-enable (requires 2 consecutive reads)
     *
     * Write-enable protocol:
     *   Write-enable is activated only after two consecutive read accesses to a
     *   write-enable switch (bits 1–0 == 01 or 11). Any write access to a soft
     *   switch, or any read to a non-write-enable switch, clears the
     *   pre-qualification latch.
     *
     * @param addr Address in the range $C080–$C08F (addresses outside this
     *             range are silently ignored).
     * @param isRead True if this is a read access; false for a write access.
     * @return true if the address was a valid LC soft switch ($C080–$C08F);
     *         false otherwise.
     */
    bool applySoftSwitch(uint16_t addr, bool isRead);

    /**
     * Returns true if the LC write-enable pre-qualification latch is set.
     *
     * After one read to a write-enable soft switch, this returns true.
     * A second such read actually enables write. Any other access clears it.
     */
    bool isLCWritePrequalified() const {
      return m_lcWritePrequalified;
    }

   private:
    // Bank indices for the Apple II memory map.
    static constexpr std::size_t MAIN_RAM_LAST_BANK = 12;  // $0000-$CFFF (banks 0-12)
    static constexpr std::size_t LC_D000_BANK       = 13;  // $D000-$DFFF (language card)
    static constexpr std::size_t LC_E000_BANK       = 14;  // $E000-$EFFF (LC high)
    static constexpr std::size_t LC_F000_BANK       = 15;  // $F000-$FFFF (LC high)

    // Main RAM: banks 0-15 ($0000-$FFFF), 64KB total.
    // When LC read is enabled, banks 13-15 are redirected to the LC buffers below.
    std::array<std::array<uint8_t, BANK_SIZE>, NUM_BANKS> m_mainRam;

    // Language Card bank 2 storage for $D000-$DFFF (bank index 13).
    // LC bank 1 for $D000-$DFFF reuses m_mainRam[13].
    std::array<uint8_t, BANK_SIZE> m_lcBank2;

    // ROM area: loaded via loadROM() or zero-filled if not loaded, used for $D000-$FFFF when LC
    // read is disabled. Sized to cover banks 13-15 (3 × 4KB).
    std::array<uint8_t, BANK_SIZE * 3> m_romArea;

    // Write-sink area: used for $D000-$FFFF when LC write is disabled.
    // Sized to cover banks 13-15 (3 × 4KB). Writes go here and are effectively ignored.
    std::array<uint8_t, BANK_SIZE * 3> m_writeSink;

    MemoryBanks      m_banks;
    ConstMemoryBanks m_constBanks;

    bool m_lcReadEnabled;
    bool m_lcWriteEnabled;
    bool m_lcBank1;
    bool m_lcWritePrequalified;  // write-enable pre-qualification latch

    // Recompute m_banks / m_constBanks to reflect current LC state.
    void updateBanks();
  };

}  // namespace prodos8emu
