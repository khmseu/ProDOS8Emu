#include <ctime>

#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"

namespace prodos8emu {

  namespace {

    // ProDOS global page address for date/time (in emulated memory)
    // $BF90 = date (2 bytes), $BF92 = time (2 bytes)
    constexpr uint16_t PRODOS_TIME_DATE_ADDR = 0xBF90;
    constexpr uint16_t PRODOS_TIME_TIME_ADDR = 0xBF92;

    // Maximum number of interrupt slots
    constexpr uint8_t MAX_INTERRUPT_SLOTS = 4;

    /**
     * Encode a Unix timestamp to ProDOS date word.
     * Date format: bits 0-4: day (1-31), bits 5-8: month (1-12),
     *              bits 9-15: year (0-127, offset from 1900)
     */
    uint16_t encodeProDOSDate(time_t timestamp) {
      struct tm* t = localtime(&timestamp);
      if (!t) return 0;

      int day   = t->tm_mday;     // 1-31
      int month = t->tm_mon + 1;  // 0-11 -> 1-12
      int year  = t->tm_year;     // years since 1900

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
     * Time format: bits 0-5: minute (0-59), bits 8-12: hour (0-23)
     */
    uint16_t encodeProDOSTime(time_t timestamp) {
      struct tm* t = localtime(&timestamp);
      if (!t) return 0;

      int minute = t->tm_min;   // 0-59
      int hour   = t->tm_hour;  // 0-23

      if (minute < 0) minute = 0;
      if (minute > 59) minute = 59;
      if (hour < 0) hour = 0;
      if (hour > 23) hour = 23;

      return static_cast<uint16_t>((minute & 0x3F) | ((hour & 0x1F) << 8));
    }

  }  // anonymous namespace

  uint8_t MLIContext::setBufCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t  refNum   = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    uint16_t ioBufPtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    it->second.ioBuffer = ioBufPtr;
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::getBufCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));

    auto it = m_openFiles.find(refNum);
    if (it == m_openFiles.end()) {
      return ERR_BAD_REF_NUM;
    }

    write_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2), it->second.ioBuffer);
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::getTimeCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    // Per ProDOS 8 Technical Reference Manual Section 4.6.1:
    // "This call has no parameter list, and it cannot generate an error."
    // We ignore the parameter block entirely and always succeed.
    (void)paramBlockAddr;  // Unused

    time_t   now  = ::time(nullptr);
    uint16_t date = encodeProDOSDate(now);
    uint16_t time = encodeProDOSTime(now);

    write_u16_le(banks, PRODOS_TIME_DATE_ADDR, date);
    write_u16_le(banks, PRODOS_TIME_TIME_ADDR, time);

    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::allocInterruptCall(MemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 2) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint16_t intCodePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));
    if (intCodePtr == 0) {
      return ERR_INVALID_PARAMETER;
    }

    // Find first free interrupt slot (1-4)
    uint8_t slot = 0;
    for (uint8_t i = 0; i < MAX_INTERRUPT_SLOTS; i++) {
      if (m_interruptHandlers[i] == 0) {
        slot = static_cast<uint8_t>(i + 1);
        break;
      }
    }
    if (slot == 0) {
      return ERR_INTERRUPT_TABLE_FULL;
    }

    m_interruptHandlers[slot - 1] = intCodePtr;
    write_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1), slot);
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::deallocInterruptCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 1) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    uint8_t intNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
    if (intNum < 1 || intNum > MAX_INTERRUPT_SLOTS) {
      return ERR_INVALID_PARAMETER;
    }

    m_interruptHandlers[intNum - 1] = 0;
    return ERR_NO_ERROR;
  }

  uint8_t MLIContext::readBlockCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 3) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    return ERR_IO_ERROR;
  }

  uint8_t MLIContext::writeBlockCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr) {
    uint8_t paramCount = read_u8(banks, paramBlockAddr);
    if (paramCount != 3) {
      return ERR_BAD_CALL_PARAM_COUNT;
    }

    return ERR_IO_ERROR;
  }

}  // namespace prodos8emu
