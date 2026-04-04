#pragma once

#include <cstdint>
#include <span>

namespace prodos8emu {

  enum MonitorSymbolFlag : uint8_t {
    MonitorSymbolRead  = 1 << 0,
    MonitorSymbolWrite = 1 << 1,
    MonitorSymbolPc    = 1 << 2,
  };

  struct MonitorSymbol {
    uint16_t    address;
    const char* name;
    uint8_t     flags = 0;
  };

  std::span<const MonitorSymbol> get_monitor_symbols();

}  // namespace prodos8emu
