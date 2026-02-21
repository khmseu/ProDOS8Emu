#include "prodos8emu/apple2mem.hpp"

#include <cstring>

namespace prodos8emu {

  Apple2Memory::Apple2Memory()
      : m_mainRam{}, m_lcBank2{}, m_romArea{}, m_lcReadEnabled(false), m_lcWriteEnabled(false),
        m_lcBank1(true), m_lcWritePrequalified(false) {
    updateBanks();
  }

  void Apple2Memory::reset() {
    for (auto& bank : m_mainRam) {
      bank.fill(0);
    }
    m_lcBank2.fill(0);
    // m_romArea is always zero (never written); no need to re-zero it.
    m_lcReadEnabled       = false;
    m_lcWriteEnabled      = false;
    m_lcBank1             = true;
    m_lcWritePrequalified = false;
    updateBanks();
  }

  void Apple2Memory::setLCReadEnabled(bool enable) {
    if (m_lcReadEnabled != enable) {
      m_lcReadEnabled = enable;
      updateBanks();
    }
  }

  void Apple2Memory::setLCWriteEnabled(bool enable) {
    m_lcWriteEnabled = enable;
  }

  void Apple2Memory::setLCBank1(bool bank1) {
    if (m_lcBank1 != bank1) {
      m_lcBank1 = bank1;
      updateBanks();
    }
  }

  void Apple2Memory::updateBanks() {
    // Banks 0-12 ($0000-$CFFF): always main RAM.
    for (std::size_t i = 0; i <= MAIN_RAM_LAST_BANK; i++) {
      m_banks[i]      = m_mainRam[i].data();
      m_constBanks[i] = m_mainRam[i].data();
    }

    if (m_lcReadEnabled) {
      // Bank 13 ($D000-$DFFF): LC bank 1 or LC bank 2.
      if (m_lcBank1) {
        m_banks[LC_D000_BANK]      = m_mainRam[LC_D000_BANK].data();
        m_constBanks[LC_D000_BANK] = m_mainRam[LC_D000_BANK].data();
      } else {
        m_banks[LC_D000_BANK]      = m_lcBank2.data();
        m_constBanks[LC_D000_BANK] = m_lcBank2.data();
      }
      // Banks 14-15 ($E000-$FFFF): LC high RAM (single bank, stored in m_mainRam[14..15]).
      m_banks[LC_E000_BANK]      = m_mainRam[LC_E000_BANK].data();
      m_constBanks[LC_E000_BANK] = m_mainRam[LC_E000_BANK].data();
      m_banks[LC_F000_BANK]      = m_mainRam[LC_F000_BANK].data();
      m_constBanks[LC_F000_BANK] = m_mainRam[LC_F000_BANK].data();
    } else {
      // LC read disabled: $D000-$FFFF reads from the zero-filled ROM area.
      m_banks[LC_D000_BANK]      = m_romArea.data();
      m_constBanks[LC_D000_BANK] = m_romArea.data();
      m_banks[LC_E000_BANK]      = m_romArea.data() + BANK_SIZE;
      m_constBanks[LC_E000_BANK] = m_romArea.data() + BANK_SIZE;
      m_banks[LC_F000_BANK]      = m_romArea.data() + BANK_SIZE * 2;
      m_constBanks[LC_F000_BANK] = m_romArea.data() + BANK_SIZE * 2;
    }
  }

  bool Apple2Memory::applySoftSwitch(uint16_t addr, bool isRead) {
    if (addr < 0xC080 || addr > 0xC08F) {
      return false;
    }

    uint8_t offset = static_cast<uint8_t>(addr & 0x000F);
    bool    bank1  = (offset & 0x08) != 0;  // bit 3: 1 = bank 1, 0 = bank 2
    uint8_t cmd    = offset & 0x03;          // bits 1-0: command

    // Select the $D000-$DFFF bank
    setLCBank1(bank1);

    // Determine what this switch requests
    bool wantsWriteEnable = (cmd == 1 || cmd == 3);
    bool wantsLCRead      = (cmd == 0 || cmd == 3);

    if (!isRead) {
      // Write access: clear pre-qualification, disable write
      m_lcWritePrequalified = false;
      setLCWriteEnabled(false);
    } else if (wantsWriteEnable) {
      if (m_lcWritePrequalified) {
        // Second consecutive qualifying read: enable write, consume the latch
        setLCWriteEnabled(true);
        m_lcWritePrequalified = false;
      } else {
        // First qualifying read: set pre-qualification latch
        m_lcWritePrequalified = true;
        setLCWriteEnabled(false);
      }
    } else {
      // Read to non-write-enable switch: clear pre-qualification, disable write
      m_lcWritePrequalified = false;
      setLCWriteEnabled(false);
    }

    setLCReadEnabled(wantsLCRead);

    return true;
  }

}  // namespace prodos8emu
