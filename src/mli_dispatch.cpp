#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"

namespace prodos8emu {

  uint8_t mli_dispatch(MLIContext& ctx, MemoryBanks& banks, uint8_t callNumber,
                       uint16_t paramBlockAddr) {
    ConstMemoryBanks constBanks;
    for (std::size_t i = 0; i < NUM_BANKS; i++) {
      constBanks[i] = banks[i];
    }

    switch (callNumber) {
      // Housekeeping
      case 0xC0:
        return ctx.createCall(constBanks, paramBlockAddr);
      case 0xC1:
        return ctx.destroyCall(constBanks, paramBlockAddr);
      case 0xC2:
        return ctx.renameCall(constBanks, paramBlockAddr);
      case 0xC3:
        return ctx.setFileInfoCall(constBanks, paramBlockAddr);
      case 0xC4:
        return ctx.getFileInfoCall(banks, paramBlockAddr);
      case 0xC5:
        return ctx.onLineCall(banks, paramBlockAddr);
      case 0xC6:
        return ctx.setPrefixCall(constBanks, paramBlockAddr);
      case 0xC7:
        return ctx.getPrefixCall(banks, paramBlockAddr);

      // Filing
      case 0xC8:
        return ctx.openCall(banks, paramBlockAddr);
      case 0xC9:
        return ctx.newlineCall(constBanks, paramBlockAddr);
      case 0xCA:
        return ctx.readCall(banks, paramBlockAddr);
      case 0xCB:
        return ctx.writeCall(banks, paramBlockAddr);
      case 0xCC:
        return ctx.closeCall(constBanks, paramBlockAddr);
      case 0xCD:
        return ctx.flushCall(constBanks, paramBlockAddr);
      case 0xCE:
        return ctx.setMarkCall(constBanks, paramBlockAddr);
      case 0xCF:
        return ctx.getMarkCall(banks, paramBlockAddr);
      case 0xD0:
        return ctx.setEofCall(constBanks, paramBlockAddr);
      case 0xD1:
        return ctx.getEofCall(banks, paramBlockAddr);

      // Buffer
      case 0xD2:
        return ctx.setBufCall(constBanks, paramBlockAddr);
      case 0xD3:
        return ctx.getBufCall(banks, paramBlockAddr);

      // System
      case 0x40:
        return ctx.allocInterruptCall(banks, paramBlockAddr);
      case 0x41:
        return ctx.deallocInterruptCall(constBanks, paramBlockAddr);
      case 0x80:
        return ctx.readBlockCall(constBanks, paramBlockAddr);
      case 0x81:
        return ctx.writeBlockCall(constBanks, paramBlockAddr);
      case 0x82:
        return ctx.getTimeCall(banks, paramBlockAddr);

      default:
        return ERR_BAD_CALL_NUMBER;
    }
  }

}  // namespace prodos8emu
