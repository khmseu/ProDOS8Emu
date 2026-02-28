#include "prodos8emu/cpu65c02.hpp"

#include <iomanip>
#include <ostream>

#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"

namespace prodos8emu {

  namespace {

    constexpr uint16_t VEC_RESET       = 0xFFFC;
    constexpr uint16_t VEC_IRQ         = 0xFFFE;
    constexpr uint16_t COUT_VECTOR_PTR = 0x0036;

    inline uint16_t make_u16(uint8_t lo, uint8_t hi) {
      return static_cast<uint16_t>(static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8));
    }

    const char* mli_call_name(uint8_t callNumber) {
      switch (callNumber) {
        case 0xC0:
          return "CREATE";
        case 0xC1:
          return "DESTROY";
        case 0xC2:
          return "RENAME";
        case 0xC3:
          return "SET_FILE_INFO";
        case 0xC4:
          return "GET_FILE_INFO";
        case 0xC5:
          return "ON_LINE";
        case 0xC6:
          return "SET_PREFIX";
        case 0xC7:
          return "GET_PREFIX";
        case 0xC8:
          return "OPEN";
        case 0xC9:
          return "NEWLINE";
        case 0xCA:
          return "READ";
        case 0xCB:
          return "WRITE";
        case 0xCC:
          return "CLOSE";
        case 0xCD:
          return "FLUSH";
        case 0xCE:
          return "SET_MARK";
        case 0xCF:
          return "GET_MARK";
        case 0xD0:
          return "SET_EOF";
        case 0xD1:
          return "GET_EOF";
        case 0xD2:
          return "SET_BUF";
        case 0xD3:
          return "GET_BUF";
        case 0x40:
          return "ALLOC_INTERRUPT";
        case 0x41:
          return "DEALLOC_INTERRUPT";
        case 0x80:
          return "READ_BLOCK";
        case 0x81:
          return "WRITE_BLOCK";
        case 0x82:
          return "GET_TIME";
        default:
          return "UNKNOWN";
      }
    }

    void write_hex(std::ostream& os, uint32_t value, int width) {
      auto flags = os.flags();
      auto fill  = os.fill();
      os << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << value;
      os.flags(flags);
      os.fill(fill);
    }

  }  // namespace

  CPU65C02::CPU65C02(Apple2Memory& mem) : m_mem(mem) {
  }

  void CPU65C02::attachMLI(MLIContext& mli) {
    m_mli = &mli;
  }

  void CPU65C02::detachMLI() {
    m_mli = nullptr;
  }

  void CPU65C02::setDebugLogs(std::ostream* mliLog, std::ostream* coutLog) {
    m_mliLog  = mliLog;
    m_coutLog = coutLog;
  }

  uint8_t CPU65C02::read8(uint16_t addr) {
    if (addr >= 0xC080 && addr <= 0xC08F) {
      m_mem.applySoftSwitch(addr, true);
      return 0;
    }
    return read_u8(m_mem.constBanks(), addr);
  }

  void CPU65C02::write8(uint16_t addr, uint8_t value) {
    if (addr >= 0xC080 && addr <= 0xC08F) {
      m_mem.applySoftSwitch(addr, false);
      return;
    }
    write_u8(m_mem.banks(), addr, value);
  }

  uint16_t CPU65C02::read16(uint16_t addr) {
    uint8_t lo = read8(addr);
    uint8_t hi = read8(static_cast<uint16_t>(addr + 1));
    return make_u16(lo, hi);
  }

  uint16_t CPU65C02::read16_zp(uint8_t zpAddr) {
    uint8_t lo = read8(zpAddr);
    uint8_t hi = read8(static_cast<uint8_t>(zpAddr + 1));
    return make_u16(lo, hi);
  }

  void CPU65C02::dummyReadLastInstructionByte() {
    // CMOS 65C02-family quirk: on page-crossing indexed reads, the extra bus read is of the
    // last instruction byte (not an invalid effective address read as on NMOS 6502).
    (void)read8(static_cast<uint16_t>(m_r.pc - 1));
  }

  uint8_t CPU65C02::read8_pageCrossed(uint16_t addr, bool pageCrossed) {
    if (pageCrossed) {
      dummyReadLastInstructionByte();
    }
    return read8(addr);
  }

  uint8_t CPU65C02::fetch8() {
    uint8_t v = read8(m_r.pc);
    m_r.pc    = static_cast<uint16_t>(m_r.pc + 1);
    return v;
  }

  uint16_t CPU65C02::fetch16() {
    uint8_t lo = fetch8();
    uint8_t hi = fetch8();
    return make_u16(lo, hi);
  }

  void CPU65C02::push8(uint8_t v) {
    write8(static_cast<uint16_t>(0x0100 | m_r.sp), v);
    m_r.sp = static_cast<uint8_t>(m_r.sp - 1);
  }

  uint8_t CPU65C02::pull8() {
    m_r.sp = static_cast<uint8_t>(m_r.sp + 1);
    return read8(static_cast<uint16_t>(0x0100 | m_r.sp));
  }

  void CPU65C02::push16(uint16_t v) {
    push8(static_cast<uint8_t>((v >> 8) & 0xFF));
    push8(static_cast<uint8_t>(v & 0xFF));
  }

  uint16_t CPU65C02::pull16() {
    uint8_t lo = pull8();
    uint8_t hi = pull8();
    return make_u16(lo, hi);
  }

  void CPU65C02::setFlag(uint8_t mask, bool v) {
    if (v) {
      m_r.p = static_cast<uint8_t>(m_r.p | mask);
    } else {
      m_r.p = static_cast<uint8_t>(m_r.p & ~mask);
    }
    m_r.p = static_cast<uint8_t>(m_r.p | FLAG_U);
  }

  bool CPU65C02::getFlag(uint8_t mask) const {
    return (m_r.p & mask) != 0;
  }

  void CPU65C02::setNZ(uint8_t v) {
    setFlag(FLAG_Z, v == 0);
    setFlag(FLAG_N, (v & 0x80) != 0);
  }

  void CPU65C02::reset() {
    m_waiting = false;
    m_stopped = false;

    m_r.sp = 0xFF;
    m_r.p  = static_cast<uint8_t>(FLAG_I | FLAG_U);

    m_r.pc = read16(VEC_RESET);
  }

  uint64_t CPU65C02::run(uint64_t maxInstructions) {
    uint64_t executed = 0;
    while (executed < maxInstructions && !m_stopped) {
      (void)step();
      executed++;
      if (m_waiting) {
        break;
      }
    }
    return executed;
  }

  uint16_t CPU65C02::addr_zp() {
    return fetch8();
  }

  uint16_t CPU65C02::addr_zpx() {
    return static_cast<uint8_t>(fetch8() + m_r.x);
  }

  uint16_t CPU65C02::addr_zpy() {
    return static_cast<uint8_t>(fetch8() + m_r.y);
  }

  uint16_t CPU65C02::addr_abs() {
    return fetch16();
  }

  uint16_t CPU65C02::addr_absx(bool& pageCrossed) {
    uint16_t base = fetch16();
    uint16_t addr = static_cast<uint16_t>(base + m_r.x);
    pageCrossed   = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
  }

  uint16_t CPU65C02::addr_absy(bool& pageCrossed) {
    uint16_t base = fetch16();
    uint16_t addr = static_cast<uint16_t>(base + m_r.y);
    pageCrossed   = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
  }

  uint16_t CPU65C02::addr_ind() {
    uint16_t ptr = fetch16();
    // 65C02 fixes the 6502 page-wrap bug for JMP (abs).
    return read16(ptr);
  }

  uint16_t CPU65C02::addr_indx() {
    uint8_t zp = static_cast<uint8_t>(fetch8() + m_r.x);
    return read16_zp(zp);
  }

  uint16_t CPU65C02::addr_indy(bool& pageCrossed) {
    uint8_t  zp   = fetch8();
    uint16_t base = read16_zp(zp);
    uint16_t addr = static_cast<uint16_t>(base + m_r.y);
    pageCrossed   = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
  }

  uint16_t CPU65C02::addr_zpind() {
    uint8_t zp = fetch8();
    return read16_zp(zp);
  }

  uint16_t CPU65C02::addr_absind_x() {
    uint16_t base = fetch16();
    uint16_t ptr  = static_cast<uint16_t>(base + m_r.x);
    return read16(ptr);
  }

  int8_t CPU65C02::rel8() {
    return static_cast<int8_t>(fetch8());
  }

  uint8_t CPU65C02::adc(uint8_t a, uint8_t b) {
    uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(b) + (getFlag(FLAG_C) ? 1 : 0);

    bool v = (~(a ^ b) & (a ^ static_cast<uint8_t>(sum)) & 0x80) != 0;

    if (getFlag(FLAG_D)) {
      uint16_t lo = (a & 0x0F) + (b & 0x0F) + (getFlag(FLAG_C) ? 1 : 0);
      uint16_t hi = (a & 0xF0) + (b & 0xF0);

      if (lo > 0x09) {
        lo += 0x06;
      }
      if (lo > 0x0F) {
        hi += 0x10;
      }
      if ((hi & 0x1F0) > 0x90) {
        hi += 0x60;
      }

      uint16_t bcd = (lo & 0x0F) | (hi & 0xF0);
      setFlag(FLAG_C, (hi & 0xFF00) != 0);
      setFlag(FLAG_V, v);
      uint8_t r = static_cast<uint8_t>(bcd & 0xFF);
      setNZ(r);
      return r;
    }

    setFlag(FLAG_C, sum > 0xFF);
    setFlag(FLAG_V, v);
    uint8_t r = static_cast<uint8_t>(sum & 0xFF);
    setNZ(r);
    return r;
  }

  uint8_t CPU65C02::sbc(uint8_t a, uint8_t b) {
    uint16_t diff = static_cast<uint16_t>(a) - static_cast<uint16_t>(b) - (getFlag(FLAG_C) ? 0 : 1);

    bool v = ((a ^ b) & (a ^ static_cast<uint8_t>(diff)) & 0x80) != 0;

    if (getFlag(FLAG_D)) {
      int16_t al = static_cast<int16_t>((a & 0x0F) - (b & 0x0F) - (getFlag(FLAG_C) ? 0 : 1));
      int16_t ah = static_cast<int16_t>((a & 0xF0) - (b & 0xF0));

      if (al < 0) {
        al -= 0x06;
        ah -= 0x10;
      }
      if (ah < 0) {
        ah -= 0x60;
      }

      uint16_t bcd = static_cast<uint16_t>((static_cast<uint16_t>(al) & 0x0F) |
                                           (static_cast<uint16_t>(ah) & 0xF0));
      setFlag(FLAG_C, diff < 0x100);
      setFlag(FLAG_V, v);
      uint8_t r = static_cast<uint8_t>(bcd & 0xFF);
      setNZ(r);
      return r;
    }

    setFlag(FLAG_C, diff < 0x100);
    setFlag(FLAG_V, v);
    uint8_t r = static_cast<uint8_t>(diff & 0xFF);
    setNZ(r);
    return r;
  }

  uint8_t CPU65C02::cmp(uint8_t r, uint8_t v) {
    uint16_t diff = static_cast<uint16_t>(r) - static_cast<uint16_t>(v);
    setFlag(FLAG_C, diff < 0x100);
    setNZ(static_cast<uint8_t>(diff & 0xFF));
    return static_cast<uint8_t>(diff & 0xFF);
  }

  void CPU65C02::tsb(uint16_t addr) {
    uint8_t m = read8(addr);
    setFlag(FLAG_Z, (m & m_r.a) == 0);
    write8(addr, static_cast<uint8_t>(m | m_r.a));
  }

  void CPU65C02::trb(uint16_t addr) {
    uint8_t m = read8(addr);
    setFlag(FLAG_Z, (m & m_r.a) == 0);
    write8(addr, static_cast<uint8_t>(m & static_cast<uint8_t>(~m_r.a)));
  }

  void CPU65C02::branch(bool cond) {
    int8_t rel = rel8();
    if (!cond) {
      return;
    }
    uint16_t from        = m_r.pc;
    uint16_t to          = static_cast<uint16_t>(from + rel);
    bool     pageCrossed = (from & 0xFF00) != (to & 0xFF00);
    if (pageCrossed) {
      // Model the 65C02 page-cross extra read.
      dummyReadLastInstructionByte();
    }
    m_r.pc = to;
  }

  uint32_t CPU65C02::jsr_abs(uint16_t target) {
    // Special trap: JSR $BF00 invokes ProDOS MLI dispatch rather than changing PC.
    if (target == 0xBF00 && m_mli != nullptr) {
      // ProDOS MLI calling convention:
      //   JSR $BF00
      //   .byte callNumber
      //   .word paramBlockAddr
      // On return, execution continues after these 3 bytes.
      uint8_t  callNumber = read8(m_r.pc);
      uint16_t paramBlock = read16(static_cast<uint16_t>(m_r.pc + 1));
      m_r.pc              = static_cast<uint16_t>(m_r.pc + 3);

      uint8_t err = mli_dispatch(*m_mli, m_mem.banks(), callNumber, paramBlock);

      if (m_mliLog != nullptr) {
        *m_mliLog << "MLI call=$";
        write_hex(*m_mliLog, callNumber, 2);
        *m_mliLog << " (" << mli_call_name(callNumber) << ") param=$";
        write_hex(*m_mliLog, paramBlock, 4);
        *m_mliLog << " result=$";
        write_hex(*m_mliLog, err, 2);
        if (err == 0) {
          *m_mliLog << " OK\n";
        } else {
          *m_mliLog << " ERROR\n";
        }
        m_mliLog->flush();
      }

      // ProDOS convention: Carry set on error, A holds error code.
      m_r.a = err;
      setFlag(FLAG_C, err != 0);
      setNZ(m_r.a);

      // ProDOS MLI returns with decimal mode cleared.
      setFlag(FLAG_D, false);
      return 6;
    }

    // Normal JSR behavior.
    // After operand fetch, PC points at the next instruction; JSR pushes (PC-1).
    uint16_t ret = static_cast<uint16_t>(m_r.pc - 1);
    push16(ret);
    m_r.pc = target;
    return 6;
  }

  uint32_t CPU65C02::execute(uint8_t op) {
    // Rockwell/WDC 65C02 bit manipulation/branch opcodes.
    // RMBn: 07,17,27,37,47,57,67,77 (clear bit n in zp)
    // SMBn: 87,97,A7,B7,C7,D7,E7,F7 (set bit n in zp)
    if ((op & 0x0F) == 0x07) {
      uint8_t bit = static_cast<uint8_t>((op >> 4) & 0x07);
      uint8_t zp  = fetch8();
      uint8_t m   = read8(zp);
      if ((op & 0x80) != 0) {
        m = static_cast<uint8_t>(m | static_cast<uint8_t>(1u << bit));
      } else {
        m = static_cast<uint8_t>(m & static_cast<uint8_t>(~static_cast<uint8_t>(1u << bit)));
      }
      write8(zp, m);
      return 5;
    }

    // BBRn: 0F,1F,2F,3F,4F,5F,6F,7F (branch if bit n clear)
    // BBSn: 8F,9F,AF,BF,CF,DF,EF,FF (branch if bit n set)
    if ((op & 0x0F) == 0x0F) {
      uint8_t bit   = static_cast<uint8_t>((op >> 4) & 0x07);
      bool    isBBS = (op & 0x80) != 0;
      uint8_t zp    = fetch8();
      int8_t  rel   = static_cast<int8_t>(fetch8());

      uint8_t m      = read8(zp);
      bool    bitSet = (m & static_cast<uint8_t>(1u << bit)) != 0;
      bool    take   = isBBS ? bitSet : !bitSet;
      if (take) {
        uint16_t from        = m_r.pc;
        uint16_t to          = static_cast<uint16_t>(from + rel);
        bool     pageCrossed = (from & 0xFF00) != (to & 0xFF00);
        if (pageCrossed) {
          dummyReadLastInstructionByte();
        }
        m_r.pc = to;
      }
      return 5;
    }

    // Default for reserved/unknown opcodes: treat as 1-byte NOP.
    // Many 65C02 implementations treat undefined opcodes as NOP.
    switch (op) {
      case 0x00: {  // BRK
        // BRK is treated as a 2-byte instruction; PC is incremented once more.
        m_r.pc = static_cast<uint16_t>(m_r.pc + 1);
        push16(m_r.pc);
        push8(static_cast<uint8_t>(m_r.p | FLAG_B | FLAG_U));
        setFlag(FLAG_I, true);
        setFlag(FLAG_D, false);  // 65C02 clears D on interrupt
        m_r.pc = read16(VEC_IRQ);
        return 7;
      }

      case 0xEA:  // NOP
        return 2;

      case 0xDB:  // STP
        m_stopped = true;
        return 3;

      case 0xCB:  // WAI
        m_waiting = true;
        return 3;

      // Flag operations
      case 0x18:
        setFlag(FLAG_C, false);
        return 2;
      case 0x38:
        setFlag(FLAG_C, true);
        return 2;
      case 0x58:
        setFlag(FLAG_I, false);
        return 2;
      case 0x78:
        setFlag(FLAG_I, true);
        return 2;
      case 0xD8:
        setFlag(FLAG_D, false);
        return 2;
      case 0xF8:
        setFlag(FLAG_D, true);
        return 2;
      case 0xB8:
        setFlag(FLAG_V, false);
        return 2;

      // Transfers
      case 0xAA:
        m_r.x = m_r.a;
        setNZ(m_r.x);
        return 2;
      case 0x8A:
        m_r.a = m_r.x;
        setNZ(m_r.a);
        return 2;
      case 0xA8:
        m_r.y = m_r.a;
        setNZ(m_r.y);
        return 2;
      case 0x98:
        m_r.a = m_r.y;
        setNZ(m_r.a);
        return 2;
      case 0xBA:
        m_r.x = m_r.sp;
        setNZ(m_r.x);
        return 2;
      case 0x9A:
        m_r.sp = m_r.x;
        return 2;

      // INC/DEC registers
      case 0xE8:
        m_r.x = static_cast<uint8_t>(m_r.x + 1);
        setNZ(m_r.x);
        return 2;
      case 0xCA:
        m_r.x = static_cast<uint8_t>(m_r.x - 1);
        setNZ(m_r.x);
        return 2;
      case 0xC8:
        m_r.y = static_cast<uint8_t>(m_r.y + 1);
        setNZ(m_r.y);
        return 2;
      case 0x88:
        m_r.y = static_cast<uint8_t>(m_r.y - 1);
        setNZ(m_r.y);
        return 2;

      // INC/DEC accumulator (65C02)
      case 0x1A:
        m_r.a = static_cast<uint8_t>(m_r.a + 1);
        setNZ(m_r.a);
        return 2;
      case 0x3A:
        m_r.a = static_cast<uint8_t>(m_r.a - 1);
        setNZ(m_r.a);
        return 2;

      // Stack
      case 0x48:
        push8(m_r.a);
        return 3;
      case 0x68:
        m_r.a = pull8();
        setNZ(m_r.a);
        return 4;
      case 0x08:
        push8(static_cast<uint8_t>(m_r.p | FLAG_B | FLAG_U));
        return 3;
      case 0x28:
        m_r.p = static_cast<uint8_t>(pull8() | FLAG_U);
        return 4;
      case 0xDA:  // PHX
        push8(m_r.x);
        return 3;
      case 0xFA:  // PLX
        m_r.x = pull8();
        setNZ(m_r.x);
        return 4;
      case 0x5A:  // PHY
        push8(m_r.y);
        return 3;
      case 0x7A:  // PLY
        m_r.y = pull8();
        setNZ(m_r.y);
        return 4;

      // Jumps/returns
      case 0x4C:  // JMP abs
        m_r.pc = fetch16();
        return 3;
      case 0x6C: {  // JMP (abs)
        uint16_t ptr    = fetch16();
        uint16_t target = read16(ptr);
        if (ptr == COUT_VECTOR_PTR && m_coutLog != nullptr) {
          uint8_t ch = static_cast<uint8_t>(m_r.a & 0x7F);

          // ProDOS convention: 0x0D (CR) -> output newline
          if (ch == 0x0D) {
            *m_coutLog << '\n';
          }
          // Printable ASCII: output as-is
          else if (ch >= 0x20 && ch <= 0x7E) {
            *m_coutLog << static_cast<char>(ch);
          }
          // Control characters: output C-style escape sequences
          else {
            switch (ch) {
              case 0x00:
                *m_coutLog << "\\0";
                break;
              case 0x07:
                *m_coutLog << "\\a";
                break;
              case 0x08:
                *m_coutLog << "\\b";
                break;
              case 0x09:
                *m_coutLog << "\\t";
                break;
              case 0x0A:
                *m_coutLog << "\\n";
                break;
              case 0x0B:
                *m_coutLog << "\\v";
                break;
              case 0x0C:
                *m_coutLog << "\\f";
                break;
              case 0x1B:
                *m_coutLog << "\\e";
                break;
              case 0x7F:
                *m_coutLog << "\\x7f";
                break;
              default:
                // Other control characters: use \xHH notation
                *m_coutLog << "\\x";
                write_hex(*m_coutLog, ch, 2);
                break;
            }
          }
          m_coutLog->flush();
        }
        m_r.pc = target;
        return 5;
      }
      case 0x7C:  // JMP (abs,X)
        m_r.pc = addr_absind_x();
        return 6;
      case 0x20: {  // JSR abs
        uint16_t target = fetch16();
        return jsr_abs(target);
      }
      case 0x60:  // RTS
        m_r.pc = static_cast<uint16_t>(pull16() + 1);
        return 6;
      case 0x40:  // RTI
        m_r.p  = static_cast<uint8_t>(pull8() | FLAG_U);
        m_r.pc = pull16();
        return 6;

      // Branches
      case 0x80:  // BRA
        branch(true);
        return 3;
      case 0x10:
        branch(!getFlag(FLAG_N));
        return 2;
      case 0x30:
        branch(getFlag(FLAG_N));
        return 2;
      case 0x50:
        branch(!getFlag(FLAG_V));
        return 2;
      case 0x70:
        branch(getFlag(FLAG_V));
        return 2;
      case 0x90:
        branch(!getFlag(FLAG_C));
        return 2;
      case 0xB0:
        branch(getFlag(FLAG_C));
        return 2;
      case 0xD0:
        branch(!getFlag(FLAG_Z));
        return 2;
      case 0xF0:
        branch(getFlag(FLAG_Z));
        return 2;

      // Loads
      case 0xA9:  // LDA #imm
        m_r.a = fetch8();
        setNZ(m_r.a);
        return 2;
      case 0xA5:
        m_r.a = read8(addr_zp());
        setNZ(m_r.a);
        return 3;
      case 0xB5:
        m_r.a = read8(addr_zpx());
        setNZ(m_r.a);
        return 4;
      case 0xAD:
        m_r.a = read8(addr_abs());
        setNZ(m_r.a);
        return 4;
      case 0xBD: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xB9: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xA1:
        m_r.a = read8(addr_indx());
        setNZ(m_r.a);
        return 6;
      case 0xB1: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0xB2:  // LDA (zp)
        m_r.a = read8(addr_zpind());
        setNZ(m_r.a);
        return 5;

      case 0xA2:  // LDX #imm
        m_r.x = fetch8();
        setNZ(m_r.x);
        return 2;
      case 0xA6:
        m_r.x = read8(addr_zp());
        setNZ(m_r.x);
        return 3;
      case 0xB6:
        m_r.x = read8(addr_zpy());
        setNZ(m_r.x);
        return 4;
      case 0xAE:
        m_r.x = read8(addr_abs());
        setNZ(m_r.x);
        return 4;
      case 0xBE: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.x       = read8_pageCrossed(a, pc);
        setNZ(m_r.x);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }

      case 0xA0:  // LDY #imm
        m_r.y = fetch8();
        setNZ(m_r.y);
        return 2;
      case 0xA4:
        m_r.y = read8(addr_zp());
        setNZ(m_r.y);
        return 3;
      case 0xB4:
        m_r.y = read8(addr_zpx());
        setNZ(m_r.y);
        return 4;
      case 0xAC:
        m_r.y = read8(addr_abs());
        setNZ(m_r.y);
        return 4;
      case 0xBC: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.y       = read8_pageCrossed(a, pc);
        setNZ(m_r.y);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }

      // Stores
      case 0x85:
        write8(addr_zp(), m_r.a);
        return 3;
      case 0x95:
        write8(addr_zpx(), m_r.a);
        return 4;
      case 0x8D:
        write8(addr_abs(), m_r.a);
        return 4;
      case 0x9D: {
        bool pc = false;
        write8(addr_absx(pc), m_r.a);
        return 5;
      }
      case 0x99: {
        bool pc = false;
        write8(addr_absy(pc), m_r.a);
        return 5;
      }
      case 0x81:
        write8(addr_indx(), m_r.a);
        return 6;
      case 0x91: {
        bool pc = false;
        write8(addr_indy(pc), m_r.a);
        return 6;
      }
      case 0x92:  // STA (zp)
        write8(addr_zpind(), m_r.a);
        return 5;

      case 0x86:
        write8(addr_zp(), m_r.x);
        return 3;
      case 0x96:
        write8(addr_zpy(), m_r.x);
        return 4;
      case 0x8E:
        write8(addr_abs(), m_r.x);
        return 4;

      case 0x84:
        write8(addr_zp(), m_r.y);
        return 3;
      case 0x94:
        write8(addr_zpx(), m_r.y);
        return 4;
      case 0x8C:
        write8(addr_abs(), m_r.y);
        return 4;

      // STZ
      case 0x64:
        write8(addr_zp(), 0);
        return 3;
      case 0x74:
        write8(addr_zpx(), 0);
        return 4;
      case 0x9C:
        write8(addr_abs(), 0);
        return 4;
      case 0x9E: {
        bool pc = false;
        write8(addr_absx(pc), 0);
        return 5;
      }

      // Logical
      // ORA
      case 0x09:
        m_r.a = static_cast<uint8_t>(m_r.a | fetch8());
        setNZ(m_r.a);
        return 2;
      case 0x05:
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_zp()));
        setNZ(m_r.a);
        return 3;
      case 0x15:
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_zpx()));
        setNZ(m_r.a);
        return 4;
      case 0x0D:
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_abs()));
        setNZ(m_r.a);
        return 4;
      case 0x1D: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a | read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x19: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a | read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x01:
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_indx()));
        setNZ(m_r.a);
        return 6;
      case 0x11: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a | read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0x12:  // ORA (zp)
        m_r.a = static_cast<uint8_t>(m_r.a | read8(addr_zpind()));
        setNZ(m_r.a);
        return 5;

      // AND
      case 0x29:
        m_r.a = static_cast<uint8_t>(m_r.a & fetch8());
        setNZ(m_r.a);
        return 2;
      case 0x25:
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_zp()));
        setNZ(m_r.a);
        return 3;
      case 0x35:
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_zpx()));
        setNZ(m_r.a);
        return 4;
      case 0x2D:
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_abs()));
        setNZ(m_r.a);
        return 4;
      case 0x3D: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a & read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x39: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a & read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x21:
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_indx()));
        setNZ(m_r.a);
        return 6;
      case 0x31: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a & read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0x32:  // AND (zp)
        m_r.a = static_cast<uint8_t>(m_r.a & read8(addr_zpind()));
        setNZ(m_r.a);
        return 5;

      // EOR
      case 0x49:
        m_r.a = static_cast<uint8_t>(m_r.a ^ fetch8());
        setNZ(m_r.a);
        return 2;
      case 0x45:
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_zp()));
        setNZ(m_r.a);
        return 3;
      case 0x55:
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_zpx()));
        setNZ(m_r.a);
        return 4;
      case 0x4D:
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_abs()));
        setNZ(m_r.a);
        return 4;
      case 0x5D: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a ^ read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x59: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a ^ read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x41:
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_indx()));
        setNZ(m_r.a);
        return 6;
      case 0x51: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = static_cast<uint8_t>(m_r.a ^ read8_pageCrossed(a, pc));
        setNZ(m_r.a);
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0x52:  // EOR (zp)
        m_r.a = static_cast<uint8_t>(m_r.a ^ read8(addr_zpind()));
        setNZ(m_r.a);
        return 5;

      // ADC/SBC
      case 0x69:
        m_r.a = adc(m_r.a, fetch8());
        return 2;
      case 0x65:
        m_r.a = adc(m_r.a, read8(addr_zp()));
        return 3;
      case 0x75:
        m_r.a = adc(m_r.a, read8(addr_zpx()));
        return 4;
      case 0x6D:
        m_r.a = adc(m_r.a, read8(addr_abs()));
        return 4;
      case 0x7D: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = adc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x79: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = adc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0x61:
        m_r.a = adc(m_r.a, read8(addr_indx()));
        return 6;
      case 0x71: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = adc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0x72:
        m_r.a = adc(m_r.a, read8(addr_zpind()));
        return 5;

      case 0xE9:
        m_r.a = sbc(m_r.a, fetch8());
        return 2;
      case 0xE5:
        m_r.a = sbc(m_r.a, read8(addr_zp()));
        return 3;
      case 0xF5:
        m_r.a = sbc(m_r.a, read8(addr_zpx()));
        return 4;
      case 0xED:
        m_r.a = sbc(m_r.a, read8(addr_abs()));
        return 4;
      case 0xFD: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = sbc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xF9: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = sbc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xE1:
        m_r.a = sbc(m_r.a, read8(addr_indx()));
        return 6;
      case 0xF1: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = sbc(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0xF2:
        m_r.a = sbc(m_r.a, read8(addr_zpind()));
        return 5;

      // Compare
      case 0xC9:
        (void)cmp(m_r.a, fetch8());
        return 2;
      case 0xC5:
        (void)cmp(m_r.a, read8(addr_zp()));
        return 3;
      case 0xD5:
        (void)cmp(m_r.a, read8(addr_zpx()));
        return 4;
      case 0xCD:
        (void)cmp(m_r.a, read8(addr_abs()));
        return 4;
      case 0xDD: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        (void)cmp(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xD9: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        (void)cmp(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }
      case 0xC1:
        (void)cmp(m_r.a, read8(addr_indx()));
        return 6;
      case 0xD1: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        (void)cmp(m_r.a, read8_pageCrossed(a, pc));
        return static_cast<uint32_t>(5 + (pc ? 1 : 0));
      }
      case 0xD2:
        (void)cmp(m_r.a, read8(addr_zpind()));
        return 5;

      case 0xE0:
        (void)cmp(m_r.x, fetch8());
        return 2;
      case 0xE4:
        (void)cmp(m_r.x, read8(addr_zp()));
        return 3;
      case 0xEC:
        (void)cmp(m_r.x, read8(addr_abs()));
        return 4;
      case 0xC0:
        (void)cmp(m_r.y, fetch8());
        return 2;
      case 0xC4:
        (void)cmp(m_r.y, read8(addr_zp()));
        return 3;
      case 0xCC:
        (void)cmp(m_r.y, read8(addr_abs()));
        return 4;

      // INC/DEC memory
      case 0xE6: {
        uint16_t a = addr_zp();
        uint8_t  v = static_cast<uint8_t>(read8(a) + 1);
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0xF6: {
        uint16_t a = addr_zpx();
        uint8_t  v = static_cast<uint8_t>(read8(a) + 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0xEE: {
        uint16_t a = addr_abs();
        uint8_t  v = static_cast<uint8_t>(read8(a) + 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0xFE: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = static_cast<uint8_t>(read8(a) + 1);
        write8(a, v);
        setNZ(v);
        return 7;
      }
      case 0xC6: {
        uint16_t a = addr_zp();
        uint8_t  v = static_cast<uint8_t>(read8(a) - 1);
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0xD6: {
        uint16_t a = addr_zpx();
        uint8_t  v = static_cast<uint8_t>(read8(a) - 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0xCE: {
        uint16_t a = addr_abs();
        uint8_t  v = static_cast<uint8_t>(read8(a) - 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0xDE: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = static_cast<uint8_t>(read8(a) - 1);
        write8(a, v);
        setNZ(v);
        return 7;
      }

      // Shifts/rotates
      case 0x0A: {  // ASL A
        setFlag(FLAG_C, (m_r.a & 0x80) != 0);
        m_r.a = static_cast<uint8_t>(m_r.a << 1);
        setNZ(m_r.a);
        return 2;
      }
      case 0x06: {
        uint16_t a = addr_zp();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>(v << 1);
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0x16: {
        uint16_t a = addr_zpx();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>(v << 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x0E: {
        uint16_t a = addr_abs();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>(v << 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x1E: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8(a);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>(v << 1);
        write8(a, v);
        setNZ(v);
        return 7;
      }

      case 0x4A: {  // LSR A
        setFlag(FLAG_C, (m_r.a & 0x01) != 0);
        m_r.a = static_cast<uint8_t>(m_r.a >> 1);
        setNZ(m_r.a);
        return 2;
      }
      case 0x46: {
        uint16_t a = addr_zp();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>(v >> 1);
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0x56: {
        uint16_t a = addr_zpx();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>(v >> 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x4E: {
        uint16_t a = addr_abs();
        uint8_t  v = read8(a);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>(v >> 1);
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x5E: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8(a);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>(v >> 1);
        write8(a, v);
        setNZ(v);
        return 7;
      }

      case 0x2A: {  // ROL A
        bool c = getFlag(FLAG_C);
        setFlag(FLAG_C, (m_r.a & 0x80) != 0);
        m_r.a = static_cast<uint8_t>((m_r.a << 1) | (c ? 1 : 0));
        setNZ(m_r.a);
        return 2;
      }
      case 0x26: {
        uint16_t a = addr_zp();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0x36: {
        uint16_t a = addr_zpx();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x2E: {
        uint16_t a = addr_abs();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x3E: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8(a);
        bool     c  = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x80) != 0);
        v = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
        write8(a, v);
        setNZ(v);
        return 7;
      }

      case 0x6A: {  // ROR A
        bool c = getFlag(FLAG_C);
        setFlag(FLAG_C, (m_r.a & 0x01) != 0);
        m_r.a = static_cast<uint8_t>((m_r.a >> 1) | (c ? 0x80 : 0));
        setNZ(m_r.a);
        return 2;
      }
      case 0x66: {
        uint16_t a = addr_zp();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
        write8(a, v);
        setNZ(v);
        return 5;
      }
      case 0x76: {
        uint16_t a = addr_zpx();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x6E: {
        uint16_t a = addr_abs();
        uint8_t  v = read8(a);
        bool     c = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
        write8(a, v);
        setNZ(v);
        return 6;
      }
      case 0x7E: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8(a);
        bool     c  = getFlag(FLAG_C);
        setFlag(FLAG_C, (v & 0x01) != 0);
        v = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
        write8(a, v);
        setNZ(v);
        return 7;
      }

      // BIT
      case 0x89: {  // BIT #imm (65C02)
        uint8_t v = fetch8();
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        return 2;
      }
      case 0x24: {
        uint8_t v = read8(addr_zp());
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        setFlag(FLAG_N, (v & 0x80) != 0);
        setFlag(FLAG_V, (v & 0x40) != 0);
        return 3;
      }
      case 0x2C: {
        uint8_t v = read8(addr_abs());
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        setFlag(FLAG_N, (v & 0x80) != 0);
        setFlag(FLAG_V, (v & 0x40) != 0);
        return 4;
      }
      case 0x34: {
        uint8_t v = read8(addr_zpx());
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        setFlag(FLAG_N, (v & 0x80) != 0);
        setFlag(FLAG_V, (v & 0x40) != 0);
        return 4;
      }
      case 0x3C: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        uint8_t  v  = read8_pageCrossed(a, pc);
        setFlag(FLAG_Z, (m_r.a & v) == 0);
        setFlag(FLAG_N, (v & 0x80) != 0);
        setFlag(FLAG_V, (v & 0x40) != 0);
        return static_cast<uint32_t>(4 + (pc ? 1 : 0));
      }

      // TSB/TRB
      case 0x04:
        tsb(addr_zp());
        return 5;
      case 0x0C:
        tsb(addr_abs());
        return 6;
      case 0x14:
        trb(addr_zp());
        return 5;
      case 0x1C:
        trb(addr_abs());
        return 6;

      // Unused opcodes on WDC 65C02: documented as NOP variants.
      // See: 6502.org 65C02 opcodes, section "Unused opcodes (undocumented NOPs)".
      // 1-byte, 1-cycle NOPs (no operand)
      case 0x03:
      case 0x0B:
      case 0x13:
      case 0x1B:
      case 0x23:
      case 0x2B:
      case 0x33:
      case 0x3B:
      case 0x43:
      case 0x4B:
      case 0x53:
      case 0x5B:
      case 0x63:
      case 0x6B:
      case 0x73:
      case 0x7B:
      case 0x83:
      case 0x8B:
      case 0x93:
      case 0x9B:
      case 0xA3:
      case 0xAB:
      case 0xB3:
      case 0xBB:
      case 0xC3:
      case 0xD3:
      case 0xE3:
      case 0xEB:
      case 0xF3:
      case 0xFB:
        return 1;

      // 2-byte, 2-cycle NOP immediate
      case 0x02:
      case 0x22:
      case 0x42:
      case 0x62:
      case 0x82:
      case 0xC2:
      case 0xE2:
        (void)fetch8();
        return 2;

      // 2-byte NOP with zp read
      case 0x44: {
        uint8_t zp = fetch8();
        (void)read8(zp);
        return 3;
      }

      // 2-byte NOP with zp,X read
      case 0x54:
      case 0xD4:
      case 0xF4: {
        uint8_t zp = fetch8();
        (void)read8(static_cast<uint8_t>(zp + m_r.x));
        return 4;
      }

      // 3-byte NOP with absolute read
      case 0xDC:
      case 0xFC: {
        uint16_t a = fetch16();
        (void)read8(a);
        return 4;
      }

      // 3-byte NOP with unusual read behavior (treat as absolute read for now)
      case 0x5C: {
        uint16_t a = fetch16();
        (void)read8(a);
        return 8;
      }

      default:
        return 2;
    }
  }

  uint32_t CPU65C02::step() {
    if (m_stopped) {
      return 0;
    }
    if (m_waiting) {
      return 0;
    }

    uint8_t op = fetch8();
    return execute(op);
  }

}  // namespace prodos8emu
