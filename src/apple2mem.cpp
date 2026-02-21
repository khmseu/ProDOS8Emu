#include "prodos8emu/apple2mem.hpp"

#include <cstring>

namespace prodos8emu {

  Apple2Memory::Apple2Memory()
      : m_mainRam{}, m_lcBank2{}, m_romArea{}, m_lcReadEnabled(false), m_lcWriteEnabled(false),
        m_lcBank1(true) {
    updateBanks();
  }

  void Apple2Memory::reset() {
    for (auto& bank : m_mainRam) {
      bank.fill(0);
    }
    m_lcBank2.fill(0);
    // m_romArea is always zero (never written); no need to re-zero it.
    m_lcReadEnabled  = false;
    m_lcWriteEnabled = false;
    m_lcBank1        = true;
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

}  // namespace prodos8emu
