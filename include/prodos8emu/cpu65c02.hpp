#pragma once

#include <iosfwd>

#include "apple2mem.hpp"

namespace prodos8emu {

  class MLIContext;

  struct CPU65C02Regs {
    uint16_t pc = 0;
    uint8_t  a  = 0;
    uint8_t  x  = 0;
    uint8_t  y  = 0;
    uint8_t  sp = 0xFF;
    uint8_t  p  = 0x24;  // I=1 after reset on real HW; keep bit5 set.
  };

  class CPU65C02 {
   public:
    explicit CPU65C02(Apple2Memory& mem);

    void attachMLI(MLIContext& mli);
    void detachMLI();
    void setDebugLogs(std::ostream* mliLog, std::ostream* coutLog);

    // Reset vectors are read from $FFFC/$FFFD.
    void reset();

    // Execute a single instruction. Returns the nominal cycle count.
    uint32_t step();

    // Execute until 'maxInstructions' have been executed, or until the CPU is stopped.
    uint64_t run(uint64_t maxInstructions);

    const CPU65C02Regs& regs() const {
      return m_r;
    }

    CPU65C02Regs& regs() {
      return m_r;
    }

    bool isStopped() const {
      return m_stopped;
    }

    bool isWaiting() const {
      return m_waiting;
    }

   private:
    Apple2Memory& m_mem;
    MLIContext*   m_mli     = nullptr;
    std::ostream* m_mliLog  = nullptr;
    std::ostream* m_coutLog = nullptr;

    CPU65C02Regs m_r;

    bool     m_waiting          = false;  // WAI
    bool     m_stopped          = false;  // STP
    uint64_t m_instructionCount = 0;      // Total instructions executed

    // PC ring buffer for tracking explicit PC changes (JMP, JSR, RTS, branches, etc.)
    // Stores from->to address pairs, with loop compression via counts
    // Filters out ROM-internal transitions ($F800-$FFFF -> $F800-$FFFF)
    static constexpr size_t PC_RING_SIZE                = 100;
    uint16_t                m_pcRingFrom[PC_RING_SIZE]  = {};
    uint16_t                m_pcRingTo[PC_RING_SIZE]    = {};
    uint32_t                m_pcRingCount[PC_RING_SIZE] = {};
    size_t                  m_pcRingIndex               = 0;

    // Flags
    static constexpr uint8_t FLAG_C = 0x01;
    static constexpr uint8_t FLAG_Z = 0x02;
    static constexpr uint8_t FLAG_I = 0x04;
    static constexpr uint8_t FLAG_D = 0x08;
    static constexpr uint8_t FLAG_B = 0x10;
    static constexpr uint8_t FLAG_U = 0x20;
    static constexpr uint8_t FLAG_V = 0x40;
    static constexpr uint8_t FLAG_N = 0x80;

    // Memory helpers (read bus uses constBanks; write bus uses banks)
    uint8_t  read8(uint16_t addr);
    void     write8(uint16_t addr, uint8_t value);
    uint16_t read16(uint16_t addr);
    uint16_t read16_zp(uint8_t zpAddr);

    // Bus quirks
    void    dummyReadLastInstructionByte();
    uint8_t read8_pageCrossed(uint16_t addr, bool pageCrossed);

    uint8_t  fetch8();
    uint16_t fetch16();

    void     push8(uint8_t v);
    uint8_t  pull8();
    void     push16(uint16_t v);
    uint16_t pull16();

    void setFlag(uint8_t mask, bool v);
    bool getFlag(uint8_t mask) const;
    void setNZ(uint8_t v);

    // Addressing
    uint16_t addr_zp();
    uint16_t addr_zpx();
    uint16_t addr_zpy();
    uint16_t addr_abs();
    uint16_t addr_absx(bool& pageCrossed);
    uint16_t addr_absy(bool& pageCrossed);
    uint16_t addr_ind();
    uint16_t addr_indx();
    uint16_t addr_indy(bool& pageCrossed);
    uint16_t addr_zpind();
    uint16_t addr_absind_x();

    int8_t rel8();

    // ALU
    uint8_t adc(uint8_t a, uint8_t b);
    uint8_t sbc(uint8_t a, uint8_t b);
    uint8_t cmp(uint8_t r, uint8_t v);

    // Bit ops
    void tsb(uint16_t addr);
    void trb(uint16_t addr);

    // Branch helper
    void branch(bool cond);

    // Record explicit PC change in ring buffer
    void recordPCChange(uint16_t fromPC, uint16_t toPC);

    // JSR trap
    uint32_t jsr_abs(uint16_t target);

    // Execute opcode
    uint32_t execute(uint8_t opcode);
  };

}  // namespace prodos8emu
