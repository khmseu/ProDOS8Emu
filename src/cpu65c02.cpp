#include "prodos8emu/cpu65c02.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>

#include "prodos8emu/errors.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"

namespace prodos8emu {

  namespace {

    constexpr uint16_t VEC_RESET       = 0xFFFC;
    constexpr uint16_t VEC_IRQ         = 0xFFFE;
    constexpr uint16_t COUT_VECTOR_PTR = 0x0036;

    enum class NopVariantMode : uint8_t {
      Implied,
      ImmediateDiscard,
      ZpRead,
      ZpXRead,
      AbsRead,
    };

    struct NopVariantMetadata {
      uint8_t        opcode;
      NopVariantMode mode;
      uint8_t        cycles;
    };

    enum class BitFamilyKind : uint8_t {
      Bit,
      Tsb,
      Trb,
    };

    enum class BitFamilyModeMeta : uint8_t {
      Immediate,
      Zp,
      Abs,
      Zpx,
      Absx,
    };

    struct BitFamilyMetadata {
      uint8_t           opcode;
      BitFamilyKind     kind;
      BitFamilyModeMeta mode;
      bool              updateNV;
    };

    static constexpr BitFamilyMetadata kBitFamilyTable[] = {
        {0x89, BitFamilyKind::Bit, BitFamilyModeMeta::Immediate, false},
        {0x24, BitFamilyKind::Bit, BitFamilyModeMeta::Zp, true},
        {0x2C, BitFamilyKind::Bit, BitFamilyModeMeta::Abs, true},
        {0x34, BitFamilyKind::Bit, BitFamilyModeMeta::Zpx, true},
        {0x3C, BitFamilyKind::Bit, BitFamilyModeMeta::Absx, true},
        {0x04, BitFamilyKind::Tsb, BitFamilyModeMeta::Zp, false},
        {0x0C, BitFamilyKind::Tsb, BitFamilyModeMeta::Abs, false},
        {0x14, BitFamilyKind::Trb, BitFamilyModeMeta::Zp, false},
        {0x1C, BitFamilyKind::Trb, BitFamilyModeMeta::Abs, false},
    };

    const BitFamilyMetadata* find_bit_family_metadata(uint8_t opcode) {
      for (const BitFamilyMetadata& metadata : kBitFamilyTable) {
        if (metadata.opcode == opcode) {
          return &metadata;
        }
      }
      return nullptr;
    }

    enum class BitOpcodeFamily : uint8_t {
      None,
      RmbSmb,
      BbrBbs,
    };

    enum class ControlFlowBranchCondition : uint8_t {
      Always,
      FlagSet,
      FlagClear,
    };

    struct ControlFlowBranchMetadata {
      uint8_t                    opcode;
      ControlFlowBranchCondition condition;
      uint8_t                    flagMask;
      uint8_t                    cycles;
    };

    static constexpr ControlFlowBranchMetadata kControlFlowBranchTable[] = {
        {0x80, ControlFlowBranchCondition::Always, 0x00, 3},
        {0x10, ControlFlowBranchCondition::FlagClear, 0x80, 2},
        {0x30, ControlFlowBranchCondition::FlagSet, 0x80, 2},
        {0x50, ControlFlowBranchCondition::FlagClear, 0x40, 2},
        {0x70, ControlFlowBranchCondition::FlagSet, 0x40, 2},
        {0x90, ControlFlowBranchCondition::FlagClear, 0x01, 2},
        {0xB0, ControlFlowBranchCondition::FlagSet, 0x01, 2},
        {0xD0, ControlFlowBranchCondition::FlagClear, 0x02, 2},
        {0xF0, ControlFlowBranchCondition::FlagSet, 0x02, 2},
    };

    const ControlFlowBranchMetadata* find_control_flow_branch_metadata(uint8_t opcode) {
      for (const ControlFlowBranchMetadata& metadata : kControlFlowBranchTable) {
        if (metadata.opcode == opcode) {
          return &metadata;
        }
      }
      return nullptr;
    }

    BitOpcodeFamily classify_bit_opcode_family(uint8_t opcode) {
      const uint8_t lowNibble = static_cast<uint8_t>(opcode & 0x0F);
      if (lowNibble == 0x07) {
        return BitOpcodeFamily::RmbSmb;
      }
      if (lowNibble == 0x0F) {
        return BitOpcodeFamily::BbrBbs;
      }
      return BitOpcodeFamily::None;
    }

    static constexpr NopVariantMetadata kNopVariantTable[] = {
        // 1-byte, 1-cycle NOPs (no operand)
        {0x03, NopVariantMode::Implied, 1},
        {0x0B, NopVariantMode::Implied, 1},
        {0x13, NopVariantMode::Implied, 1},
        {0x1B, NopVariantMode::Implied, 1},
        {0x23, NopVariantMode::Implied, 1},
        {0x2B, NopVariantMode::Implied, 1},
        {0x33, NopVariantMode::Implied, 1},
        {0x3B, NopVariantMode::Implied, 1},
        {0x43, NopVariantMode::Implied, 1},
        {0x4B, NopVariantMode::Implied, 1},
        {0x53, NopVariantMode::Implied, 1},
        {0x5B, NopVariantMode::Implied, 1},
        {0x63, NopVariantMode::Implied, 1},
        {0x6B, NopVariantMode::Implied, 1},
        {0x73, NopVariantMode::Implied, 1},
        {0x7B, NopVariantMode::Implied, 1},
        {0x83, NopVariantMode::Implied, 1},
        {0x8B, NopVariantMode::Implied, 1},
        {0x93, NopVariantMode::Implied, 1},
        {0x9B, NopVariantMode::Implied, 1},
        {0xA3, NopVariantMode::Implied, 1},
        {0xAB, NopVariantMode::Implied, 1},
        {0xB3, NopVariantMode::Implied, 1},
        {0xBB, NopVariantMode::Implied, 1},
        {0xC3, NopVariantMode::Implied, 1},
        {0xD3, NopVariantMode::Implied, 1},
        {0xE3, NopVariantMode::Implied, 1},
        {0xEB, NopVariantMode::Implied, 1},
        {0xF3, NopVariantMode::Implied, 1},
        {0xFB, NopVariantMode::Implied, 1},

        // 2-byte, 2-cycle NOP immediate
        {0x02, NopVariantMode::ImmediateDiscard, 2},
        {0x22, NopVariantMode::ImmediateDiscard, 2},
        {0x42, NopVariantMode::ImmediateDiscard, 2},
        {0x62, NopVariantMode::ImmediateDiscard, 2},
        {0x82, NopVariantMode::ImmediateDiscard, 2},
        {0xC2, NopVariantMode::ImmediateDiscard, 2},
        {0xE2, NopVariantMode::ImmediateDiscard, 2},

        // 2-byte NOP with zp read
        {0x44, NopVariantMode::ZpRead, 3},

        // 2-byte NOP with zp,X read
        {0x54, NopVariantMode::ZpXRead, 4},
        {0xD4, NopVariantMode::ZpXRead, 4},
        {0xF4, NopVariantMode::ZpXRead, 4},

        // 3-byte NOP with absolute read
        {0xDC, NopVariantMode::AbsRead, 4},
        {0xFC, NopVariantMode::AbsRead, 4},

        // 3-byte NOP with unusual read behavior (modeled as absolute read + 8 cycles)
        {0x5C, NopVariantMode::AbsRead, 8},
    };

    const NopVariantMetadata* find_nop_variant_metadata(uint8_t opcode) {
      for (const NopVariantMetadata& metadata : kNopVariantTable) {
        if (metadata.opcode == opcode) {
          return &metadata;
        }
      }
      return nullptr;
    }

    enum class FallbackRoute : uint8_t {
      None,
      MiscTail,
      AluFamily,
      CompareXY,
      RmwFamily,
    };

    enum class AluOperandMode : uint8_t {
      IndX,
      Zp,
      Immediate,
      Abs,
      IndY,
      ZpInd,
      ZpX,
      AbsY,
      AbsX,
    };

    struct AluModeMetadata {
      uint8_t        mode;
      AluOperandMode operandMode;
      uint8_t        baseCycles;
      bool           hasPageCrossPenalty;
    };

    enum class AluGroupOperation : uint8_t {
      Ora,
      And,
      Eor,
      Adc,
      Cmp,
      Sbc,
    };

    struct AluGroupMetadata {
      uint8_t           group;
      AluGroupOperation operation;
    };

    enum class RmwTargetMode : uint8_t {
      Zp,
      ZpX,
      Abs,
      AbsX,
    };

    struct RmwModeMetadata {
      uint8_t       mode;
      RmwTargetMode targetMode;
      uint8_t       cycles;
    };

    enum class RmwGroupOperation : uint8_t {
      Asl,
      Rol,
      Lsr,
      Ror,
      Dec,
      Inc,
    };

    struct RmwGroupMetadata {
      uint8_t           group;
      RmwGroupOperation operation;
    };

    static constexpr AluModeMetadata kAluModeTable[] = {
        {0x01, AluOperandMode::IndX, 6, false},       //
        {0x05, AluOperandMode::Zp, 3, false},         //
        {0x09, AluOperandMode::Immediate, 2, false},  //
        {0x0D, AluOperandMode::Abs, 4, false},        //
        {0x11, AluOperandMode::IndY, 5, true},        //
        {0x12, AluOperandMode::ZpInd, 5, false},      //
        {0x15, AluOperandMode::ZpX, 4, false},        //
        {0x19, AluOperandMode::AbsY, 4, true},        //
        {0x1D, AluOperandMode::AbsX, 4, true},
    };

    static constexpr AluGroupMetadata kAluGroupTable[] = {
        {0x00, AluGroupOperation::Ora},  //
        {0x20, AluGroupOperation::And},  //
        {0x40, AluGroupOperation::Eor},  //
        {0x60, AluGroupOperation::Adc},  //
        {0xC0, AluGroupOperation::Cmp},  //
        {0xE0, AluGroupOperation::Sbc},
    };

    static constexpr RmwModeMetadata kRmwModeTable[] = {
        {0x06, RmwTargetMode::Zp, 5},
        {0x16, RmwTargetMode::ZpX, 6},
        {0x0E, RmwTargetMode::Abs, 6},
        {0x1E, RmwTargetMode::AbsX, 7},
    };

    static constexpr RmwGroupMetadata kRmwGroupTable[] = {
        {0x00, RmwGroupOperation::Asl},  //
        {0x20, RmwGroupOperation::Rol},  //
        {0x40, RmwGroupOperation::Lsr},  //
        {0x60, RmwGroupOperation::Ror},  //
        {0xC0, RmwGroupOperation::Dec},  //
        {0xE0, RmwGroupOperation::Inc},
    };

    const AluModeMetadata* find_alu_mode_metadata(uint8_t mode) {
      for (const AluModeMetadata& metadata : kAluModeTable) {
        if (metadata.mode == mode) {
          return &metadata;
        }
      }
      return nullptr;
    }

    const AluGroupMetadata* find_alu_group_metadata(uint8_t group) {
      for (const AluGroupMetadata& metadata : kAluGroupTable) {
        if (metadata.group == group) {
          return &metadata;
        }
      }
      return nullptr;
    }

    const RmwModeMetadata* find_rmw_mode_metadata(uint8_t mode) {
      for (const RmwModeMetadata& metadata : kRmwModeTable) {
        if (metadata.mode == mode) {
          return &metadata;
        }
      }
      return nullptr;
    }

    const RmwGroupMetadata* find_rmw_group_metadata(uint8_t group) {
      for (const RmwGroupMetadata& metadata : kRmwGroupTable) {
        if (metadata.group == group) {
          return &metadata;
        }
      }
      return nullptr;
    }

    bool is_fallback_misc_tail_opcode(uint8_t opcode) {
      return opcode == 0xE8 || opcode == 0xCA || opcode == 0xC8 || opcode == 0x88;
    }

    bool is_fallback_alu_rmw_group(uint8_t group) {
      return find_alu_group_metadata(group) != nullptr;
    }

    bool is_fallback_alu_mode(uint8_t mode) {
      return find_alu_mode_metadata(mode) != nullptr;
    }

    bool is_fallback_compare_mode(uint8_t mode) {
      return mode == 0x00 || mode == 0x04 || mode == 0x0C;
    }

    bool is_fallback_rmw_mode(uint8_t mode) {
      return find_rmw_mode_metadata(mode) != nullptr;
    }

    FallbackRoute classify_fallback_route(uint8_t opcode) {
      if (is_fallback_misc_tail_opcode(opcode)) {
        return FallbackRoute::MiscTail;
      }

      const uint8_t mode  = static_cast<uint8_t>(opcode & 0x1F);
      const uint8_t group = static_cast<uint8_t>(opcode & 0xE0);

      if (is_fallback_alu_rmw_group(group) && is_fallback_alu_mode(mode)) {
        return FallbackRoute::AluFamily;
      }

      if ((group == 0xC0 || group == 0xE0) && is_fallback_compare_mode(mode)) {
        return FallbackRoute::CompareXY;
      }

      if (is_fallback_alu_rmw_group(group) && is_fallback_rmw_mode(mode)) {
        return FallbackRoute::RmwFamily;
      }

      return FallbackRoute::None;
    }

    inline uint16_t make_u16(uint8_t lo, uint8_t hi) {
      return static_cast<uint16_t>(static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8));
    }

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

    static const MonitorSymbol kMonitorSymbols[] = {
        {0x0000, "LOC0"},      //
        {0x0000, "Z00"},       //
        {0x0001, "LOC1"},      //
        {0x0008, "Reg4"},      //
        {0x000A, "LoMem"},     //
        {0x000A, "TxtBgn"},    //
        {0x000C, "HiMem"},     //
        {0x000E, "TxtEnd"},    //
        {0x0010, "Z10"},       //
        {0x0012, "Z12"},       //
        {0x0018, "Reg12"},     //
        {0x0020, "WNDLWFT"},   //
        {0x0021, "WNDWDTH"},   //
        {0x0022, "WNDTOP"},    //
        {0x0023, "WNDBTM"},    //
        {0x0024, "CH"},        //
        {0x0025, "SV"},        //
        {0x0028, "BASL"},      //
        {0x0029, "BASH"},      //
        {0x002A, "BAS2L"},     //
        {0x002B, "BAS2H"},     //
        {0x0032, "INVFLG"},    //
        {0x0033, "Prompt"},    //
        {0x0035, "YSAV1"},     //
        {0x0036, "CSWL"},      //
        {0x0038, "KSWL"},      //
        {0x003C, "A1"},        //
        {0x003C, "A1L"},       //
        {0x003D, "A1H"},       //
        {0x003E, "A2"},        //
        {0x003E, "A2L"},       //
        {0x003F, "A2H"},       //
        {0x0042, "A4"},        //
        {0x0042, "A4L"},       //
        {0x0043, "A4H"},       //
        {0x0048, "STATUS"},    //
        {0x0050, "VideoSlt"},  //
        {0x0051, "FileType"},  //
        {0x0053, "ExecMode"},  //
        {0x0054, "PtrMode"},   //
        {0x005F, "TabChar"},   //
        //>>> 0x0060
        //>>> 0x0060
        //>>> 0x0060
        //>>> 0x0060
        {0x0060, "BCDNbr"},    //
        {0x0060, "Z60"},       //
        {0x0061, "PrColumn"},  //
        //>>> 0x0063
        //>>> 0x0063
        //>>> 0x0063
        //>>> 0x0063
        {0x0063, "StrtSymT"},                                         //
        {0x0065, "EndSymT"},                                          //
        {0x0067, "PassNbr", MonitorSymbolRead | MonitorSymbolWrite},  //
        {0x0068, "ListingF", MonitorSymbolWrite},                     //
        {0x0068, "UserTabT"},                                         //
        {0x0069, "SubTtlF", MonitorSymbolWrite},                      //
        {0x006A, "LineCnt", MonitorSymbolWrite},                      //
        {0x006B, "PageNbr", MonitorSymbolWrite},                      //
        {0x006D, "FileNbr", MonitorSymbolWrite},                      //
        {0x006E, "LogPL", MonitorSymbolWrite},                        //
        {0x006F, "PhyPL", MonitorSymbolWrite},                        //
        {0x0071, "PrtCol"},                                           //
        {0x0072, "EIStack", MonitorSymbolWrite},                      //
        {0x0073, "CancelF"},                                          //
        {0x0073, "PrintF"},                                           //
        {0x0074, "NbrErrs", MonitorSymbolWrite},                      //
        {0x0074, "SwapMode"},                                         //
        {0x0075, "CaseMode"},                                         //
        {0x0076, "PrSlot", MonitorSymbolWrite},                       //
        {0x0077, "AbortF"},                                           //
        {0x0077, "MulCmdF"},                                          //
        {0x0078, "CmdDelim"},                                         //
        {0x0078, "SavIndY"},                                          //
        {0x0079, "SrcP"},                                             //
        {0x0079, "TruncF"},                                           //
        {0x0079, "UnsortedP"},                                        //
        {0x007B, "Src2P"},                                            //
        {0x007C, "HelpF"},                                            //
        {0x007D, "PC"},                                               //
        {0x007D, "SortedP"},                                          //
        {0x007F, "AuxAryE"},                                          //
        {0x007F, "CodeLen"},                                          //
        {0x007F, "ObjPC"},                                            //
        {0x007F, "SymFBP"},                                           //
        {0x0081, "FileLen"},                                          //
        {0x0083, "CurrORG"},                                          //
        {0x0085, "MnemP"},                                            //
        {0x0085, "SymP"},                                             //
        {0x0087, "MemTop"},                                           //
        {0x0089, "TotLines"},                                         //
        {0x008C, "VidSlot", MonitorSymbolWrite},                      //
        {0x008D, "SaveA"},                                            //
        {0x008E, "SaveY"},                                            //
        {0x008F, "SaveX"},                                            //
        {0x0090, "DskListF", MonitorSymbolWrite},                     //
        {0x0092, "WinLeft", MonitorSymbolWrite},                      //
        {0x0093, "WinRight", MonitorSymbolWrite},                     //
        {0x0094, "X6502F"},                                           //
        {0x0095, "HighMem"},                                          //
        {0x0097, "ColCnt"},                                           //
        {0x0097, "ExprAccF"},                                         //
        {0x0098, "NxtToken"},                                         //
        {0x0098, "SortF"},                                            //
        {0x0099, "LstCodeF", MonitorSymbolWrite},                     //
        {0x0099, "SymRefCh"},                                         //
        {0x009A, "GMC"},                                              //
        {0x009A, "IsFwdRef"},                                         //
        {0x009B, "NumCols", MonitorSymbolWrite},                      //
        {0x009C, "SymIdx"},                                           //
        {0x009D, "ERfield"},                                          //
        {0x009D, "SymAddr"},                                          //
        {0x009F, "ValExpr"},                                          //
        {0x00A3, "Length"},                                           //
        {0x00A4, "ModWrdL"},                                          //
        {0x00A5, "ModWrdH"},                                          //
        {0x00A6, "LenTIdx"},                                          //
        {0x00A7, "GMCIdx"},                                           //
        {0x00A7, "Jump"},                                             //
        {0x00A7, "RadixCh"},                                          //
        {0x00A7, "SavLstF", MonitorSymbolWrite},                      //
        {0x00A8, "BitsDig"},                                          //
        {0x00A9, "LabelF"},                                           //
        {0x00A9, "NumRecs"},                                          //
        {0x00A9, "RecCnt"},                                           //
        {0x00AA, "SubTIdx"},                                          //
        {0x00AB, "ZAB"},                                              //
        {0x00AC, "ErrorF"},                                           //
        {0x00AC, "ErrTIdx"},                                          //
        {0x00AD, "msbF"},                                             //
        {0x00AF, "Accum"},                                            //
        {0x00B1, "Ret816F"},                                          //
        {0x00B2, "RepChar"},                                          //
        {0x00B3, "SavSTS"},                                           //
        {0x00B3, "SymNbr"},                                           //
        {0x00B4, "GblAbsF"},                                          //
        {0x00B5, "DummyF"},                                           //
        {0x00BA, "CondAsmF"},                                         //
        {0x00BB, "TabTIdx"},                                          //
        {0x00BC, "SymFByte"},                                         //
        {0x00BD, "RelCodeF"},                                         //
        {0x00BE, "DskSrcF"},                                          //
        {0x00BF, "GenF", MonitorSymbolRead | MonitorSymbolWrite},     //
        {0x00C0, "ObjDBIdx"},                                         //
        {0x00C1, "IDskSrcF"},                                         //
        {0x00C2, "MacroF"},                                           //
        {0x00C6, "FCTIndex"},                                         //
        {0x00C7, "PathP"},                                            //
        {0x00C9, "SrcPathP"},                                         //
        {0x00CB, "RelExprF"},                                         //
        {0x00CC, "SavSTE"},                                           //
        {0x00CD, "NewF"},                                             //
        {0x00CE, "Msg2P"},                                            //
        {0x00CE, "ParmBIdx"},                                         //
        {0x00CE, "SymNodeP"},                                         //
        {0x00CE, "ZCE"},                                              //
        {0x00CF, "ZCF"},                                              //
        {0x00D0, "RLDEnd", MonitorSymbolWrite},                       //
        {0x00D5, "MsgP"},                                             //
        {0x00D5, "SBufP"},                                            //
        {0x00D7, "HashIdx"},                                          //
        {0x00D8, "PrvSymP"},                                          //
        {0x00DD, "NumCycles"},                                        //
        {0x00DE, "NbrWarns"},                                         //
        {0x00E0, "LstCyc", MonitorSymbolWrite},                       //
        {0x00E1, "LstUnAsm", MonitorSymbolWrite},                     //
        {0x00E2, "LstExpMac", MonitorSymbolWrite},                    //
        {0x00E3, "LstWarns", MonitorSymbolWrite},                     //
        {0x00E4, "LstGCode", MonitorSymbolWrite},                     //
        {0x00E5, "LstASym", MonitorSymbolWrite},                      //
        {0x00E6, "LstVSym", MonitorSymbolWrite},                      //
        {0x00E7, "Lst6Cols", MonitorSymbolWrite},                     //
        {0x00E8, "ZE8"},                                              //
        {0x00E9, "SW16F"},                                            //
        {0x00EA, "ZPSaveY"},                                          //
        {0x00EB, "ErrNbr4"},                                          //
        {0x00ED, "DecimalS"},                                         //
        {0x0100, "Stack"},                                            //
        {0x0200, "InBuf"},                                            //
        {0x0280, "TxBuf2"},                                           //
        {0x03F2, "SOFTEV"},                                           //
        {0x03F4, "PWREDUP"},                                          //
        {0x03F8, "USRADR"},                                           //
        {0x2000, "L2000", MonitorSymbolPc},                           //
        {0x2007, "L2007", MonitorSymbolPc},                           //
        {0x2011, "L2011", MonitorSymbolPc},                           //
        {0x201D, "L201D", MonitorSymbolPc},                           //
        {0x201F, "L201F", MonitorSymbolPc},                           //
        {0x2022, "L2022", MonitorSymbolPc},                           //
        {0x202D, "L202D", MonitorSymbolPc},                           //
        {0x2044, "L2044", MonitorSymbolPc},                           //
        {0x204A, "L204A", MonitorSymbolPc},                           //
        {0x20B6, "L20B6", MonitorSymbolPc},                           //
        {0x20E1, "L20E1", MonitorSymbolPc},                           //
        {0x20E5, "L20E5", MonitorSymbolPc},                           //
        {0x20FC, "L20FC", MonitorSymbolPc},                           //
        {0x213A, "OLUnit"},                                           //
        {0x213D, "ShowErr", MonitorSymbolPc},                         //
        {0x2342, "L2342", MonitorSymbolPc},                           //
        {0x235A, "L235A", MonitorSymbolPc},                           //
        {0x237A, "L237A", MonitorSymbolPc},                           //
        {0x7800, "Assembler", MonitorSymbolPc},                       // must have
        {0x7800, "ColdStrt", MonitorSymbolPc},                        //
        {0x7800, "X7800", MonitorSymbolPc},                           //
        {0x7805, "DateStr"},                                          //
        {0x7814, "ResetEnt"},                                         //
        {0x7817, "ExecAsm", MonitorSymbolPc},                         //
        {0x782D, "CleanUp", MonitorSymbolPc},                         //
        {0x7843, "CleanUp2", MonitorSymbolPc},                        //
        {0x7848, "FlushObj", MonitorSymbolPc},                        //
        {0x784B, "ListSymTbl", MonitorSymbolPc},                      //
        {0x7859, "AbortAsm", MonitorSymbolPc},                        //
        {0x785D, "ListErrs", MonitorSymbolPc},                        //
        {0x7866, "TellUser", MonitorSymbolPc},                        //
        {0x7869, "EndAsm", MonitorSymbolPc},                          //
        {0x7875, "ExitASM", MonitorSymbolPc},                         //
        {0x78AE, "SetupVec", MonitorSymbolPc},                        //
        {0x78DB, "PrtEndAsm", MonitorSymbolPc},                       //
        {0x78F6, "PrtErrs", MonitorSymbolPc},                         //
        {0x790D, "GudAsm", MonitorSymbolPc},                          //
        {0x7912, "PrtCrMsg", MonitorSymbolPc},                        //
        {0x7919, "PrtLoop", MonitorSymbolPc},                         //
        {0x794E, "PrtLoop1", MonitorSymbolPc},                        //
        {0x7958, "PrtFreeMem", MonitorSymbolPc},                      //
        {0x795E, "doRTS", MonitorSymbolPc},                           //
        {0x795F, "MsgPrtr", MonitorSymbolPc},                         //
        {0x796C, "ChkEndMsg", MonitorSymbolPc},                       //
        {0x7972, "SummaryStr"},                                       //
        {0x7980, "PrSummry", MonitorSymbolPc},                        //
        {0x798C, "PrtLoop2", MonitorSymbolPc},                        //
        {0x7995, "PrtAsmErr", MonitorSymbolPc},                       //
        {0x799C, "PrtErrLin", MonitorSymbolPc},                       //
        {0x79C6, "PrtLoop3", MonitorSymbolPc},                        //
        {0x79E6, "StopPrt", MonitorSymbolPc},                         //
        {0x79EB, "RegAsmEW", MonitorSymbolPc},                        //
        {0x79F0, "NewErrWarn", MonitorSymbolPc},                      //
        {0x7A0D, "FlagErr", MonitorSymbolPc},                         //
        {0x7A14, "SkipIt", MonitorSymbolPc},                          //
        {0x7A1B, "FlagErrZ", MonitorSymbolPc},                        //
        {0x7A1E, "doPause", MonitorSymbolPc},                         //
        {0x7A25, "DelayLup", MonitorSymbolPc},                        //
        {0x7A33, "PutCR", MonitorSymbolPc},                           //
        {0x7A38, "DoAlert", MonitorSymbolPc},                         //
        {0x7A45, "PrtLoop4", MonitorSymbolPc},                        //
        {0x7A61, "RingBell", MonitorSymbolPc},                        //
        {0x7A66, "PrtBlnk", MonitorSymbolPc},                         //
        {0x7A6B, "PrtErrMsg", MonitorSymbolPc},                       //
        {0x7A74, "PrtEM1", MonitorSymbolPc},                          //
        {0x7A82, "PrtEMLoop", MonitorSymbolPc},                       //
        {0x7A8E, "L7A8E", MonitorSymbolPc},                           //
        {0x7A9D, "SkipIt2", MonitorSymbolPc},                         //
        {0x7AA6, "PrtLoop5", MonitorSymbolPc},                        //
        {0x7AB3, "CountErr", MonitorSymbolPc},                        //
        {0x7AC3, "SaveErrInfo", MonitorSymbolPc},                     //
        {0x7ACB, "NotStd", MonitorSymbolPc},                          //
        {0x7ACD, "Is2Many", MonitorSymbolPc},                         //
        {0x7AF4, "doRTS2", MonitorSymbolPc},                          //
        {0x7AF7, "Bit08"},                                            //
        {0x7AF9, "Bit40"},                                            //
        {0x7AFA, "SaveZP", MonitorSymbolPc},                          //
        {0x7B02, "SavLoop", MonitorSymbolPc},                         //
        {0x7B13, "InitASM", MonitorSymbolPc},                         //
        {0x7B2C, "Not80", MonitorSymbolPc},                           //
        {0x7BB5, "ZeroLoop", MonitorSymbolPc},                        //
        {0x7BBF, "BlnkLoop", MonitorSymbolPc},                        //
        {0x7BD2, "CpyLoop", MonitorSymbolPc},                         //
        {0x7BFE, "InitA1", MonitorSymbolPc},                          //
        {0x7C0C, "ZeroLoop1", MonitorSymbolPc},                       //
        {0x7C13, "ZeroLnCnt", MonitorSymbolPc},                       //
        {0x7C2C, "OpenSrc1", MonitorSymbolPc},                        //
        {0x7C3A, "CpySrcPN", MonitorSymbolPc},                        //
        {0x7C3C, "CpyLoop1", MonitorSymbolPc},                        //
        {0x7C4D, "InitFlags", MonitorSymbolPc},                       //
        {0x7C7C, "ZeroLup", MonitorSymbolPc},                         //
        {0x7C83, "GetSrcPN", MonitorSymbolPc},                        //
        {0x7C8A, "MovLoop", MonitorSymbolPc},                         //
        {0x7CAD, "GoodPN", MonitorSymbolPc},                          //
        {0x7CB3, "doRTS3", MonitorSymbolPc},                          //
        {0x7CB4, "GetObjPN", MonitorSymbolPc},                        //
        {0x7CC7, "MovLoop1", MonitorSymbolPc},                        //
        {0x7CCE, "SkipLoop", MonitorSymbolPc},                        //
        {0x7CDF, "GoodObjPN", MonitorSymbolPc},                       //
        {0x7CE7, "MkObjPN", MonitorSymbolPc},                         //
        {0x7D28, "doRTS4", MonitorSymbolPc},                          //
        {0x7D29, "PrtSetup", MonitorSymbolPc},                        //
        {0x7D33, "ValROM", MonitorSymbolPc},                          //
        {0x7E19, "L7E19", MonitorSymbolPc},                           //
        {0x7E1B, "L7E1B", MonitorSymbolPc},                           //
        {0x7E25, "ToUpper", MonitorSymbolPc},                         //
        {0x7E2F, "NotAlfa", MonitorSymbolPc},                         //
        {0x7E30, "DoPass1", MonitorSymbolPc},                         //
        {0x7E40, "Pass1Lup", MonitorSymbolPc},                        //
        {0x7E46, "L7E46", MonitorSymbolPc},                           //
        {0x7E5A, "L7E5A", MonitorSymbolPc},                           //
        {0x7E62, "L7E62", MonitorSymbolPc},                           //
        {0x7E6A, "ChkCommLin", MonitorSymbolPc},                      //
        {0x7E74, "L7E74", MonitorSymbolPc},                           //
        {0x7E77, "ChkLabel", MonitorSymbolPc},                        //
        {0x7EAE, "NewLabel", MonitorSymbolPc},                        //
        {0x7EBA, "L7EBA", MonitorSymbolPc},                           //
        {0x7EC8, "L7EC8", MonitorSymbolPc},                           //
        {0x7EDD, "L7EDD", MonitorSymbolPc},                           //
        {0x7EE5, "L7EE5", MonitorSymbolPc},                           //
        {0x7EF0, "L7EF0", MonitorSymbolPc},                           //
        {0x7EFE, "L7EFE", MonitorSymbolPc},                           //
        {0x7F01, "L7F01", MonitorSymbolPc},                           //
        {0x7F04, "L7F04", MonitorSymbolPc},                           //
        {0x7F0D, "DoPass2", MonitorSymbolPc},                         //
        {0x7F2D, "Pass2Lup", MonitorSymbolPc},                        //
        {0x7F33, "L7F33", MonitorSymbolPc},                           //
        {0x7F50, "L7F50", MonitorSymbolPc},                           //
        {0x7F57, "L7F57", MonitorSymbolPc},                           //
        {0x7F6E, "L7F6E", MonitorSymbolPc},                           //
        {0x7F73, "L7F73", MonitorSymbolPc},                           //
        {0x7F77, "L7F77", MonitorSymbolPc},                           //
        {0x7F7A, "L7F7A", MonitorSymbolPc},                           //
        {0x7F88, "L7F88", MonitorSymbolPc},                           //
        {0x7F99, "Bit80"},                                            //
        {0x7F9A, "CodeGen", MonitorSymbolPc},                         //
        {0x7FCB, "L7FCB", MonitorSymbolPc},                           //
        {0x7FED, "L7FED", MonitorSymbolPc},                           //
        {0x800A, "GenNow", MonitorSymbolPc},                          //
        {0x8025, "L8025", MonitorSymbolPc},                           //
        {0x8038, "L8038", MonitorSymbolPc},                           //
        {0x806F, "L806F", MonitorSymbolPc},                           //
        {0x807A, "L807A", MonitorSymbolPc},                           //
        {0x8088, "L8088", MonitorSymbolPc},                           //
        {0x808B, "RVLsting", MonitorSymbolPc},                        //
        {0x8098, "L8098", MonitorSymbolPc},                           //
        {0x80A0, "L80A0", MonitorSymbolPc},                           //
        {0x80A5, "doRTS5", MonitorSymbolPc},                          //
        {0x80A6, "PrtAsmLn", MonitorSymbolPc},                        //
        {0x80B1, "doRTS6", MonitorSymbolPc},                          //
        {0x80B2, "StorGMC", MonitorSymbolPc},                         //
        {0x80B8, "L80B8", MonitorSymbolPc},                           //
        {0x80C5, "L80C5", MonitorSymbolPc},                           //
        {0x80C8, "L80C8", MonitorSymbolPc},                           //
        {0x80D5, "doRTS7", MonitorSymbolPc},                          //
        {0x81A3, "L81A3", MonitorSymbolPc},                           //
        {0x81AF, "L81AF", MonitorSymbolPc},                           //
        {0x81C7, "L81C7", MonitorSymbolPc},                           //
        {0x81E4, "L81E4", MonitorSymbolPc},                           //
        {0x81E5, "doRTS9", MonitorSymbolPc},                          //
        {0x81E6, "SkipSpcs", MonitorSymbolPc},                        //
        {0x81EF, "L81EF", MonitorSymbolPc},                           //
        {0x81F0, "L81F0", MonitorSymbolPc},                           //
        {0x81FF, "WhiteSpc", MonitorSymbolPc},                        //
        {0x8207, "doRet", MonitorSymbolPc},                           //
        {0x8208, "ChrGet", MonitorSymbolPc},                          //
        {0x8209, "ChrGot", MonitorSymbolPc},                          //
        {0x8211, "L8211", MonitorSymbolPc},                           //
        {0x821D, "doRet2", MonitorSymbolPc},                          //
        {0x821E, "ChrGet2", MonitorSymbolPc},                         //
        {0x821F, "ChrGot2", MonitorSymbolPc},                         //
        {0x8227, "L8227", MonitorSymbolPc},                           //
        {0x8233, "doRet3", MonitorSymbolPc},                          //
        {0x8234, "NxtField", MonitorSymbolPc},                        //
        {0x823D, "L823D", MonitorSymbolPc},                           //
        {0x824B, "NextRec", MonitorSymbolPc},                         //
        {0x824D, "L824D", MonitorSymbolPc},                           //
        {0x8254, "AdvSrcP", MonitorSymbolPc},                         //
        {0x8262, "AdvPC", MonitorSymbolPc},                           //
        {0x826B, "doRet4", MonitorSymbolPc},                          //
        {0x8295, "PollKbd", MonitorSymbolPc},                         //
        {0x829A, "L829A", MonitorSymbolPc},                           //
        {0x829C, "L829C", MonitorSymbolPc},                           //
        {0x8317, "IsC02Op", MonitorSymbolPc},                         //
        {0x8319, "L8319", MonitorSymbolPc},                           //
        {0x8326, "doRet5", MonitorSymbolPc},                          //
        {0x8327, "L8327"},                                            //
        {0x8339, "HndlMnem", MonitorSymbolPc},                        //
        {0x8348, "L8348", MonitorSymbolPc},                           //
        {0x8359, "L8359", MonitorSymbolPc},                           //
        {0x8361, "L8361", MonitorSymbolPc},                           //
        {0x837C, "L837C", MonitorSymbolPc},                           //
        {0x837E, "L837E", MonitorSymbolPc},                           //
        {0x8385, "L8385", MonitorSymbolPc},                           //
        {0x8386, "L8386", MonitorSymbolPc},                           //
        {0x8392, "L8392", MonitorSymbolPc},                           //
        {0x839C, "L839C", MonitorSymbolPc},                           //
        {0x83AC, "L83AC", MonitorSymbolPc},                           //
        {0x8458, "GInstLen", MonitorSymbolPc},                        //
        {0x84CE, "L84CE", MonitorSymbolPc},                           //
        {0x84D4, "L84D4", MonitorSymbolPc},                           //
        {0x84D8, "Bit20"},                                            //
        {0x8511, "L8511", MonitorSymbolPc},                           //
        {0x8513, "L8513", MonitorSymbolPc},                           //
        {0x85E5, "EvalExpr", MonitorSymbolPc},                        //
        {0x8601, "L8601", MonitorSymbolPc},                           //
        {0x8608, "L8608", MonitorSymbolPc},                           //
        {0x8610, "L8610", MonitorSymbolPc},                           //
        {0x8618, "L8618", MonitorSymbolPc},                           //
        {0x8627, "L8627", MonitorSymbolPc},                           //
        {0x8628, "L8628", MonitorSymbolPc},                           //
        {0x862C, "L862C", MonitorSymbolPc},                           //
        {0x8636, "L8636", MonitorSymbolPc},                           //
        {0x8662, "L8662", MonitorSymbolPc},                           //
        {0x8674, "L8674", MonitorSymbolPc},                           //
        {0x867B, "L867B", MonitorSymbolPc},                           //
        {0x869C, "L869C", MonitorSymbolPc},                           //
        {0x86AC, "GNToken", MonitorSymbolPc},                         //
        {0x86C4, "doRet7", MonitorSymbolPc},                          //
        {0x86C5, "EvalTerm", MonitorSymbolPc},                        //
        {0x86D6, "L86D6", MonitorSymbolPc},                           //
        {0x8716, "L8716", MonitorSymbolPc},                           //
        {0x8781, "L8781", MonitorSymbolPc},                           //
        {0x879E, "L879E", MonitorSymbolPc},                           //
        {0x87AF, "L87AF", MonitorSymbolPc},                           //
        {0x87B9, "L87B9", MonitorSymbolPc},                           //
        {0x87C3, "L87C3", MonitorSymbolPc},                           //
        {0x87CB, "L87CB", MonitorSymbolPc},                           //
        {0x87CF, "L87CF", MonitorSymbolPc},                           //
        {0x87DA, "L87DA", MonitorSymbolPc},                           //
        {0x87E8, "L87E8", MonitorSymbolPc},                           //
        {0x87E9, "L87E9", MonitorSymbolPc},                           //
        {0x87ED, "L87ED", MonitorSymbolPc},                           //
        {0x87F9, "L87F9", MonitorSymbolPc},                           //
        {0x8808, "L8808", MonitorSymbolPc},                           //
        {0x880F, "L880F", MonitorSymbolPc},                           //
        {0x8887, "Operators"},                                        //
        {0x88C3, "FindSym", MonitorSymbolPc},                         //
        {0x8908, "L8908", MonitorSymbolPc},                           //
        {0x891D, "HashFn", MonitorSymbolPc},                          //
        {0x8947, "L8947", MonitorSymbolPc},                           //
        {0x8968, "L8968", MonitorSymbolPc},                           //
        {0x897D, "L897D", MonitorSymbolPc},                           //
        {0x8984, "RsvdId", MonitorSymbolPc},                          //
        {0x898E, "IsAXY", MonitorSymbolPc},                           //
        {0x899F, "L899F", MonitorSymbolPc},                           //
        {0x89A7, "L89A7", MonitorSymbolPc},                           //
        {0x89A8, "doRet9", MonitorSymbolPc},                          //
        {0x89A9, "AddNode", MonitorSymbolPc},                         //
        {0x89B4, "L89B4", MonitorSymbolPc},                           //
        {0x89C8, "L89C8", MonitorSymbolPc},                           //
        {0x89E3, "L89E3", MonitorSymbolPc},                           //
        {0x89E6, "L89E6", MonitorSymbolPc},                           //
        {0x8A00, "L8A00", MonitorSymbolPc},                           //
        {0x8A1E, "L8A1E", MonitorSymbolPc},                           //
        {0x8A30, "doRtn3", MonitorSymbolPc},                          //
        {0x8A50, "L8A50", MonitorSymbolPc},                           //
        {0x8A58, "DrtvFin", MonitorSymbolPc},                         //
        {0x8A82, "L8A82", MonitorSymbolPc},                           //
        {0x8A9A, "L8A9A", MonitorSymbolPc},                           //
        {0x8A9E, "L8A9E", MonitorSymbolPc},                           //
        {0x8AA1, "L8AA1", MonitorSymbolPc},                           //
        {0x8AAE, "L8AAE", MonitorSymbolPc},                           //
        {0x8AD7, "L8AD7", MonitorSymbolPc},                           //
        {0x8AF5, "L8AF5", MonitorSymbolPc},                           //
        {0x8B05, "L8B05", MonitorSymbolPc},                           //
        {0x8B2C, "L8B2C", MonitorSymbolPc},                           //
        {0x8B90, "SetPC", MonitorSymbolPc},                           //
        {0x8BFF, "EvalOprnd", MonitorSymbolPc},                       //
        {0x8D4C, "DrtvDone", MonitorSymbolPc},                        //
        {0x8ECA, "L8ECA", MonitorSymbolPc},                           //
        {0x8EE9, "L8EE9", MonitorSymbolPc},                           //
        {0x8EEE, "L8EEE", MonitorSymbolPc},                           //
        {0x8EF3, "L8EF3", MonitorSymbolPc},                           //
        {0x8F22, "L8F22", MonitorSymbolPc},                           //
        {0x8F34, "L8F34", MonitorSymbolPc},                           //
        {0x8F37, "L8F37", MonitorSymbolPc},                           //
        {0x92F0, "L92F0", MonitorSymbolPc},                           //
        {0x92F8, "L92F8", MonitorSymbolPc},                           //
        {0x9306, "L9306", MonitorSymbolPc},                           //
        {0x9334, "L9334", MonitorSymbolPc},                           //
        {0x935F, "doRtn6", MonitorSymbolPc},                          //
        {0x9508, "ListCode", MonitorSymbolPc},                        //
        {0x9528, "doRtn8", MonitorSymbolPc},                          //
        {0x9529, "ListPC", MonitorSymbolPc},                          //
        {0x9545, "Pr1BL", MonitorSymbolPc},                           //
        {0x954B, "LCLoop", MonitorSymbolPc},                          //
        {0x9568, "PrMCode", MonitorSymbolPc},                         //
        {0x9576, "LCDone", MonitorSymbolPc},                          //
        {0x959D, "L959D", MonitorSymbolPc},                           //
        {0x95A2, "LstSrcLn", MonitorSymbolPc},                        //
        {0x95AB, "L95AB", MonitorSymbolPc},                           //
        {0x95B3, "L95B3", MonitorSymbolPc},                           //
        {0x95BF, "L95BF", MonitorSymbolPc},                           //
        {0x95C7, "L95C7", MonitorSymbolPc},                           //
        {0x95E3, "L95E3", MonitorSymbolPc},                           //
        {0x95F3, "L95F3", MonitorSymbolPc},                           //
        {0x95F5, "L95F5", MonitorSymbolPc},                           //
        {0x95F8, "L95F8", MonitorSymbolPc},                           //
        {0x95FE, "L95FE", MonitorSymbolPc},                           //
        {0x9603, "PutC", MonitorSymbolPc},                            //
        {0x961E, "L961E", MonitorSymbolPc},                           //
        {0x962E, "L962E", MonitorSymbolPc},                           //
        {0x9631, "L9631", MonitorSymbolPc},                           //
        {0x9632, "L9632", MonitorSymbolPc},                           //
        {0x963A, "L963A", MonitorSymbolPc},                           //
        {0x9642, "L9642", MonitorSymbolPc},                           //
        {0x9652, "L9652", MonitorSymbolPc},                           //
        {0x9666, "L9666", MonitorSymbolPc},                           //
        {0x9671, "L9671", MonitorSymbolPc},                           //
        {0x9674, "L9674", MonitorSymbolPc},                           //
        {0x967B, "DoLF", MonitorSymbolPc},                            //
        {0x9685, "PrBlnkX", MonitorSymbolPc},                         //
        {0x968E, "PrtPC", MonitorSymbolPc},                           //
        {0x969C, "PrByte", MonitorSymbolPc},                          //
        {0x96A5, "PrHex", MonitorSymbolPc},                           //
        {0x96AF, "L96AF", MonitorSymbolPc},                           //
        {0x96B2, "PrtDecS", MonitorSymbolPc},                         //
        {0x96C1, "L96C1", MonitorSymbolPc},                           //
        {0x96C2, "L96C2", MonitorSymbolPc},                           //
        {0x96CE, "L96CE", MonitorSymbolPc},                           //
        {0x96DC, "L96DC", MonitorSymbolPc},                           //
        {0x96ED, "L96ED", MonitorSymbolPc},                           //
        {0x96F5, "L96F5", MonitorSymbolPc},                           //
        {0x96FE, "MonCOUT", MonitorSymbolPc},                         //
        {0x970C, "PrtChar", MonitorSymbolPc},                         //
        {0x9715, "L9715", MonitorSymbolPc},                           //
        {0x9721, "L9721", MonitorSymbolPc},                           //
        {0x9786, "L9786", MonitorSymbolPc},                           //
        {0x9797, "L9797", MonitorSymbolPc},                           //
        {0x979C, "L979C", MonitorSymbolPc},                           //
        {0x979F, "L979F", MonitorSymbolPc},                           //
        {0x97A4, "L97A4", MonitorSymbolPc},                           //
        {0x97A9, "L97A9", MonitorSymbolPc},                           //
        {0x9880, "VidOut", MonitorSymbolPc},                          //
        {0x989D, "L989D", MonitorSymbolPc},                           //
        {0x98A2, "L98A2", MonitorSymbolPc},                           //
        {0x98CB, "PrtFF", MonitorSymbolPc},                           //
        {0x98D5, "L98D5", MonitorSymbolPc},                           //
        {0x98DD, "IsVideo", MonitorSymbolPc},                         //
        {0x98E3, "Exit1", MonitorSymbolPc},                           //
        {0x98EE, "Open4RW", MonitorSymbolPc},                         //
        {0x991A, "L991A", MonitorSymbolPc},                           //
        {0x9923, "L9923", MonitorSymbolPc},                           //
        {0x9948, "L9948", MonitorSymbolPc},                           //
        {0x9962, "L9962", MonitorSymbolPc},                           //
        {0x9975, "L9975", MonitorSymbolPc},                           //
        {0x997E, "L997E", MonitorSymbolPc},                           //
        {0x9981, "L9981", MonitorSymbolPc},                           //
        {0x9982, "L9982", MonitorSymbolPc},                           //
        {0x9989, "L9989", MonitorSymbolPc},                           //
        {0x998C, "L998C", MonitorSymbolPc},                           //
        {0x99A2, "L99A2", MonitorSymbolPc},                           //
        {0x99AA, "L99AA", MonitorSymbolPc},                           //
        {0x99B1, "ClsFile", MonitorSymbolPc},                         //
        {0x99BD, "L99BD", MonitorSymbolPc},                           //
        {0x99C8, "ClsFileX", MonitorSymbolPc},                        //
        {0x99CD, "L99CD", MonitorSymbolPc},                           //
        {0x99D6, "L99D6", MonitorSymbolPc},                           //
        {0x99DB, "L99DB", MonitorSymbolPc},                           //
        {0x99DC, "L99DC", MonitorSymbolPc},                           //
        {0x99DE, "Exit2", MonitorSymbolPc},                           //
        {0x99DF, "L99DF", MonitorSymbolPc},                           //
        {0x99E3, "Exit3", MonitorSymbolPc},                           //
        {0x99E3, "L99E3", MonitorSymbolPc},                           //
        {0x99E4, "L99E4", MonitorSymbolPc},                           //
        {0x99F2, "L99F2", MonitorSymbolPc},                           //
        {0x99FE, "L99FE", MonitorSymbolPc},                           //
        {0x9A04, "L9A04", MonitorSymbolPc},                           //
        {0x9A22, "L9A22", MonitorSymbolPc},                           //
        {0x9A38, "L9A38", MonitorSymbolPc},                           //
        {0x9A3A, "L9A3A", MonitorSymbolPc},                           //
        {0x9AA2, "L9AA2", MonitorSymbolPc},                           //
        {0x9AA5, "X9AA5", MonitorSymbolPc},                           //
        {0x9AB9, "L9AB9", MonitorSymbolPc},                           //
        {0x9AD3, "L9AD3", MonitorSymbolPc},                           //
        {0x9AD9, "L9AD9", MonitorSymbolPc},                           //
        {0x9AE2, "Wr1Byte", MonitorSymbolPc},                         //
        {0x9AFB, "L9AFB", MonitorSymbolPc},                           //
        {0x9B01, "L9B01", MonitorSymbolPc},                           //
        {0x9B07, "L9B07", MonitorSymbolPc},                           //
        {0x9B0B, "Flush", MonitorSymbolPc},                           //
        {0x9B50, "L9B50", MonitorSymbolPc},                           //
        {0x9B88, "GSrcLin", MonitorSymbolPc},                         //
        {0x9B94, "Exit4", MonitorSymbolPc},                           //
        {0x9B95, "L9B95", MonitorSymbolPc},                           //
        {0x9B9D, "L9B9D", MonitorSymbolPc},                           //
        {0x9BA2, "L9BA2", MonitorSymbolPc},                           //
        {0x9BBB, "L9BBB", MonitorSymbolPc},                           //
        {0x9BCC, "L9BCC", MonitorSymbolPc},                           //
        {0x9BD8, "L9BD8", MonitorSymbolPc},                           //
        {0x9BE3, "L9BE3", MonitorSymbolPc},                           //
        {0x9BFC, "L9BFC", MonitorSymbolPc},                           //
        {0x9C06, "L9C06", MonitorSymbolPc},                           //
        {0x9C0A, "L9C0A", MonitorSymbolPc},                           //
        {0x9C55, "ReadMore", MonitorSymbolPc},                        //
        {0x9C86, "L9C86", MonitorSymbolPc},                           //
        {0x9C8F, "L9C8F", MonitorSymbolPc},                           //
        {0x9CA4, "L9CA4", MonitorSymbolPc},                           //
        {0x9CAA, "L9CAA", MonitorSymbolPc},                           //
        {0x9CBA, "L9CBA", MonitorSymbolPc},                           //
        {0x9D21, "X9D21"},                                            //
        {0x9E13, "SetPNBuf", MonitorSymbolPc},                        //
        {0x9E18, "L9E18", MonitorSymbolPc},                           //
        {0x9E32, "L9E32", MonitorSymbolPc},                           //
        {0x9E36, "L9E36", MonitorSymbolPc},                           //
        {0x9E38, "L9E38", MonitorSymbolPc},                           //
        {0x9E3B, "L9E3B", MonitorSymbolPc},                           //
        {0x9E3E, "L9E3E", MonitorSymbolPc},                           //
        {0x9E55, "SBufSize"},                                         //
        {0x9E55, "X9E55", MonitorSymbolPc},                           //
        {0x9E56, "IBufSize"},                                         //
        {0x9E57, "PNTable"},                                          //
        {0x9E61, "FBufTbl"},                                          //
        {0x9E6B, "RefNbrT"},                                          //
        {0x9E75, "OneByte"},                                          //
        {0x9E78, "OpenPN"},                                           //
        {0x9E7A, "OpenFBuf"},                                         //
        {0x9E7C, "OpenRN"},                                           //
        {0x9E7E, "L9E7E", MonitorSymbolPc},                           //
        {0x9E7E, "ReadRN"},                                           //
        {0x9E7F, "ReadDBuf"},                                         //
        {0x9E81, "ReadLen"},                                          //
        {0x9E83, "LenRead"},                                          //
        {0x9E86, "WrObjRN"},                                          //
        {0x9E87, "WrObjDB"},                                          //
        {0x9E89, "WrObjLen"},                                         //
        {0x9E9A, "L9E9A", MonitorSymbolPc},                           //
        {0x9EA2, "GFInfoPB"},                                         //
        {0x9EA3, "GFIPN"},                                            //
        {0x9EA6, "GFIftype"},                                         //
        {0x9EB4, "SFInfoPB"},                                         //
        {0x9EB9, "SFIaux"},                                           //
        {0x9EC3, "CreatePN"},                                         //
        {0x9EC6, "CrFType"},                                          //
        {0x9ECB, "L9ECB", MonitorSymbolPc},                           //
        {0x9ECF, "DestroyPN"},                                        //
        {0x9ED2, "CloseRN"},                                          //
        {0x9EDC, "L9EDC"},                                            //
        {0x9F1D, "ObjPNB"},                                           //
        {0x9F5E, "ChnPNB"},                                           //
        {0x9F83, "X9F83"},                                            //
        {0xA042, "ftypeT"},                                           //
        {0xA04C, "XA04C"},                                            //
        {0xA056, "XA056"},                                            //
        {0xA060, "XA060"},                                            //
        {0xA06A, "XA06A"},                                            //
        {0xA074, "XA074"},                                            //
        {0xA08E, "SubTitle"},                                         //
        {0xA0B2, "ErrInfoT"},                                         //
        {0xA57F, "MonthsT"},                                          //
        {0xA75B, "XA75B", MonitorSymbolPc},                           //
        {0xA76B, "LA76B", MonitorSymbolPc},                           //
        {0xA785, "LA785", MonitorSymbolPc},                           //
        {0xA787, "LA787", MonitorSymbolPc},                           //
        {0xA788, "XA788", MonitorSymbolPc},                           //
        {0xA78A, "XA78A", MonitorSymbolPc},                           //
        {0xA7AB, "LA7AB", MonitorSymbolPc},                           //
        {0xA7E0, "LA7E0", MonitorSymbolPc},                           //
        {0xA872, "XA872", MonitorSymbolPc},                           //
        {0xA8A9, "LA8A9", MonitorSymbolPc},                           //
        {0xB100, "EIStart", MonitorSymbolPc},                         // must have
        {0xB102, "LB102", MonitorSymbolPc},                           //
        {0xB113, "LB113", MonitorSymbolPc},                           //
        {0xB164, "LB164", MonitorSymbolPc},                           //
        {0xB16F, "LB16F", MonitorSymbolPc},                           //
        {0xB195, "LB195", MonitorSymbolPc},                           //
        {0xB1AB, "EIMainLoop", MonitorSymbolPc},                      //
        {0xB1BC, "LB1BC", MonitorSymbolPc},                           //
        {0xB1CB, "LB1CB", MonitorSymbolPc},                           //
        {0xB1DA, "LB1DA", MonitorSymbolPc},                           //
        {0xB1E3, "LB1E3", MonitorSymbolPc},                           //
        {0xB1EE, "LB1EE", MonitorSymbolPc},                           //
        {0xB202, "LB202", MonitorSymbolPc},                           //
        {0xB20F, "LB20F", MonitorSymbolPc},                           //
        {0xB211, "LB211", MonitorSymbolPc},                           //
        {0xB220, "LB220", MonitorSymbolPc},                           //
        {0xB226, "LB226", MonitorSymbolPc},                           //
        {0xB23D, "LB23D", MonitorSymbolPc},                           //
        {0xB245, "LB245", MonitorSymbolPc},                           //
        {0xB250, "LB250", MonitorSymbolPc},                           //
        {0xB257, "LB257", MonitorSymbolPc},                           //
        {0xB263, "GetChar", MonitorSymbolPc},                         //
        {0xB273, "LB273", MonitorSymbolPc},                           //
        {0xB274, "GetNBChar", MonitorSymbolPc},                       //
        {0xB274, "XB274", MonitorSymbolPc},                           //
        {0xB27E, "GetToken", MonitorSymbolPc},                        //
        {0xB286, "LB286", MonitorSymbolPc},                           //
        {0xB289, "LB289", MonitorSymbolPc},                           //
        {0xB296, "LB296"},                                            //
        {0xB29A, "LB29A"},                                            //
        {0xB29D, "LB29D"},                                            //
        {0xB2BD, "LB2BD", MonitorSymbolPc},                           //
        {0xB2D4, "LB2D4", MonitorSymbolPc},                           //
        {0xB2E5, "LB2E5", MonitorSymbolPc},                           //
        {0xB2F7, "LB2F7", MonitorSymbolPc},                           //
        {0xB2FA, "LB2FA", MonitorSymbolPc},                           //
        {0xB305, "LB305", MonitorSymbolPc},                           //
        {0xB318, "LB318", MonitorSymbolPc},                           //
        {0xB31A, "LB31A", MonitorSymbolPc},                           //
        {0xB328, "LB328", MonitorSymbolPc},                           //
        {0xB32B, "LB32B", MonitorSymbolPc},                           //
        {0xB339, "PrtCR", MonitorSymbolPc},                           //
        {0xB341, "ClearScr", MonitorSymbolPc},                        //
        {0xB343, "PrChar", MonitorSymbolPc},                          //
        {0xB34F, "LB34F", MonitorSymbolPc},                           //
        {0xB355, "LB355", MonitorSymbolPc},                           //
        {0xB35F, "LB35F", MonitorSymbolPc},                           //
        {0xB3A4, "LB3A4", MonitorSymbolPc},                           //
        {0xB3AC, "LB3AC", MonitorSymbolPc},                           //
        {0xB3D9, "LB3D9", MonitorSymbolPc},                           //
        {0xB3E8, "LB3E8", MonitorSymbolPc},                           //
        {0xB3E8, "LFDF6", MonitorSymbolPc},                           //
        {0xB3F6, "LB3F6", MonitorSymbolPc},                           //
        {0xB404, "LB404", MonitorSymbolPc},                           //
        {0xB662, "LB662", MonitorSymbolPc},                           //
        {0xB6A4, "LB6A4", MonitorSymbolPc},                           //
        {0xB6E6, "LB6E6", MonitorSymbolPc},                           //
        {0xB6EA, "LB6EA", MonitorSymbolPc},                           //
        {0xB6EF, "LB6EF", MonitorSymbolPc},                           //
        {0xB6FB, "LB6FB", MonitorSymbolPc},                           //
        {0xB700, "LB700", MonitorSymbolPc},                           //
        {0xB704, "LB704", MonitorSymbolPc},                           //
        {0xB715, "LB715", MonitorSymbolPc},                           //
        {0xB721, "LB721", MonitorSymbolPc},                           //
        {0xB729, "LB729", MonitorSymbolPc},                           //
        {0xB72E, "DoAsmbly", MonitorSymbolPc},                        //
        {0xB741, "LB741", MonitorSymbolPc},                           //
        {0xB774, "LoadDEd", MonitorSymbolPc},                         //
        {0xB7AB, "Mv2LCB2", MonitorSymbolPc},                         //
        {0xB7BF, "LB7BF", MonitorSymbolPc},                           //
        {0xB7D9, "LoadAsm", MonitorSymbolPc},                         //
        {0xB7E6, "LB7E6", MonitorSymbolPc},                           //
        {0xB822, "LB822", MonitorSymbolPc},                           //
        {0xB836, "LB836", MonitorSymbolPc},                           //
        {0xB83D, "LB83D", MonitorSymbolPc},                           //
        {0xB844, "LoadMod", MonitorSymbolPc},                         // must have
        {0xB844, "XB844", MonitorSymbolPc},                           //
        {0xB85D, "LB85D", MonitorSymbolPc},                           //
        {0xB866, "LB866", MonitorSymbolPc},                           //
        {0xB86F, "LB86F", MonitorSymbolPc},                           //
        {0xB883, "CkAttrF", MonitorSymbolPc},                         //
        {0xB8A7, "LB8A7", MonitorSymbolPc},                           //
        {0xB8B0, "LB8B0", MonitorSymbolPc},                           //
        {0xB8B1, "LB8B1", MonitorSymbolPc},                           //
        {0xB8B2, "SetupFN", MonitorSymbolPc},                         //
        {0xB8B4, "LB8B4", MonitorSymbolPc},                           //
        {0xB8C7, "LB8C7", MonitorSymbolPc},                           //
        {0xB8CE, "OpenDFile", MonitorSymbolPc},                       //
        {0xB8DF, "LB8DF", MonitorSymbolPc},                           //
        {0xB8EF, "ClsFile", MonitorSymbolPc},                         //
        {0xB8FA, "LB8FA", MonitorSymbolPc},                           //
        {0xB937, "OpenNRd", MonitorSymbolPc},                         //
        {0xB961, "LB961", MonitorSymbolPc},                           //
        {0xB964, "LB964", MonitorSymbolPc},                           //
        {0xB965, "LB965", MonitorSymbolPc},                           //
        {0xB966, "CkSizeF", MonitorSymbolPc},                         //
        {0xB97B, "LB97B", MonitorSymbolPc},                           //
        {0xB990, "LB990", MonitorSymbolPc},                           //
        {0xB997, "LB997", MonitorSymbolPc},                           //
        {0xB99A, "LB99A", MonitorSymbolPc},                           //
        {0xB9CA, "LB9CA", MonitorSymbolPc},                           //
        {0xB9D6, "LB9D6", MonitorSymbolPc},                           //
        {0xBA80, "FreeMem"},                                          //
        {0xBA83, "PathNameP"},                                        //
        {0xBA85, "IOBufP"},                                           //
        {0xBA87, "OpenRN"},                                           //
        {0xBA89, "ReadRN"},                                           //
        {0xBA8A, "ReadDBuf"},                                         //
        {0xBA8A, "XBA8A"},                                            //
        {0xBA8C, "ReadLen"},                                          //
        {0xBA8C, "XBA8C"},                                            //
        {0xBA91, "WriteRN"},                                          //
        {0xBA99, "RdExeRN"},                                          //
        {0xBA9E, "LBA9E"},                                            //
        {0xBAA1, "NewLineRN"},                                        //
        {0xBAC3, "GeofRN"},                                           //
        {0xBAC4, "fileEOF"},                                          //
        {0xBAC8, "GFIPath"},                                          //
        {0xBACB, "GFIftype"},                                         //
        {0xBAF7, "CloseRN"},                                          //
        {0xBB06, "ExitFlag"},                                         //
        {0xBB40, "FileNameB"},                                        //
        {0xBB80, "CurrPfxB"},                                         //
        {0xBC00, "HeaderT"},                                          //
        {0xBC00, "SavCmdB"},                                          //
        {0xBC80, "ExecLineB"},                                        //
        {0xBD00, "ObjDataB"},                                         //
        {0xBD80, "AsmParmB"},                                         //
        {0xBE00, "XBE00"},                                            //
        {0xBE40, "DevCtlS"},                                          //
        {0xBE60, "TabTable"},                                         //
        {0xBE64, "DateTime"},                                         //
        {0xBEFC, "PrtError"},                                         //
        {0xBF00, "PRODOS8", MonitorSymbolPc},                         //
        {0xBF30, "LASTDEV"},                                          //
        {0xBF58, "BitMap"},                                           //
        {0xBF90, "P8DATE"},                                           //
        {0xBF92, "P8TIME"},                                           //
        {0xBF98, "MACHID"},                                           //
        {0xC000, "IOADR"},                                            //
        {0xC000, "KBD"},                                              //
        {0xC00C, "CLR80VID"},                                         //
        {0xC030, "SPKR"},                                             //
        {0xC051, "TXTSET"},                                           //
        {0xC054, "LOWSCR"},                                           //
        {0xC056, "LORES"},                                            //
        {0xC080, "RDBANK2"},                                          //
        {0xC081, "ROMIN2"},                                           //
        {0xC082, "RDROM2"},                                           //
        {0xC083, "LCBANK2"},                                          //
        {0xC30B, "XC30B"},                                            //
        {0xD000, "DoPass3", MonitorSymbolPc},                         //
        {0xD007, "ChkPrtCols", MonitorSymbolPc},                      //
        {0xD00F, "UsePrtr", MonitorSymbolPc},                         //
        {0xD015, "SetPtrCols", MonitorSymbolPc},                      //
        {0xD01A, "ChkLoop", MonitorSymbolPc},                         //
        {0xD025, "LD025", MonitorSymbolPc},                           //
        {0xD056, "LD056", MonitorSymbolPc},                           //
        {0xD05C, "LD05C", MonitorSymbolPc},                           //
        {0xD06A, "LD06A", MonitorSymbolPc},                           //
        {0xD076, "LD076", MonitorSymbolPc},                           //
        {0xD078, "LD078", MonitorSymbolPc},                           //
        {0xD081, "LD081", MonitorSymbolPc},                           //
        {0xD083, "LD083", MonitorSymbolPc},                           //
        {0xD096, "LD096", MonitorSymbolPc},                           //
        {0xD099, "LD099", MonitorSymbolPc},                           //
        {0xD0A2, "LD0A2", MonitorSymbolPc},                           //
        {0xD0B1, "LD0B1", MonitorSymbolPc},                           //
        {0xD0BF, "LD0BF", MonitorSymbolPc},                           //
        {0xD0CE, "LD0CE", MonitorSymbolPc},                           //
        {0xD0E3, "LD0E3", MonitorSymbolPc},                           //
        {0xD0EB, "LD0EB", MonitorSymbolPc},                           //
        {0xD0ED, "LD0ED", MonitorSymbolPc},                           //
        {0xD0F5, "LD0F5", MonitorSymbolPc},                           //
        {0xD108, "LD108", MonitorSymbolPc},                           //
        {0xD12D, "LD12D", MonitorSymbolPc},                           //
        {0xD138, "LD138", MonitorSymbolPc},                           //
        {0xD14C, "LD14C", MonitorSymbolPc},                           //
        {0xD163, "LD163", MonitorSymbolPc},                           //
        {0xD16E, "LD16E", MonitorSymbolPc},                           //
        {0xD172, "doRtn", MonitorSymbolPc},                           //
        {0xD198, "LD198", MonitorSymbolPc},                           //
        {0xD1A5, "LD1A5", MonitorSymbolPc},                           //
        {0xD1B7, "LD1B7", MonitorSymbolPc},                           //
        {0xD1BF, "LD1BF", MonitorSymbolPc},                           //
        {0xD1C5, "LD1C5", MonitorSymbolPc},                           //
        {0xD1CD, "doRtn1", MonitorSymbolPc},                          //
        {0xD1CE, "DoSort", MonitorSymbolPc},                          //
        {0xD1D6, "WhileLoop", MonitorSymbolPc},                       //
        {0xD1E1, "LD1E1", MonitorSymbolPc},                           //
        {0xD2D5, "PrSymTbl", MonitorSymbolPc},                        //
        {0xD2D8, "LD2D8", MonitorSymbolPc},                           //
        {0xD2E4, "LD2E4", MonitorSymbolPc},                           //
        {0xD2F0, "LD2F0", MonitorSymbolPc},                           //
        {0xD2F8, "LD2F8", MonitorSymbolPc},                           //
        {0xD307, "LD307", MonitorSymbolPc},                           //
        {0xD310, "LD310", MonitorSymbolPc},                           //
        {0xD319, "LD319", MonitorSymbolPc},                           //
        {0xD329, "LD329", MonitorSymbolPc},                           //
        {0xD32B, "LD32B", MonitorSymbolPc},                           //
        {0xD348, "LD348", MonitorSymbolPc},                           //
        {0xD34B, "LD34B", MonitorSymbolPc},                           //
        {0xD356, "LD356", MonitorSymbolPc},                           //
        {0xD362, "LD362", MonitorSymbolPc},                           //
        {0xD363, "LD363", MonitorSymbolPc},                           //
        {0xD370, "LD370", MonitorSymbolPc},                           //
        {0xD386, "doRtn2", MonitorSymbolPc},                          //
        {0xD387, "AdvRecP", MonitorSymbolPc},                         //
        {0xD38F, "LD38F", MonitorSymbolPc},                           //
        {0xD397, "LD397", MonitorSymbolPc},                           //
        {0xD3D4, "ErrMsgT"},                                          //
        {0xD41E, "FileTxt"},                                          //
        {0xD425, "ASEndTxt"},                                         //
        {0xD501, "InLinTxt"},                                         //
        {0xD835, "OpcodeT"},                                          //
        {0xD90A, "CycTimes"},                                         //
        {0xD9DF, "CharMap1"},                                         //
        {0xDA5F, "CharMap2"},                                         //
        {0xDD41, "XDD41", MonitorSymbolPc},                           //
        {0xDD4A, "LDD4A", MonitorSymbolPc},                           //
        {0xDD53, "LDD53", MonitorSymbolPc},                           //
        {0xDDBE, "DefTabsT"},                                         //
        {0xDE18, "IRQV"},                                             //
        {0xDE19, "IRQV"},                                             //
        {0xDE1A, "IRQV"},                                             //
        {0xDE1B, "IRQV"},                                             //
        {0xDE1C, "IRQV"},                                             //
        {0xDE1D, "IRQV"},                                             //
        {0xDE1E, "IRQV"},                                             //
        {0xDE1F, "IRQV"},                                             //
        {0xDE20, "IRQV"},                                             //
        {0xDE21, "IRQV"},                                             //
        {0xDE22, "IRQV"},                                             //
        {0xDE23, "IRQV"},                                             //
        {0xDE24, "IRQV"},                                             //
        {0xDE25, "IRQV"},                                             //
        {0xDE26, "IRQV"},                                             //
        {0xDE27, "IRQV"},                                             //
        {0xDE28, "IRQV"},                                             //
        {0xDE29, "IRQV"},                                             //
        {0xDE2A, "IRQV"},                                             //
        {0xDE2B, "IRQV"},                                             //
        {0xDE2C, "IRQV"},                                             //
        {0xDE2D, "IRQV"},                                             //
        {0xDE2E, "IRQV"},                                             //
        {0xDE2F, "IRQV"},                                             //
        {0xDE30, "IRQV"},                                             //
        {0xDE31, "IRQV"},                                             //
        {0xDE32, "IRQV"},                                             //
        {0xDE33, "IRQV"},                                             //
        {0xDE34, "IRQV"},                                             //
        {0xDE35, "IRQV"},                                             //
        {0xDE36, "IRQV"},                                             //
        {0xDE37, "IRQV"},                                             //
        {0xDE38, "IRQV"},                                             //
        {0xDE39, "IRQV"},                                             //
        {0xDE3A, "IRQV"},                                             //
        {0xDE3B, "IRQV"},                                             //
        {0xDE3C, "IRQV"},                                             //
        {0xDE3D, "IRQV"},                                             //
        {0xDE3E, "IRQV"},                                             //
        {0xDE3F, "IRQV"},                                             //
        {0xDE40, "IRQV"},                                             //
        {0xDE41, "IRQV"},                                             //
        {0xDE42, "IRQV"},                                             //
        {0xDE43, "IRQV"},                                             //
        {0xDE44, "IRQV"},                                             //
        {0xDE45, "IRQV"},                                             //
        {0xDE46, "IRQV"},                                             //
        {0xDE47, "IRQV"},                                             //
        {0xDE48, "IRQV"},                                             //
        {0xDE49, "IRQV"},                                             //
        {0xDE4A, "IRQV"},                                             //
        {0xDE4B, "IRQV"},                                             //
        {0xDE4C, "IRQV"},                                             //
        {0xDE4D, "IRQV"},                                             //
        {0xDE4E, "IRQV"},                                             //
        {0xDE4F, "IRQV"},                                             //
        {0xDE50, "IRQV"},                                             //
        {0xDE51, "IRQV"},                                             //
        {0xDE52, "IRQV"},                                             //
        {0xDE53, "IRQV"},                                             //
        {0xDE54, "IRQV"},                                             //
        {0xDE55, "IRQV"},                                             //
        {0xDE56, "IRQV"},                                             //
        {0xDE57, "IRQV"},                                             //
        {0xDE58, "IRQV"},                                             //
        {0xDE59, "IRQV"},                                             //
        {0xDE5A, "IRQV"},                                             //
        {0xDE5B, "IRQV"},                                             //
        {0xDE5C, "IRQV"},                                             //
        {0xDE5D, "IRQV"},                                             //
        {0xDE5E, "IRQV"},                                             //
        {0xDE5F, "IRQV"},                                             //
        {0xDE60, "IRQV"},                                             //
        {0xDE61, "IRQV"},                                             //
        {0xDE62, "IRQV"},                                             //
        {0xDE63, "IRQV"},                                             //
        {0xDE64, "IRQV"},                                             //
        {0xDE65, "IRQV"},                                             //
        {0xDE66, "IRQV"},                                             //
        {0xDE67, "IRQV"},                                             //
        {0xDE68, "IRQV"},                                             //
        {0xDE69, "IRQV"},                                             //
        {0xDE6A, "IRQV"},                                             //
        {0xDE6B, "IRQV"},                                             //
        {0xDE6C, "IRQV"},                                             //
        {0xDE6D, "IRQV"},                                             //
        {0xDE6E, "IRQV"},                                             //
        {0xDE6F, "IRQV"},                                             //
        {0xDE70, "IRQV"},                                             //
        {0xDE71, "IRQV"},                                             //
        {0xDE72, "IRQV"},                                             //
        {0xDE73, "IRQV"},                                             //
        {0xDE74, "IRQV"},                                             //
        {0xDE75, "IRQV"},                                             //
        {0xDE76, "IRQV"},                                             //
        {0xDE77, "IRQV"},                                             //
        {0xDE78, "IRQV"},                                             //
        {0xDE79, "IRQV"},                                             //
        {0xDE7A, "IRQV"},                                             //
        {0xDE7B, "IRQV"},                                             //
        {0xDE7C, "IRQV"},                                             //
        {0xDE7D, "IRQV"},                                             //
        {0xDE7E, "IRQV"},                                             //
        {0xDE7F, "IRQV"},                                             //
        {0xDE80, "IRQV"},                                             //
        {0xDE81, "IRQV"},                                             //
        {0xDE82, "IRQV"},                                             //
        {0xDE83, "IRQV"},                                             //
        {0xDE84, "IRQV"},                                             //
        {0xDE85, "IRQV"},                                             //
        {0xDE86, "IRQV"},                                             //
        {0xDE87, "IRQV"},                                             //
        {0xDE88, "IRQV"},                                             //
        {0xDE89, "IRQV"},                                             //
        {0xDE8A, "IRQV"},                                             //
        {0xDE8B, "IRQV"},                                             //
        {0xDE8C, "IRQV"},                                             //
        {0xDE8D, "IRQV"},                                             //
        {0xDE8E, "IRQV"},                                             //
        {0xDE8F, "IRQV"},                                             //
        {0xDE90, "IRQV"},                                             //
        {0xDE91, "IRQV"},                                             //
        {0xDF09, "Tbl1stLet"},                                        //
        {0xDF4E, "SvZPArea"},                                         //
        {0xFB2F, "INIT", MonitorSymbolPc},                            //
        {0xFB4B, "LFB4B", MonitorSymbolPc},                           //
        {0xFB78, "LFB78", MonitorSymbolPc},                           //
        {0xFB94, "LFB94", MonitorSymbolPc},                           //
        {0xFBC1, "LFBC1", MonitorSymbolPc},                           //
        {0xFBD0, "LFBD0", MonitorSymbolPc},                           //
        {0xFBD9, "LFBD9", MonitorSymbolPc},                           //
        {0xFBE4, "LFBE4", MonitorSymbolPc},                           //
        {0xFBEF, "LFBEF", MonitorSymbolPc},                           //
        {0xFBF0, "LFBF0", MonitorSymbolPc},                           //
        {0xFBF4, "LFBF4", MonitorSymbolPc},                           //
        {0xFBFC, "LFBFC", MonitorSymbolPc},                           //
        {0xFBFD, "LFBFD", MonitorSymbolPc},                           //
        {0xFC22, "LFC22", MonitorSymbolPc},                           //
        {0xFC24, "LFC24", MonitorSymbolPc},                           //
        {0xFC2B, "LFC2B", MonitorSymbolPc},                           //
        {0xFC46, "LFC46", MonitorSymbolPc},                           //
        {0xFC58, "HOME", MonitorSymbolPc},                            //
        {0xFC62, "LFC62", MonitorSymbolPc},                           //
        {0xFC66, "LFC66", MonitorSymbolPc},                           //
        {0xFC76, "LFC76", MonitorSymbolPc},                           //
        {0xFC8C, "LFC8C", MonitorSymbolPc},                           //
        {0xFC95, "LFC95", MonitorSymbolPc},                           //
        {0xFC9E, "LFC9E", MonitorSymbolPc},                           //
        {0xFCA0, "LFCA0", MonitorSymbolPc},                           //
        {0xFCA8, "LFCA8", MonitorSymbolPc},                           //
        {0xFCA8, "WAIT", MonitorSymbolPc},                            //
        {0xFCA9, "LFCA9", MonitorSymbolPc},                           //
        {0xFCAA, "LFCAA", MonitorSymbolPc},                           //
        {0xFCB4, "LFCB4", MonitorSymbolPc},                           //
        {0xFCBA, "LFCBA", MonitorSymbolPc},                           //
        {0xFCC8, "LFCC8", MonitorSymbolPc},                           //
        {0xFDED, "COUT", MonitorSymbolPc},                            //
        {0xFDF0, "COUT1", MonitorSymbolPc},                           //
        {0xFDF6, "LFDF6", MonitorSymbolPc},                           //
        {0xFE2C, "MOVE", MonitorSymbolPc},                            //
        {0xFE84, "SETNORM", MonitorSymbolPc},                         //
        {0xFE86, "LFE86", MonitorSymbolPc},                           //
        {0xFE89, "SETKBD", MonitorSymbolPc},                          //
        {0xFE93, "SETVID", MonitorSymbolPc},                          //
        {0xFE9B, "LFE9B", MonitorSymbolPc},                           //
        {0xFEA7, "LFEA7", MonitorSymbolPc},                           //
        {0xFEA9, "LFEA9", MonitorSymbolPc},                           //
    };

    const MonitorSymbol* find_monitor_symbol(uint16_t addr, uint8_t requiredFlag) {
      for (const MonitorSymbol& symbol : kMonitorSymbols) {
        if (symbol.address == addr && (symbol.flags & requiredFlag) != 0) {
          return &symbol;
        }
      }
      return nullptr;
    }

    const MonitorSymbol* find_any_monitor_symbol(uint16_t addr) {
      for (const MonitorSymbol& symbol : kMonitorSymbols) {
        if (symbol.address == addr) {
          return &symbol;
        }
      }
      return nullptr;
    }

    const char* zp_monitor_mutator_name(uint8_t opcode) {
      const uint8_t mode  = static_cast<uint8_t>(opcode & 0x1F);
      const uint8_t group = static_cast<uint8_t>(opcode & 0xE0);

      // Decode-group classification keeps mutator labeling independent from opcode whitelists.
      if (group == 0x80) {
        const AluModeMetadata* modeMetadata = find_alu_mode_metadata(mode);
        if (modeMetadata != nullptr && modeMetadata->operandMode != AluOperandMode::Immediate) {
          return "STA";
        }
      }

      switch (opcode) {
        case 0x64:
        case 0x74:
        case 0x9C:
        case 0x9E:
          return "STZ";

        case 0x04:
        case 0x0C:
          return "TSB";

        case 0x14:
        case 0x1C:
          return "TRB";
      }

      if ((opcode & 0x0F) == 0x07) {
        return ((opcode & 0x80) == 0) ? "RMB" : "SMB";
      }

      const RmwModeMetadata*  rmwModeMetadata  = find_rmw_mode_metadata(mode);
      const RmwGroupMetadata* rmwGroupMetadata = find_rmw_group_metadata(group);
      if (rmwModeMetadata != nullptr && rmwGroupMetadata != nullptr) {
        switch (rmwGroupMetadata->operation) {
          case RmwGroupOperation::Asl:
            return "ASL";
          case RmwGroupOperation::Rol:
            return "ROL";
          case RmwGroupOperation::Lsr:
            return "LSR";
          case RmwGroupOperation::Ror:
            return "ROR";
          case RmwGroupOperation::Dec:
            return "DEC";
          case RmwGroupOperation::Inc:
            return "INC";
        }
      }

      return nullptr;
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
        case 0x65:
          return "QUIT";
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

    void dump_stack(std::ostream& os, const ConstMemoryBanks& banks, uint8_t sp) {
      // 6502 stack is at $0100-$01FF, SP points to next available location
      // Stack grows downward, so used portion is from $0100+SP+1 to $01FF
      uint16_t stackTop = 0x01FF;
      uint16_t stackPtr = 0x0100 + sp;

      os << "\nStack dump (SP=$";
      write_hex(os, sp, 2);
      os << ", used bytes: " << (stackTop - stackPtr) << "):\n";

      if (stackPtr >= stackTop) {
        os << "  (stack empty)\n";
        return;
      }

      // Dump in rows of 16 bytes
      for (uint16_t addr = stackPtr + 1; addr <= stackTop; addr++) {
        if ((addr - 0x0100) % 16 == 0 || addr == stackPtr + 1) {
          if (addr != stackPtr + 1) {
            os << "\n";
          }
          os << "  $";
          write_hex(os, addr, 4);
          os << ":";
        }
        os << " ";
        uint8_t byte = read_u8(banks, addr);
        write_hex(os, byte, 2);
      }
      os << "\n";
    }

    void dump_pc_ring(std::ostream& os, const uint16_t* pcRingFrom, const uint16_t* pcRingTo,
                      const uint32_t* pcRingCount, size_t ringSize, size_t ringIndex) {
      os << "\nPC ring buffer (last " << ringSize << " explicit PC changes, newest first):\n";

      // ringIndex points to the next slot to write, so ringIndex-1 is the most recent
      // We walk backwards through the ring buffer
      size_t count = 0;
      for (size_t i = 0; i < ringSize; i++) {
        // Calculate the index walking backwards from most recent
        size_t   idx     = (ringIndex + ringSize - 1 - i) % ringSize;
        uint16_t fromPC  = pcRingFrom[idx];
        uint16_t toPC    = pcRingTo[idx];
        uint32_t pcCount = pcRingCount[idx];

        // Stop if we hit an uninitialized entry (from=0, to=0 before buffer fills up)
        if (fromPC == 0 && toPC == 0 && i >= ringIndex) {
          break;
        }

        if (count % 4 == 0) {
          if (count > 0) {
            os << "\n";
          }
          os << "  ";
        } else {
          os << " ";
        }
        os << "$";
        write_hex(os, fromPC, 4);
        os << "->$";
        write_hex(os, toPC, 4);
        if (pcCount > 1) {
          os << "x" << pcCount;
        }
        count++;
      }
      if (count > 0) {
        os << "\n";
      } else {
        os << "  (empty)\n";
      }
    }

    const char* error_name(uint8_t errorCode) {
      switch (errorCode) {
        case ERR_NO_ERROR:
          return "";
        case ERR_BAD_CALL_NUMBER:
          return "BAD_CALL_NUMBER";
        case ERR_BAD_CALL_PARAM_COUNT:
          return "BAD_CALL_PARAM_COUNT";
        case ERR_INTERRUPT_TABLE_FULL:
          return "INTERRUPT_TABLE_FULL";
        case ERR_IO_ERROR:
          return "IO_ERROR";
        case ERR_NO_DEVICE:
          return "NO_DEVICE";
        case ERR_WRITE_PROTECTED:
          return "WRITE_PROTECTED";
        case ERR_DISK_SWITCHED:
          return "DISK_SWITCHED";
        case ERR_INVALID_PATH_SYNTAX:
          return "INVALID_PATH_SYNTAX";
        case ERR_TOO_MANY_FILES_OPEN:
          return "TOO_MANY_FILES_OPEN";
        case ERR_BAD_REF_NUM:
          return "BAD_REF_NUM";
        case ERR_PATH_NOT_FOUND:
          return "PATH_NOT_FOUND";
        case ERR_VOL_NOT_FOUND:
          return "VOL_NOT_FOUND";
        case ERR_FILE_NOT_FOUND:
          return "FILE_NOT_FOUND";
        case ERR_DUPLICATE_FILENAME:
          return "DUPLICATE_FILENAME";
        case ERR_VOLUME_FULL:
          return "VOLUME_FULL";
        case ERR_VOL_DIR_FULL:
          return "VOL_DIR_FULL";
        case ERR_INCOMPATIBLE_VERSION:
          return "INCOMPATIBLE_VERSION";
        case ERR_UNSUPPORTED_STOR_TYPE:
          return "UNSUPPORTED_STOR_TYPE";
        case ERR_EOF_ENCOUNTERED:
          return "EOF_ENCOUNTERED";
        case ERR_POSITION_OUT_OF_RANGE:
          return "POSITION_OUT_OF_RANGE";
        case ERR_ACCESS_ERROR:
          return "ACCESS_ERROR";
        case ERR_FILE_OPEN:
          return "FILE_OPEN";
        case ERR_DIR_COUNT_ERROR:
          return "DIR_COUNT_ERROR";
        case ERR_NOT_PRODOS_VOL:
          return "NOT_PRODOS_VOL";
        case ERR_INVALID_PARAMETER:
          return "INVALID_PARAMETER";
        case ERR_VCB_TABLE_FULL:
          return "VCB_TABLE_FULL";
        case ERR_BAD_BUFFER_ADDR:
          return "BAD_BUFFER_ADDR";
        case ERR_DUPLICATE_VOLUME:
          return "DUPLICATE_VOLUME";
        case ERR_FILE_STRUCTURE_DAMAGED:
          return "FILE_STRUCTURE_DAMAGED";
        default:
          return "";
      }
    }

    /**
     * Read a counted string from memory (ProDOS pathname format).
     * First byte is length, followed by that many characters.
     * Returns empty string if length > maxLen or length == 0.
     * If outLength is provided, sets it to the actual length byte read.
     */
    std::string read_pathname(const ConstMemoryBanks& banks, uint16_t pathnamePtr,
                              uint8_t maxLen = 64, uint8_t* outLength = nullptr) {
      uint8_t length = read_u8(banks, pathnamePtr);
      if (outLength != nullptr) {
        *outLength = length;
      }

      if (length == 0 || length > maxLen) {
        return "";
      }

      std::string result;
      result.reserve(length);
      for (uint8_t i = 0; i < length; i++) {
        uint8_t ch = read_u8(banks, static_cast<uint16_t>(pathnamePtr + 1 + i));
        // ProDOS pathnames: high bit clear, uppercase ASCII
        result.push_back(static_cast<char>(ch & 0x7F));
      }
      return result;
    }

    std::string format_counted_path_for_log(const char* fieldName, const std::string& pathname,
                                            uint8_t length, uint8_t maxLen = 64) {
      if (!pathname.empty()) {
        return " " + std::string(fieldName) + "='" + pathname + "'";
      }
      if (length == 0) {
        return " " + std::string(fieldName) + "=<empty>";
      }
      if (length > maxLen) {
        return " " + std::string(fieldName) + "=<invalid:len=" + std::to_string(length) + ">";
      }
      return "";
    }

    std::string read_and_format_counted_path_for_log(const ConstMemoryBanks& banks,
                                                     uint16_t pathnamePtr, const char* fieldName,
                                                     uint8_t maxLen = 64) {
      uint8_t     length   = 0;
      std::string pathname = read_pathname(banks, pathnamePtr, maxLen, &length);
      return format_counted_path_for_log(fieldName, pathname, length, maxLen);
    }

    std::string extract_mli_on_line_log(const ConstMemoryBanks& banks, uint16_t paramBlockAddr,
                                        uint8_t err) {
      if (err != 0) {
        return "";
      }

      std::string result;
      uint8_t     unitNum       = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
      uint16_t    dataBufferPtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));

      if (unitNum == 0) {
        result           = " volumes='";
        bool     first   = true;
        uint16_t offset  = dataBufferPtr;
        int      maxRecs = 15;  // Safety limit (14 volumes + terminator)

        for (int i = 0; i < maxRecs; i++) {
          uint8_t byte0 = read_u8(banks, offset);
          if (byte0 == 0) break;  // Terminator

          uint8_t length = byte0 & 0x0F;
          uint8_t slot   = (byte0 >> 4) & 0x07;
          uint8_t drive  = (byte0 >> 7) & 0x01;

          if (length > 0 && length <= 15) {
            if (!first) result += ", ";
            first = false;

            std::string volName;
            volName.reserve(length);
            for (uint8_t j = 0; j < length; j++) {
              uint8_t ch = read_u8(banks, static_cast<uint16_t>(offset + 1 + j));
              volName.push_back(static_cast<char>(ch & 0x7F));
            }

            result += volName + "[S" + std::to_string(slot) + "D" + std::to_string(drive) + "]";
          }

          offset += 16;
        }
        result += "'";
        return result;
      }

      uint8_t byte0  = read_u8(banks, dataBufferPtr);
      uint8_t length = byte0 & 0x0F;
      uint8_t slot   = (byte0 >> 4) & 0x07;
      uint8_t drive  = (byte0 >> 7) & 0x01;

      if (length > 0 && length <= 15) {
        std::string volName;
        volName.reserve(length);
        for (uint8_t i = 0; i < length; i++) {
          uint8_t ch = read_u8(banks, static_cast<uint16_t>(dataBufferPtr + 1 + i));
          volName.push_back(static_cast<char>(ch & 0x7F));
        }

        return " volume='" + volName + "[S" + std::to_string(slot) + "D" + std::to_string(drive) +
               "]'";
      }
      if (length == 0) {
        return " volume=<none>";
      }
      return "";
    }

    void append_mli_read_directory_log(const ConstMemoryBanks& banks, MLIContext* mli,
                                       uint16_t paramBlockAddr, uint8_t refNum, uint8_t err,
                                       std::string& result) {
      bool isDirectoryRead = (mli != nullptr && err == 0 && mli->isDirectoryRefNum(refNum));
      if (!isDirectoryRead) {
        return;
      }

      uint16_t dataBufferPtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 2));
      uint16_t dirTransCount = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6));

      uint32_t markAfter  = mli->getMarkForRefNum(refNum);
      uint32_t markBefore = markAfter - dirTransCount;

      uint32_t blockNum    = markBefore / 512;
      uint32_t blockOffset = markBefore % 512;

      result += " mark=$" + std::to_string(markBefore);
      result += " blk=" + std::to_string(blockNum) + "+" + std::to_string(blockOffset);

      if (dirTransCount == 0) {
        return;
      }

      std::string posInfo;
      if (blockOffset <= 3) {
        posInfo = " [ptrs]";
      } else if (blockNum == 0 && blockOffset >= 4 && blockOffset <= 42) {
        posInfo = " [hdr]";
      } else if (blockOffset >= 4 && blockOffset <= 510) {
        uint32_t entryOffset = blockOffset - 4;
        uint32_t entryNum    = entryOffset / 39;
        posInfo              = " [ent" + std::to_string(entryNum) + "]";
      } else if (blockOffset == 511) {
        posInfo = " [pad]";
      }
      result += posInfo;

      std::string entries;
      bool        first        = true;
      int         validEntries = 0;

      uint16_t transCount    = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6));
      uint32_t blockStartAbs = static_cast<uint32_t>(blockOffset);
      uint32_t bufferEndAbs  = blockStartAbs + static_cast<uint32_t>(transCount);
      uint32_t firstEntryAbs = 0;

      if (bufferEndAbs > 4) {
        if (blockStartAbs <= 4) {
          firstEntryAbs = 4;
        } else {
          uint32_t relToFirst = blockStartAbs - 4;
          uint32_t rem        = relToFirst % 39;
          uint32_t delta      = (rem == 0) ? 0u : (39u - rem);
          firstEntryAbs       = blockStartAbs + delta;
        }
      }

      if (firstEntryAbs >= blockStartAbs && firstEntryAbs + 0x27 <= bufferEndAbs) {
        for (uint16_t offset = static_cast<uint16_t>(firstEntryAbs - blockStartAbs);
             offset + 0x27 <= transCount; offset = static_cast<uint16_t>(offset + 39)) {
          uint8_t byte0   = read_u8(banks, static_cast<uint16_t>(dataBufferPtr + offset));
          uint8_t nameLen = byte0 & 0x0F;

          if (byte0 == 0) {
            continue;
          }

          std::string entryName;
          bool        allValid = true;
          entryName.reserve(nameLen);

          for (uint8_t i = 0; i < nameLen; i++) {
            if (offset + 1 + i >= transCount) {
              allValid = false;
              break;
            }
            uint8_t ch = read_u8(banks, static_cast<uint16_t>(dataBufferPtr + offset + 1 + i));
            ch         = ch & 0x7F;

            entryName.push_back(static_cast<char>(ch));
          }

          if (allValid) {
            validEntries++;
            if (first) {
              entries = " entries='";
              first   = false;
            } else {
              entries += ", ";
            }
            entries += entryName;
          }
        }
      }

      if (validEntries >= 1) {
        entries += "'";
        result += entries;
      }
    }

    std::string extract_mli_read_log(const ConstMemoryBanks& banks, MLIContext* mli,
                                     uint16_t paramBlockAddr, uint8_t err) {
      uint8_t     refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
      std::string result = " ref=" + std::to_string(refNum);

      uint16_t requestCount = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 4));
      uint16_t transCount   = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 6));
      result += " req=" + std::to_string(requestCount);
      result += " trans=" + std::to_string(transCount);

      if (mli != nullptr) {
        uint32_t markAfter  = mli->getMarkForRefNum(refNum);
        uint32_t markBefore = (markAfter >= transCount) ? (markAfter - transCount) : 0;
        uint32_t eof        = mli->getEofForRefNum(refNum);
        result += " mark=$" + std::to_string(markBefore);
        result += " eof=$" + std::to_string(eof);
      }

      append_mli_read_directory_log(banks, mli, paramBlockAddr, refNum, err, result);
      return result;
    }

    /**
     * Extract pathname(s) from MLI parameter block for logging.
     * Returns formatted string with pathname info, or empty if call doesn't use pathnames.
     */
    std::string extract_mli_pathnames(const ConstMemoryBanks& banks, MLIContext* mli,
                                      uint8_t callNumber, uint16_t paramBlockAddr, uint8_t err) {
      std::string result;

      switch (callNumber) {
        // Single pathname at offset +1
        case 0xC0:  // CREATE
        case 0xC1:  // DESTROY
        case 0xC3:  // SET_FILE_INFO
        case 0xC4:  // GET_FILE_INFO
        case 0xC8:  // OPEN
        {
          uint16_t pathnamePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result               = read_and_format_counted_path_for_log(banks, pathnamePtr, "path");

          // OPEN: log output refnum on success
          if (callNumber == 0xC8 && err == 0) {
            uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 5));
            result += " ref=" + std::to_string(refNum);
          }
          break;
        }

        // RENAME: old pathname at +1, new pathname at +3
        case 0xC2:  // RENAME
        {
          uint16_t    oldPtr  = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          uint16_t    newPtr  = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 3));
          std::string oldPath = read_pathname(banks, oldPtr);
          std::string newPath = read_pathname(banks, newPtr);
          if (!oldPath.empty() && !newPath.empty()) {
            result = " old='" + oldPath + "' new='" + newPath + "'";
          }
          break;
        }

        // SET_PREFIX: pathname at +1
        case 0xC6:  // SET_PREFIX
        {
          uint16_t pathnamePtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result               = read_and_format_counted_path_for_log(banks, pathnamePtr, "prefix");
          break;
        }

        // GET_PREFIX: data_buffer at +1 (pathname returned there, read after call)
        case 0xC7:  // GET_PREFIX
        {
          // Only read buffer if call succeeded
          if (err == 0) {
            uint16_t dataBufferPtr = read_u16_le(banks, static_cast<uint16_t>(paramBlockAddr + 1));
            result = read_and_format_counted_path_for_log(banks, dataBufferPtr, "prefix");
          }
          break;
        }

        // ON_LINE: data_buffer at +2 (volume names returned there, read after call)
        case 0xC5:  // ON_LINE
        {
          result = extract_mli_on_line_log(banks, paramBlockAddr, err);
          break;
        }

        // READ: data_buffer at +2, might be directory entries
        case 0xCA:  // READ
        {
          result = extract_mli_read_log(banks, mli, paramBlockAddr, err);
          break;
        }

        // Calls with single input refnum at +1
        case 0xC9:  // NEWLINE
        case 0xCB:  // WRITE
        case 0xCC:  // CLOSE
        case 0xCD:  // FLUSH
        case 0xCE:  // SET_MARK
        case 0xCF:  // GET_MARK
        case 0xD0:  // SET_EOF
        case 0xD1:  // GET_EOF
        {
          uint8_t refNum = read_u8(banks, static_cast<uint16_t>(paramBlockAddr + 1));
          result         = " ref=" + std::to_string(refNum);
          break;
        }

        // Calls that don't involve pathnames or where we don't log them
        default:
          break;
      }

      return result;
    }

    std::string build_mli_trap_log_message(uint64_t instructionCount, const ConstMemoryBanks& banks,
                                           MLIContext* mli, uint16_t callPC, uint8_t callNumber,
                                           uint16_t paramBlockAddr, uint8_t err) {
      std::ostringstream mli_msg;
      mli_msg << "@" << instructionCount << " PC=$";
      write_hex(mli_msg, callPC, 4);
      mli_msg << " MLI call=$";
      write_hex(mli_msg, callNumber, 2);
      mli_msg << " (" << mli_call_name(callNumber) << ") param=$";
      write_hex(mli_msg, paramBlockAddr, 4);

      std::string pathInfo = extract_mli_pathnames(banks, mli, callNumber, paramBlockAddr, err);
      if (!pathInfo.empty()) {
        mli_msg << pathInfo;
      }

      mli_msg << " result=$";
      write_hex(mli_msg, err, 2);
      if (err == 0) {
        mli_msg << " OK\n";
      } else {
        const char* errName = error_name(err);
        if (errName[0] != '\0') {
          mli_msg << " ERROR (" << errName << ")\n";
        } else {
          mli_msg << " ERROR\n";
        }
      }

      return mli_msg.str();
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

  void CPU65C02::setTraceLog(std::ostream* traceLog) {
    m_traceLog = traceLog;
  }

  void CPU65C02::setDisassemblyTraceLog(std::ostream* disassemblyTraceLog) {
    m_disassemblyTraceLog = disassemblyTraceLog;
  }

  void CPU65C02::setJsrRtsTraceMonitorEnabled(bool enabled) {
    m_jsrRtsTraceMonitorEnabled = enabled;
  }

  void CPU65C02::recordPCChange(uint16_t fromPC, uint16_t toPC) {
    // Filter out ROM-internal transitions ($F800-$FFFF -> $F800-$FFFF)
    if (fromPC >= 0xF800 && toPC >= 0xF800) {
      return;
    }

    // Check if this is the same transition as the most recent entry
    if (m_pcRingIndex > 0 || m_pcRingFrom[PC_RING_SIZE - 1] != 0) {
      // Find the most recent entry
      size_t prevIndex = (m_pcRingIndex + PC_RING_SIZE - 1) % PC_RING_SIZE;
      if (m_pcRingFrom[prevIndex] == fromPC && m_pcRingTo[prevIndex] == toPC) {
        // Same transition as last entry, just increment the counter
        m_pcRingCount[prevIndex]++;
        return;
      }
    }

    // New transition: add to ring buffer with count = 1
    m_pcRingFrom[m_pcRingIndex]  = fromPC;
    m_pcRingTo[m_pcRingIndex]    = toPC;
    m_pcRingCount[m_pcRingIndex] = 1;
    m_pcRingIndex                = (m_pcRingIndex + 1) % PC_RING_SIZE;
  }

  uint16_t CPU65C02::control_flow_instruction_pc(uint8_t consumedOperandBytes) const {
    return static_cast<uint16_t>(m_r.pc - static_cast<uint16_t>(consumedOperandBytes + 1));
  }

  void CPU65C02::apply_control_flow_pc_change(uint16_t fromPC, uint16_t toPC) {
    m_r.pc = toPC;
    recordPCChange(fromPC, toPC);
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

    const MonitorSymbol* monitoredField = nullptr;
    uint8_t              oldValue       = 0;
    if (m_stepZpMonitorCaptureActive) {
      monitoredField = find_monitor_symbol(addr, MonitorSymbolWrite);
      if (monitoredField != nullptr) {
        oldValue = read_u8(m_mem.constBanks(), monitoredField->address);
      }
    }

    write_u8(m_mem.banks(), addr, value);

    if (monitoredField != nullptr) {
      append_step_zp_monitor_event(monitoredField->address, oldValue, value);
    }
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

    uint16_t resetVector = read16(VEC_RESET);
    m_r.pc               = resetVector;
    recordPCChange(0x0000, resetVector);  // from=0 for reset
    m_instructionCount           = 0;
    m_stepZpMonitorCaptureActive = false;
    m_stepZpMonitorEventCount    = 0;
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

  void CPU65C02::dumpDebugInfo(std::ostream& os) const {
    dump_stack(os, m_mem.constBanks(), m_r.sp);
    dump_pc_ring(os, m_pcRingFrom, m_pcRingTo, m_pcRingCount, PC_RING_SIZE, m_pcRingIndex);
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

  void CPU65C02::apply_relative_branch_offset(int8_t rel) {
    const uint16_t from        = m_r.pc;
    const uint16_t to          = static_cast<uint16_t>(from + rel);
    const bool     pageCrossed = (from & 0xFF00) != (to & 0xFF00);
    if (pageCrossed) {
      // Model the 65C02 page-cross extra read.
      dummyReadLastInstructionByte();
    }
    apply_control_flow_pc_change(from, to);
  }

  void CPU65C02::branch(bool cond) {
    const int8_t rel = rel8();
    if (!cond) {
      return;
    }
    apply_relative_branch_offset(rel);
  }

  uint32_t CPU65C02::handle_mli_jsr_trap() {
    // ProDOS MLI calling convention:
    //   JSR $BF00
    //   .byte callNumber
    //   .word paramBlockAddr
    // On return, execution continues after these 3 bytes.
    uint16_t callPC     = m_r.pc;  // PC at MLI call (points to call number)
    uint8_t  callNumber = read8(m_r.pc);
    uint16_t paramBlock = read16(static_cast<uint16_t>(m_r.pc + 1));
    uint16_t returnPC   = static_cast<uint16_t>(m_r.pc + 3);
    apply_control_flow_pc_change(0xBF00, returnPC);  // from=$BF00 (MLI entry point)

    uint8_t err = mli_dispatch(*m_mli, m_mem.banks(), callNumber, paramBlock);

    if (m_mliLog != nullptr || m_traceLog != nullptr) {
      const std::string mliMsg = build_mli_trap_log_message(
          m_instructionCount, m_mem.constBanks(), m_mli, callPC, callNumber, paramBlock, err);

      // Write to both logs
      if (m_mliLog != nullptr) {
        *m_mliLog << mliMsg;
        // Dump stack and PC ring on UNSUPPORTED_STOR_TYPE error
        if (err == ERR_UNSUPPORTED_STOR_TYPE) {
          dump_stack(*m_mliLog, m_mem.constBanks(), m_r.sp);
          dump_pc_ring(*m_mliLog, m_pcRingFrom, m_pcRingTo, m_pcRingCount, PC_RING_SIZE,
                       m_pcRingIndex);
        }
        m_mliLog->flush();
      }
      if (m_traceLog != nullptr) {
        *m_traceLog << mliMsg;
      }
    }

    // ProDOS convention: Carry set on error, A holds error code.
    m_r.a = err;
    setFlag(FLAG_C, err != 0);
    setNZ(m_r.a);

    // ProDOS MLI returns with decimal mode cleared.
    setFlag(FLAG_D, false);

    // QUIT ($65): stop emulation after standard MLI handling/logging.
    if (callNumber == 0x65 && err == ERR_NO_ERROR) {
      m_stopped = true;
      m_waiting = false;
    }

    return 6;
  }

  uint32_t CPU65C02::jsr_abs(uint16_t target) {
    // Special trap: JSR $BF00 invokes ProDOS MLI dispatch rather than changing PC.
    if (target == 0xBF00 && m_mli != nullptr) {
      return handle_mli_jsr_trap();
    }

    // Special trap: JSR $DCB8 redirects to $DCBD with A=$A0.
    if (target == 0xDCB8) {
      m_r.a  = 0xA0;
      target = 0xDCBD;
    }

    // Normal JSR behavior.
    // After operand fetch, PC points at the next instruction; JSR pushes (PC-1).
    uint16_t ret   = static_cast<uint16_t>(m_r.pc - 1);
    uint16_t jsrPC = control_flow_instruction_pc(2);
    push16(ret);
    apply_control_flow_pc_change(jsrPC, target);
    log_jsr_rts_transition("JSR", jsrPC, target);
    return 6;
  }

  void CPU65C02::emit_cout_char(uint8_t ch) {
    if (m_coutLog == nullptr) {
      return;
    }

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

  bool CPU65C02::execute_control_flow_jump_return_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x4C: {  // JMP abs
        uint16_t jmpPC  = control_flow_instruction_pc(0);
        uint16_t target = fetch16();
        apply_control_flow_pc_change(jmpPC, target);
        cycles = 3;
        return true;
      }
      case 0x6C: {  // JMP (abs)
        uint16_t jmpPC  = control_flow_instruction_pc(0);
        uint16_t ptr    = fetch16();
        uint16_t target = read16(ptr);
        if (ptr == COUT_VECTOR_PTR) {
          emit_cout_char(static_cast<uint8_t>(m_r.a & 0x7F));
        }
        apply_control_flow_pc_change(jmpPC, target);
        cycles = 5;
        return true;
      }
      case 0x7C: {  // JMP (abs,X)
        uint16_t jmpPC  = control_flow_instruction_pc(0);
        uint16_t target = addr_absind_x();
        apply_control_flow_pc_change(jmpPC, target);
        cycles = 6;
        return true;
      }
      case 0x20: {  // JSR abs
        uint16_t target = fetch16();
        cycles          = jsr_abs(target);
        return true;
      }
      case 0x60: {  // RTS
        uint16_t rtsPC      = control_flow_instruction_pc(0);
        uint16_t returnAddr = static_cast<uint16_t>(pull16() + 1);
        apply_control_flow_pc_change(rtsPC, returnAddr);
        log_jsr_rts_transition("RTS", rtsPC, returnAddr);
        cycles = 6;
        return true;
      }
      case 0x40: {  // RTI
        uint16_t rtiPC    = control_flow_instruction_pc(0);
        m_r.p             = static_cast<uint8_t>(pull8() | FLAG_U);
        uint16_t returnPC = pull16();
        apply_control_flow_pc_change(rtiPC, returnPC);
        cycles = 6;
        return true;
      }

      default:
        return false;
    }
  }

  bool CPU65C02::execute_control_flow_branch_opcode(uint8_t op, uint32_t& cycles) {
    const ControlFlowBranchMetadata* metadata = find_control_flow_branch_metadata(op);
    if (metadata == nullptr) {
      return false;
    }

    bool take = false;
    switch (metadata->condition) {
      case ControlFlowBranchCondition::Always:
        take = true;
        break;

      case ControlFlowBranchCondition::FlagSet:
        take = getFlag(metadata->flagMask);
        break;

      case ControlFlowBranchCondition::FlagClear:
        take = !getFlag(metadata->flagMask);
        break;
    }

    branch(take);
    cycles = metadata->cycles;
    return true;
  }

  bool CPU65C02::execute_control_flow_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x00: {  // BRK
        // BRK is treated as a 2-byte instruction; PC is incremented once more.
        uint16_t brkPC = control_flow_instruction_pc(0);
        m_r.pc         = static_cast<uint16_t>(m_r.pc + 1);
        push16(m_r.pc);
        push8(static_cast<uint8_t>(m_r.p | FLAG_B | FLAG_U));
        setFlag(FLAG_I, true);
        setFlag(FLAG_D, false);  // 65C02 clears D on interrupt
        uint16_t irqVector = read16(VEC_IRQ);
        apply_control_flow_pc_change(brkPC, irqVector);
        cycles = 7;
        return true;
      }

      case 0xEA:  // NOP
        cycles = 2;
        return true;

      case 0xDB:  // STP
        m_stopped = true;
        cycles    = 3;
        return true;

      case 0xCB:  // WAI
        m_waiting = true;
        cycles    = 3;
        return true;

      default:
        break;
    }

    if (execute_control_flow_jump_return_opcode(op, cycles)) {
      return true;
    }

    return execute_control_flow_branch_opcode(op, cycles);
  }

  bool CPU65C02::execute_flag_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x18:
        setFlag(FLAG_C, false);
        cycles = 2;
        return true;
      case 0x38:
        setFlag(FLAG_C, true);
        cycles = 2;
        return true;
      case 0x58:
        setFlag(FLAG_I, false);
        cycles = 2;
        return true;
      case 0x78:
        setFlag(FLAG_I, true);
        cycles = 2;
        return true;
      case 0xD8:
        setFlag(FLAG_D, false);
        cycles = 2;
        return true;
      case 0xF8:
        setFlag(FLAG_D, true);
        cycles = 2;
        return true;
      case 0xB8:
        setFlag(FLAG_V, false);
        cycles = 2;
        return true;

      default:
        return false;
    }
  }

  bool CPU65C02::execute_transfer_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0xAA:
        m_r.x = m_r.a;
        setNZ(m_r.x);
        cycles = 2;
        return true;
      case 0x8A:
        m_r.a = m_r.x;
        setNZ(m_r.a);
        cycles = 2;
        return true;
      case 0xA8:
        m_r.y = m_r.a;
        setNZ(m_r.y);
        cycles = 2;
        return true;
      case 0x98:
        m_r.a = m_r.y;
        setNZ(m_r.a);
        cycles = 2;
        return true;
      case 0xBA:
        m_r.x = m_r.sp;
        setNZ(m_r.x);
        cycles = 2;
        return true;
      case 0x9A:
        m_r.sp = m_r.x;
        cycles = 2;
        return true;

      default:
        return false;
    }
  }

  bool CPU65C02::execute_stack_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x48:
        push8(m_r.a);
        cycles = 3;
        return true;
      case 0x68:
        m_r.a = pull8();
        setNZ(m_r.a);
        cycles = 4;
        return true;
      case 0x08:
        push8(static_cast<uint8_t>(m_r.p | FLAG_B | FLAG_U));
        cycles = 3;
        return true;
      case 0x28:
        m_r.p  = static_cast<uint8_t>(pull8() | FLAG_U);
        cycles = 4;
        return true;
      case 0xDA:
        push8(m_r.x);
        cycles = 3;
        return true;
      case 0xFA:
        m_r.x = pull8();
        setNZ(m_r.x);
        cycles = 4;
        return true;
      case 0x5A:
        push8(m_r.y);
        cycles = 3;
        return true;
      case 0x7A:
        m_r.y = pull8();
        setNZ(m_r.y);
        cycles = 4;
        return true;

      default:
        return false;
    }
  }

  bool CPU65C02::execute_flag_transfer_stack_opcode(uint8_t op, uint32_t& cycles) {
    if (execute_flag_opcode(op, cycles)) {
      return true;
    }

    if (execute_transfer_opcode(op, cycles)) {
      return true;
    }

    return execute_stack_opcode(op, cycles);
  }

  bool CPU65C02::execute_accumulator_inc_dec_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x1A:
        m_r.a = static_cast<uint8_t>(m_r.a + 1);
        setNZ(m_r.a);
        cycles = 2;
        return true;
      case 0x3A:
        m_r.a = static_cast<uint8_t>(m_r.a - 1);
        setNZ(m_r.a);
        cycles = 2;
        return true;

      default:
        return false;
    }
  }

  bool CPU65C02::execute_accumulator_shift_rotate_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x0A:
        setFlag(FLAG_C, (m_r.a & 0x80) != 0);
        m_r.a = static_cast<uint8_t>(m_r.a << 1);
        setNZ(m_r.a);
        cycles = 2;
        return true;

      case 0x4A:
        setFlag(FLAG_C, (m_r.a & 0x01) != 0);
        m_r.a = static_cast<uint8_t>(m_r.a >> 1);
        setNZ(m_r.a);
        cycles = 2;
        return true;

      case 0x2A: {
        bool c = getFlag(FLAG_C);
        setFlag(FLAG_C, (m_r.a & 0x80) != 0);
        m_r.a = static_cast<uint8_t>((m_r.a << 1) | (c ? 1 : 0));
        setNZ(m_r.a);
        cycles = 2;
        return true;
      }

      case 0x6A: {
        bool c = getFlag(FLAG_C);
        setFlag(FLAG_C, (m_r.a & 0x01) != 0);
        m_r.a = static_cast<uint8_t>((m_r.a >> 1) | (c ? 0x80 : 0));
        setNZ(m_r.a);
        cycles = 2;
        return true;
      }

      default:
        return false;
    }
  }

  bool CPU65C02::execute_accumulator_misc_opcode(uint8_t op, uint32_t& cycles) {
    if (execute_accumulator_inc_dec_opcode(op, cycles)) {
      return true;
    }

    return execute_accumulator_shift_rotate_opcode(op, cycles);
  }

  bool CPU65C02::execute_load_store_load_immediate_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0xA9:  // LDA #imm
        m_r.a = fetch8();
        setNZ(m_r.a);
        cycles = 2;
        return true;
      case 0xA2:  // LDX #imm
        m_r.x = fetch8();
        setNZ(m_r.x);
        cycles = 2;
        return true;
      case 0xA0:  // LDY #imm
        m_r.y = fetch8();
        setNZ(m_r.y);
        cycles = 2;
        return true;
      default:
        return false;
    }
  }

  bool CPU65C02::execute_load_store_load_read_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0xA5:
        m_r.a = read8(addr_zp());
        setNZ(m_r.a);
        cycles = 3;
        return true;
      case 0xB5:
        m_r.a = read8(addr_zpx());
        setNZ(m_r.a);
        cycles = 4;
        return true;
      case 0xAD:
        m_r.a = read8(addr_abs());
        setNZ(m_r.a);
        cycles = 4;
        return true;
      case 0xA1:
        m_r.a = read8(addr_indx());
        setNZ(m_r.a);
        cycles = 6;
        return true;
      case 0xB2:  // LDA (zp)
        m_r.a = read8(addr_zpind());
        setNZ(m_r.a);
        cycles = 5;
        return true;

      case 0xA6:
        m_r.x = read8(addr_zp());
        setNZ(m_r.x);
        cycles = 3;
        return true;
      case 0xB6:
        m_r.x = read8(addr_zpy());
        setNZ(m_r.x);
        cycles = 4;
        return true;
      case 0xAE:
        m_r.x = read8(addr_abs());
        setNZ(m_r.x);
        cycles = 4;
        return true;

      case 0xA4:
        m_r.y = read8(addr_zp());
        setNZ(m_r.y);
        cycles = 3;
        return true;
      case 0xB4:
        m_r.y = read8(addr_zpx());
        setNZ(m_r.y);
        cycles = 4;
        return true;
      case 0xAC:
        m_r.y = read8(addr_abs());
        setNZ(m_r.y);
        cycles = 4;
        return true;

      default:
        return false;
    }
  }

  bool CPU65C02::execute_load_store_load_page_cross_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0xBD: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        cycles = static_cast<uint32_t>(4 + (pc ? 1 : 0));
        return true;
      }
      case 0xB9: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        cycles = static_cast<uint32_t>(4 + (pc ? 1 : 0));
        return true;
      }
      case 0xB1: {
        bool     pc = false;
        uint16_t a  = addr_indy(pc);
        m_r.a       = read8_pageCrossed(a, pc);
        setNZ(m_r.a);
        cycles = static_cast<uint32_t>(5 + (pc ? 1 : 0));
        return true;
      }
      case 0xBE: {
        bool     pc = false;
        uint16_t a  = addr_absy(pc);
        m_r.x       = read8_pageCrossed(a, pc);
        setNZ(m_r.x);
        cycles = static_cast<uint32_t>(4 + (pc ? 1 : 0));
        return true;
      }
      case 0xBC: {
        bool     pc = false;
        uint16_t a  = addr_absx(pc);
        m_r.y       = read8_pageCrossed(a, pc);
        setNZ(m_r.y);
        cycles = static_cast<uint32_t>(4 + (pc ? 1 : 0));
        return true;
      }

      default:
        return false;
    }
  }

  bool CPU65C02::execute_load_store_store_direct_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x85:
        write8(addr_zp(), m_r.a);
        cycles = 3;
        return true;
      case 0x95:
        write8(addr_zpx(), m_r.a);
        cycles = 4;
        return true;
      case 0x8D:
        write8(addr_abs(), m_r.a);
        cycles = 4;
        return true;
      case 0x81:
        write8(addr_indx(), m_r.a);
        cycles = 6;
        return true;
      case 0x92:  // STA (zp)
        write8(addr_zpind(), m_r.a);
        cycles = 5;
        return true;

      case 0x86:
        write8(addr_zp(), m_r.x);
        cycles = 3;
        return true;
      case 0x96:
        write8(addr_zpy(), m_r.x);
        cycles = 4;
        return true;
      case 0x8E:
        write8(addr_abs(), m_r.x);
        cycles = 4;
        return true;

      case 0x84:
        write8(addr_zp(), m_r.y);
        cycles = 3;
        return true;
      case 0x94:
        write8(addr_zpx(), m_r.y);
        cycles = 4;
        return true;
      case 0x8C:
        write8(addr_abs(), m_r.y);
        cycles = 4;
        return true;

      default:
        return false;
    }
  }

  bool CPU65C02::execute_load_store_store_indexed_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x9D: {
        bool pc = false;
        write8(addr_absx(pc), m_r.a);
        cycles = 5;
        return true;
      }
      case 0x99: {
        bool pc = false;
        write8(addr_absy(pc), m_r.a);
        cycles = 5;
        return true;
      }
      case 0x91: {
        bool pc = false;
        write8(addr_indy(pc), m_r.a);
        cycles = 6;
        return true;
      }

      default:
        return false;
    }
  }

  bool CPU65C02::execute_load_store_store_zero_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0x64:
        write8(addr_zp(), 0);
        cycles = 3;
        return true;
      case 0x74:
        write8(addr_zpx(), 0);
        cycles = 4;
        return true;
      case 0x9C:
        write8(addr_abs(), 0);
        cycles = 4;
        return true;
      case 0x9E: {
        bool pc = false;
        write8(addr_absx(pc), 0);
        cycles = 5;
        return true;
      }

      default:
        return false;
    }
  }

  bool CPU65C02::execute_load_store_opcode(uint8_t op, uint32_t& cycles) {
    if (execute_load_store_load_immediate_opcode(op, cycles)) {
      return true;
    }
    if (execute_load_store_load_read_opcode(op, cycles)) {
      return true;
    }
    if (execute_load_store_load_page_cross_opcode(op, cycles)) {
      return true;
    }
    if (execute_load_store_store_direct_opcode(op, cycles)) {
      return true;
    }
    if (execute_load_store_store_indexed_opcode(op, cycles)) {
      return true;
    }
    if (execute_load_store_store_zero_opcode(op, cycles)) {
      return true;
    }
    return false;
  }

  bool CPU65C02::read_bit_operand_for_mode(BitFamilyMode mode, uint8_t& operand, uint32_t& cycles) {
    switch (mode) {
      case BitFamilyMode::Immediate:
        operand = fetch8();
        cycles  = 2;
        return true;
      case BitFamilyMode::Zp:
        operand = read8(addr_zp());
        cycles  = 3;
        return true;
      case BitFamilyMode::Abs:
        operand = read8(addr_abs());
        cycles  = 4;
        return true;
      case BitFamilyMode::Zpx:
        operand = read8(addr_zpx());
        cycles  = 4;
        return true;
      case BitFamilyMode::Absx: {
        bool     pageCrossed = false;
        uint16_t addr        = addr_absx(pageCrossed);
        operand              = read8_pageCrossed(addr, pageCrossed);
        cycles               = static_cast<uint32_t>(4 + (pageCrossed ? 1 : 0));
        return true;
      }
      default:
        return false;
    }
  }

  bool CPU65C02::read_bit_modify_target_for_mode(BitFamilyMode mode, uint16_t& addr,
                                                 uint32_t& cycles) {
    switch (mode) {
      case BitFamilyMode::Zp:
        addr   = addr_zp();
        cycles = 5;
        return true;
      case BitFamilyMode::Abs:
        addr   = addr_abs();
        cycles = 6;
        return true;
      default:
        return false;
    }
  }

  void CPU65C02::apply_bit_test_flags(uint8_t operand, bool updateNV) {
    setFlag(FLAG_Z, (m_r.a & operand) == 0);
    if (updateNV) {
      setFlag(FLAG_N, (operand & 0x80) != 0);
      setFlag(FLAG_V, (operand & 0x40) != 0);
    }
  }

  bool CPU65C02::execute_bit_family_opcode(uint8_t op, uint32_t& cycles) {
    const BitFamilyMetadata* metadata = find_bit_family_metadata(op);
    if (metadata == nullptr) {
      return false;
    }

    BitFamilyMode mode = BitFamilyMode::Immediate;
    switch (metadata->mode) {
      case BitFamilyModeMeta::Immediate:
        mode = BitFamilyMode::Immediate;
        break;
      case BitFamilyModeMeta::Zp:
        mode = BitFamilyMode::Zp;
        break;
      case BitFamilyModeMeta::Abs:
        mode = BitFamilyMode::Abs;
        break;
      case BitFamilyModeMeta::Zpx:
        mode = BitFamilyMode::Zpx;
        break;
      case BitFamilyModeMeta::Absx:
        mode = BitFamilyMode::Absx;
        break;
    }

    switch (metadata->kind) {
      case BitFamilyKind::Bit: {
        uint8_t  operand     = 0;
        uint32_t bitOpCycles = 0;
        if (!read_bit_operand_for_mode(mode, operand, bitOpCycles)) {
          return false;
        }
        apply_bit_test_flags(operand, metadata->updateNV);
        cycles = bitOpCycles;
        return true;
      }

      case BitFamilyKind::Tsb:
      case BitFamilyKind::Trb: {
        uint16_t addr        = 0;
        uint32_t bitOpCycles = 0;
        if (!read_bit_modify_target_for_mode(mode, addr, bitOpCycles)) {
          return false;
        }
        if (metadata->kind == BitFamilyKind::Tsb) {
          tsb(addr);
        } else {
          trb(addr);
        }
        cycles = bitOpCycles;
        return true;
      }

      default:
        return false;
    }
  }

  bool CPU65C02::execute_nop_variant_opcode(uint8_t op, uint32_t& cycles) {
    const NopVariantMetadata* metadata = find_nop_variant_metadata(op);
    if (metadata == nullptr) {
      return false;
    }

    switch (metadata->mode) {
      case NopVariantMode::Implied:
        break;
      case NopVariantMode::ImmediateDiscard:
        (void)fetch8();
        break;
      case NopVariantMode::ZpRead: {
        uint8_t zp = fetch8();
        (void)read8(zp);
        break;
      }
      case NopVariantMode::ZpXRead: {
        uint8_t zp = fetch8();
        (void)read8(static_cast<uint8_t>(zp + m_r.x));
        break;
      }
      case NopVariantMode::AbsRead: {
        uint16_t a = fetch16();
        (void)read8(a);
        break;
      }
    }

    cycles = metadata->cycles;
    return true;
  }

  bool CPU65C02::execute_misc_tail_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0xE8:
        m_r.x = static_cast<uint8_t>(m_r.x + 1);
        setNZ(m_r.x);
        cycles = 2;
        return true;
      case 0xCA:
        m_r.x = static_cast<uint8_t>(m_r.x - 1);
        setNZ(m_r.x);
        cycles = 2;
        return true;
      case 0xC8:
        m_r.y = static_cast<uint8_t>(m_r.y + 1);
        setNZ(m_r.y);
        cycles = 2;
        return true;
      case 0x88:
        m_r.y = static_cast<uint8_t>(m_r.y - 1);
        setNZ(m_r.y);
        cycles = 2;
        return true;
      default:
        return false;
    }
  }

  bool CPU65C02::execute_compare_xy_opcode(uint8_t op, uint32_t& cycles) {
    switch (op) {
      case 0xE0:
        (void)cmp(m_r.x, fetch8());
        cycles = 2;
        return true;
      case 0xE4:
        (void)cmp(m_r.x, read8(addr_zp()));
        cycles = 3;
        return true;
      case 0xEC:
        (void)cmp(m_r.x, read8(addr_abs()));
        cycles = 4;
        return true;
      case 0xC0:
        (void)cmp(m_r.y, fetch8());
        cycles = 2;
        return true;
      case 0xC4:
        (void)cmp(m_r.y, read8(addr_zp()));
        cycles = 3;
        return true;
      case 0xCC:
        (void)cmp(m_r.y, read8(addr_abs()));
        cycles = 4;
        return true;
      default:
        return false;
    }
  }

  bool CPU65C02::read_alu_operand_for_mode(uint8_t mode, uint8_t& operand, uint32_t& cycles) {
    const AluModeMetadata* metadata = find_alu_mode_metadata(mode);
    if (metadata == nullptr) {
      return false;
    }

    switch (metadata->operandMode) {
      case AluOperandMode::Immediate:
        operand = fetch8();
        cycles  = metadata->baseCycles;
        return true;

      case AluOperandMode::Zp:
        operand = read8(addr_zp());
        cycles  = metadata->baseCycles;
        return true;

      case AluOperandMode::ZpX:
        operand = read8(addr_zpx());
        cycles  = metadata->baseCycles;
        return true;

      case AluOperandMode::Abs:
        operand = read8(addr_abs());
        cycles  = metadata->baseCycles;
        return true;

      case AluOperandMode::AbsX: {
        bool     pageCrossed = false;
        uint16_t addr        = addr_absx(pageCrossed);
        operand              = read8_pageCrossed(addr, pageCrossed);
        cycles               = static_cast<uint32_t>(metadata->baseCycles +
                                       ((metadata->hasPageCrossPenalty && pageCrossed) ? 1 : 0));
        return true;
      }

      case AluOperandMode::AbsY: {
        bool     pageCrossed = false;
        uint16_t addr        = addr_absy(pageCrossed);
        operand              = read8_pageCrossed(addr, pageCrossed);
        cycles               = static_cast<uint32_t>(metadata->baseCycles +
                                       ((metadata->hasPageCrossPenalty && pageCrossed) ? 1 : 0));
        return true;
      }

      case AluOperandMode::IndX:
        operand = read8(addr_indx());
        cycles  = metadata->baseCycles;
        return true;

      case AluOperandMode::IndY: {
        bool     pageCrossed = false;
        uint16_t addr        = addr_indy(pageCrossed);
        operand              = read8_pageCrossed(addr, pageCrossed);
        cycles               = static_cast<uint32_t>(metadata->baseCycles +
                                       ((metadata->hasPageCrossPenalty && pageCrossed) ? 1 : 0));
        return true;
      }

      case AluOperandMode::ZpInd:
        operand = read8(addr_zpind());
        cycles  = metadata->baseCycles;
        return true;
    }

    return false;
  }

  uint32_t CPU65C02::execute_alu_family_opcode(uint8_t op) {
    uint8_t  operand = 0;
    uint32_t cycles  = 0;
    if (!read_alu_operand_for_mode(static_cast<uint8_t>(op & 0x1F), operand, cycles)) {
      return 0;
    }

    const AluGroupMetadata* groupMetadata =
        find_alu_group_metadata(static_cast<uint8_t>(op & 0xE0));
    if (groupMetadata == nullptr) {
      return 0;
    }

    switch (groupMetadata->operation) {
      case AluGroupOperation::Ora:
        m_r.a = static_cast<uint8_t>(m_r.a | operand);
        setNZ(m_r.a);
        return cycles;

      case AluGroupOperation::And:
        m_r.a = static_cast<uint8_t>(m_r.a & operand);
        setNZ(m_r.a);
        return cycles;

      case AluGroupOperation::Eor:
        m_r.a = static_cast<uint8_t>(m_r.a ^ operand);
        setNZ(m_r.a);
        return cycles;

      case AluGroupOperation::Adc:
        m_r.a = adc(m_r.a, operand);
        return cycles;

      case AluGroupOperation::Cmp:
        (void)cmp(m_r.a, operand);
        return cycles;

      case AluGroupOperation::Sbc:
        m_r.a = sbc(m_r.a, operand);
        return cycles;
    }

    return 0;
  }

  bool CPU65C02::read_rmw_target_for_mode(uint8_t mode, uint16_t& addr, uint32_t& cycles) {
    const RmwModeMetadata* metadata = find_rmw_mode_metadata(mode);
    if (metadata == nullptr) {
      return false;
    }

    switch (metadata->targetMode) {
      case RmwTargetMode::Zp:
        addr   = addr_zp();
        cycles = metadata->cycles;
        return true;

      case RmwTargetMode::ZpX:
        addr   = addr_zpx();
        cycles = metadata->cycles;
        return true;

      case RmwTargetMode::Abs:
        addr   = addr_abs();
        cycles = metadata->cycles;
        return true;

      case RmwTargetMode::AbsX: {
        bool pc = false;
        addr    = addr_absx(pc);
        cycles  = metadata->cycles;
        return true;
      }
    }

    return false;
  }

  uint8_t CPU65C02::apply_rmw_family_op(uint8_t group, uint8_t value) {
    const RmwGroupMetadata* groupMetadata = find_rmw_group_metadata(group);
    if (groupMetadata == nullptr) {
      return value;
    }

    switch (groupMetadata->operation) {
      case RmwGroupOperation::Inc:
        return static_cast<uint8_t>(value + 1);

      case RmwGroupOperation::Dec:
        return static_cast<uint8_t>(value - 1);

      case RmwGroupOperation::Asl:
        setFlag(FLAG_C, (value & 0x80) != 0);
        return static_cast<uint8_t>(value << 1);

      case RmwGroupOperation::Lsr:
        setFlag(FLAG_C, (value & 0x01) != 0);
        return static_cast<uint8_t>(value >> 1);

      case RmwGroupOperation::Rol: {
        bool c = getFlag(FLAG_C);
        setFlag(FLAG_C, (value & 0x80) != 0);
        return static_cast<uint8_t>((value << 1) | (c ? 1 : 0));
      }

      case RmwGroupOperation::Ror: {
        bool c = getFlag(FLAG_C);
        setFlag(FLAG_C, (value & 0x01) != 0);
        return static_cast<uint8_t>((value >> 1) | (c ? 0x80 : 0));
      }
    }

    return value;
  }

  uint32_t CPU65C02::execute_rmw_family_opcode(uint8_t op) {
    uint16_t addr   = 0;
    uint32_t cycles = 0;
    if (!read_rmw_target_for_mode(static_cast<uint8_t>(op & 0x1F), addr, cycles)) {
      return 0;
    }

    uint8_t group = static_cast<uint8_t>(op & 0xE0);
    if (find_rmw_group_metadata(group) == nullptr) {
      return 0;
    }

    uint8_t v = read8(addr);
    v         = apply_rmw_family_op(group, v);
    write8(addr, v);
    setNZ(v);
    return cycles;
  }

  uint8_t CPU65C02::bit_index_from_opcode(uint8_t op) {
    return static_cast<uint8_t>((op >> 4) & 0x07);
  }

  uint8_t CPU65C02::bit_mask_for_index(uint8_t bitIndex) {
    return static_cast<uint8_t>(1u << bitIndex);
  }

  uint8_t CPU65C02::apply_rmb_smb_bit(uint8_t value, uint8_t bitIndex, bool setBit) {
    const uint8_t mask = bit_mask_for_index(bitIndex);
    if (setBit) {
      return static_cast<uint8_t>(value | mask);
    }
    return static_cast<uint8_t>(value & static_cast<uint8_t>(~mask));
  }

  bool CPU65C02::test_bit_index(uint8_t value, uint8_t bitIndex) {
    return (value & bit_mask_for_index(bitIndex)) != 0;
  }

  uint32_t CPU65C02::execute_rmb_smb_opcode(uint8_t op) {
    if (classify_bit_opcode_family(op) != BitOpcodeFamily::RmbSmb) {
      return 0;
    }

    const uint8_t bit    = bit_index_from_opcode(op);
    const bool    setBit = (op & 0x80) != 0;
    uint8_t       zp     = fetch8();
    uint8_t       m      = read8(zp);
    m                    = apply_rmb_smb_bit(m, bit, setBit);
    write8(zp, m);
    return 5;
  }

  uint32_t CPU65C02::execute_bbr_bbs_opcode(uint8_t op) {
    if (classify_bit_opcode_family(op) != BitOpcodeFamily::BbrBbs) {
      return 0;
    }

    const uint8_t bit   = bit_index_from_opcode(op);
    const bool    isBBS = (op & 0x80) != 0;
    uint8_t       zp    = fetch8();
    int8_t        rel   = static_cast<int8_t>(fetch8());

    uint8_t m      = read8(zp);
    bool    bitSet = test_bit_index(m, bit);
    bool    take   = isBBS ? bitSet : !bitSet;
    if (take) {
      apply_relative_branch_offset(rel);
    }
    return 5;
  }

  uint32_t CPU65C02::execute_fallback_router_opcode(uint8_t op) {
    uint32_t lowRiskCycles = 0;
    if (execute_flag_transfer_stack_opcode(op, lowRiskCycles)) {
      return lowRiskCycles;
    }
    if (execute_accumulator_misc_opcode(op, lowRiskCycles)) {
      return lowRiskCycles;
    }
    if (execute_load_store_opcode(op, lowRiskCycles)) {
      return lowRiskCycles;
    }
    if (execute_bit_family_opcode(op, lowRiskCycles)) {
      return lowRiskCycles;
    }
    if (execute_nop_variant_opcode(op, lowRiskCycles)) {
      return lowRiskCycles;
    }

    // Default for reserved/unknown opcodes: treat as 1-byte NOP.
    // Many 65C02 implementations treat undefined opcodes as NOP.
    switch (classify_fallback_route(op)) {
      case FallbackRoute::MiscTail:
        if (execute_misc_tail_opcode(op, lowRiskCycles)) {
          return lowRiskCycles;
        }
        break;

      case FallbackRoute::AluFamily:
        return execute_alu_family_opcode(op);

      case FallbackRoute::CompareXY:
        if (execute_compare_xy_opcode(op, lowRiskCycles)) {
          return lowRiskCycles;
        }
        break;

      case FallbackRoute::RmwFamily:
        return execute_rmw_family_opcode(op);

      case FallbackRoute::None:
        break;
    }

    return 2;
  }

  uint32_t CPU65C02::execute(uint8_t op) {
    // Rockwell/WDC 65C02 bit manipulation/branch opcodes.
    // RMBn: 07,17,27,37,47,57,67,77 (clear bit n in zp)
    // SMBn: 87,97,A7,B7,C7,D7,E7,F7 (set bit n in zp).
    uint32_t specialCycles = execute_rmb_smb_opcode(op);
    if (specialCycles != 0) {
      return specialCycles;
    }

    // BBRn: 0F,1F,2F,3F,4F,5F,6F,7F (branch if bit n clear)
    // BBSn: 8F,9F,AF,BF,CF,DF,EF,FF (branch if bit n set).
    specialCycles = execute_bbr_bbs_opcode(op);
    if (specialCycles != 0) {
      return specialCycles;
    }

    uint32_t controlCycles = 0;
    if (execute_control_flow_opcode(op, controlCycles)) {
      return controlCycles;
    }

    return execute_fallback_router_opcode(op);
  }

  void CPU65C02::log_step_trace_marker(uint16_t pc) {
    if (m_traceLog == nullptr) {
      return;
    }

    // JSR/RTS monitor mode owns transition logging and symbol annotation.
    if (m_jsrRtsTraceMonitorEnabled) {
      return;
    }

    const MonitorSymbol* marker = find_monitor_symbol(pc, MonitorSymbolPc);
    // MonitorSymbol vicinityMarker = {};
    // if (marker == nullptr) {
    //   if (pc > 0x7C98 - 0x100 && pc < 0x7C98 + 0x100) {
    //     vicinityMarker = MonitorSymbol{pc, "PrtSetup vicinity", MonitorSymbolPc};
    //     marker         = &vicinityMarker;
    //   }
    // }
    if (marker == nullptr) {
      return;
    }

    *m_traceLog << "@" << m_instructionCount << " PC=$" << std::hex << std::uppercase
                << std::setfill('0') << std::setw(4) << marker->address << std::dec << " "
                << marker->name;

    *m_traceLog << "\n";
  }

  void CPU65C02::log_jsr_rts_transition(const char* opcodeName, uint16_t fromPC, uint16_t toPC) {
    if (!m_jsrRtsTraceMonitorEnabled || m_traceLog == nullptr) {
      return;
    }

    *m_traceLog << "@" << m_instructionCount << " PC=$";
    write_hex(*m_traceLog, toPC, 4);
    *m_traceLog << " " << opcodeName << ": $";
    write_hex(*m_traceLog, fromPC, 4);
    *m_traceLog << " -> $";
    write_hex(*m_traceLog, toPC, 4);

    const MonitorSymbol* symbol = find_monitor_symbol(toPC, MonitorSymbolPc);
    if (symbol != nullptr) {
      *m_traceLog << " " << symbol->name;
    }

    *m_traceLog << "\n";
  }

  void CPU65C02::begin_step_zp_monitor_capture() {
    m_stepZpMonitorCaptureActive = true;
    m_stepZpMonitorEventCount    = 0;
  }

  void CPU65C02::end_step_zp_monitor_capture() {
    m_stepZpMonitorCaptureActive = false;
    m_stepZpMonitorEventCount    = 0;
  }

  void CPU65C02::append_step_zp_monitor_event(uint16_t addr, uint8_t oldValue, uint8_t newValue) {
    if (m_stepZpMonitorEventCount >= ZP_MONITOR_MAX_EVENTS) {
      return;
    }

    ZpMonitorEvent& event = m_stepZpMonitorEvents[m_stepZpMonitorEventCount];
    event.address         = addr;
    event.oldValue        = oldValue;
    event.newValue        = newValue;
    m_stepZpMonitorEventCount++;
  }

  void CPU65C02::log_step_zp_monitor_events(uint8_t opcode) {
    if (m_traceLog == nullptr) {
      return;
    }

    const char* mutator = zp_monitor_mutator_name(opcode);

    for (size_t i = 0; i < m_stepZpMonitorEventCount; ++i) {
      const ZpMonitorEvent& event       = m_stepZpMonitorEvents[i];
      const MonitorSymbol*  fieldSymbol = find_monitor_symbol(event.address, MonitorSymbolWrite);
      if (fieldSymbol == nullptr) {
        continue;
      }

      *m_traceLog << "@" << m_instructionCount << " PC=$" << std::hex << std::uppercase
                  << std::setfill('0') << std::setw(4) << m_r.pc << " ";
      if (mutator != nullptr) {
        *m_traceLog << mutator << " ";
      }
      *m_traceLog << fieldSymbol->name << "($" << std::setw(2)
                  << static_cast<unsigned>(event.address) << "): $" << std::setw(2)
                  << static_cast<unsigned>(event.oldValue) << " -> $" << std::setw(2)
                  << static_cast<unsigned>(event.newValue) << std::dec << "\n";
    }
  }

  static const char* control_flow_branch_mnemonic(uint8_t opcode) {
    switch (opcode) {
      case 0x80:
        return "BRA";
      case 0x10:
        return "BPL";
      case 0x30:
        return "BMI";
      case 0x50:
        return "BVC";
      case 0x70:
        return "BVS";
      case 0x90:
        return "BCC";
      case 0xB0:
        return "BCS";
      case 0xD0:
        return "BNE";
      case 0xF0:
        return "BEQ";
      default:
        return nullptr;
    }
  }

  static const char* alu_group_mnemonic(uint8_t group) {
    const RmwGroupMetadata* groupMetadata = find_rmw_group_metadata(group);
    if (groupMetadata == nullptr) {
      return nullptr;
    }
    switch (groupMetadata->operation) {
      case RmwGroupOperation::Asl:
        return "ASL";
      case RmwGroupOperation::Rol:
        return "ROL";
      case RmwGroupOperation::Lsr:
        return "LSR";
      case RmwGroupOperation::Ror:
        return "ROR";
      case RmwGroupOperation::Dec:
        return "DEC";
      case RmwGroupOperation::Inc:
        return "INC";
    }
    return nullptr;
  }

  static const char* alu_mnemonic(uint8_t group) {
    const AluGroupMetadata* groupMetadata = find_alu_group_metadata(group);
    if (groupMetadata == nullptr) {
      return nullptr;
    }
    switch (groupMetadata->operation) {
      case AluGroupOperation::Ora:
        return "ORA";
      case AluGroupOperation::And:
        return "AND";
      case AluGroupOperation::Eor:
        return "EOR";
      case AluGroupOperation::Adc:
        return "ADC";
      case AluGroupOperation::Cmp:
        return "CMP";
      case AluGroupOperation::Sbc:
        return "SBC";
    }
    return nullptr;
  }

  static void append_symbol_suffix_for_address(std::ostream& os, uint16_t addr) {
    const MonitorSymbol* symbol = find_any_monitor_symbol(addr);
    if (symbol == nullptr) {
      return;
    }
    os << " (" << symbol->name << ")";
  }

  static void append_abs_operand(std::ostream& os, uint16_t addr, const char* trailing = "") {
    os << "$";
    write_hex(os, addr, 4);
    os << trailing;
    append_symbol_suffix_for_address(os, addr);
  }

  static void append_zp_operand(std::ostream& os, uint8_t addr, const char* trailing = "") {
    os << "$";
    write_hex(os, addr, 2);
    os << trailing;
    append_symbol_suffix_for_address(os, addr);
  }

  static std::string disassembly_text_for_opcode(const ConstMemoryBanks& banks, uint16_t pc,
                                                 uint8_t opcode) {
    const uint8_t  b1       = read_u8(banks, static_cast<uint16_t>(pc + 1));
    const uint8_t  b2       = read_u8(banks, static_cast<uint16_t>(pc + 2));
    const uint16_t absValue = make_u16(b1, b2);

    std::ostringstream os;

    switch (opcode) {
      case 0x00:
        return "BRK";
      case 0x40:
        return "RTI";
      case 0x60:
        return "RTS";
      case 0xEA:
        return "NOP";
      case 0xDB:
        return "STP";
      case 0xCB:
        return "WAI";

      case 0x18:
        return "CLC";
      case 0x38:
        return "SEC";
      case 0x58:
        return "CLI";
      case 0x78:
        return "SEI";
      case 0xD8:
        return "CLD";
      case 0xF8:
        return "SED";
      case 0xB8:
        return "CLV";

      case 0xAA:
        return "TAX";
      case 0x8A:
        return "TXA";
      case 0xA8:
        return "TAY";
      case 0x98:
        return "TYA";
      case 0xBA:
        return "TSX";
      case 0x9A:
        return "TXS";

      case 0x48:
        return "PHA";
      case 0x68:
        return "PLA";
      case 0x08:
        return "PHP";
      case 0x28:
        return "PLP";
      case 0xDA:
        return "PHX";
      case 0xFA:
        return "PLX";
      case 0x5A:
        return "PHY";
      case 0x7A:
        return "PLY";

      case 0xE8:
        return "INX";
      case 0xCA:
        return "DEX";
      case 0xC8:
        return "INY";
      case 0x88:
        return "DEY";

      case 0x1A:
        return "INC A";
      case 0x3A:
        return "DEC A";
      case 0x0A:
        return "ASL A";
      case 0x4A:
        return "LSR A";
      case 0x2A:
        return "ROL A";
      case 0x6A:
        return "ROR A";

      case 0x20: {
        if (absValue == 0xBF00) {
          const uint8_t  callNumber = read_u8(banks, static_cast<uint16_t>(pc + 3));
          const uint16_t paramBlock = read_u16_le(banks, static_cast<uint16_t>(pc + 4));
          os << "MLI .byte $";
          write_hex(os, callNumber, 2);
          os << " .word $";
          write_hex(os, paramBlock, 4);
          os << " (" << mli_call_name(callNumber) << ")";
          return os.str();
        }
        os << "JSR ";
        append_abs_operand(os, absValue);
        return os.str();
      }

      case 0x4C:
        os << "JMP ";
        append_abs_operand(os, absValue);
        return os.str();
      case 0x6C:
        os << "JMP (";
        write_hex(os, absValue, 4);
        os << ")";
        append_symbol_suffix_for_address(os, absValue);
        return os.str();
      case 0x7C:
        os << "JMP (";
        write_hex(os, absValue, 4);
        os << ",X)";
        append_symbol_suffix_for_address(os, absValue);
        return os.str();

      case 0xA9:
      case 0xA2:
      case 0xA0: {
        const char* mnemonic = (opcode == 0xA9) ? "LDA" : ((opcode == 0xA2) ? "LDX" : "LDY");
        os << mnemonic << " #$";
        write_hex(os, b1, 2);
        return os.str();
      }

      case 0xA5:
      case 0xA6:
      case 0xA4:
      case 0x85:
      case 0x86:
      case 0x84:
      case 0x64:
      case 0xC4:
      case 0xE4: {
        const char* mnemonic = nullptr;
        switch (opcode) {
          case 0xA5:
            mnemonic = "LDA";
            break;
          case 0xA6:
            mnemonic = "LDX";
            break;
          case 0xA4:
            mnemonic = "LDY";
            break;
          case 0x85:
            mnemonic = "STA";
            break;
          case 0x86:
            mnemonic = "STX";
            break;
          case 0x84:
            mnemonic = "STY";
            break;
          case 0x64:
            mnemonic = "STZ";
            break;
          case 0xC4:
            mnemonic = "CPY";
            break;
          case 0xE4:
            mnemonic = "CPX";
            break;
        }
        os << mnemonic << " ";
        append_zp_operand(os, b1);
        return os.str();
      }

      case 0xB5:
      case 0xB4:
      case 0x95:
      case 0x94:
      case 0x74: {
        const char* mnemonic = nullptr;
        switch (opcode) {
          case 0xB5:
            mnemonic = "LDA";
            break;
          case 0xB4:
            mnemonic = "LDY";
            break;
          case 0x95:
            mnemonic = "STA";
            break;
          case 0x94:
            mnemonic = "STY";
            break;
          case 0x74:
            mnemonic = "STZ";
            break;
        }
        os << mnemonic << " ";
        append_zp_operand(os, b1, ",X");
        return os.str();
      }

      case 0xB6:
      case 0x96: {
        const char* mnemonic = (opcode == 0xB6) ? "LDX" : "STX";
        os << mnemonic << " ";
        append_zp_operand(os, b1, ",Y");
        return os.str();
      }

      case 0xAD:
      case 0xAE:
      case 0xAC:
      case 0x8D:
      case 0x8E:
      case 0x8C:
      case 0x9C:
      case 0xCC:
      case 0xEC: {
        const char* mnemonic = nullptr;
        switch (opcode) {
          case 0xAD:
            mnemonic = "LDA";
            break;
          case 0xAE:
            mnemonic = "LDX";
            break;
          case 0xAC:
            mnemonic = "LDY";
            break;
          case 0x8D:
            mnemonic = "STA";
            break;
          case 0x8E:
            mnemonic = "STX";
            break;
          case 0x8C:
            mnemonic = "STY";
            break;
          case 0x9C:
            mnemonic = "STZ";
            break;
          case 0xCC:
            mnemonic = "CPY";
            break;
          case 0xEC:
            mnemonic = "CPX";
            break;
        }
        os << mnemonic << " ";
        append_abs_operand(os, absValue);
        return os.str();
      }

      case 0xBD:
      case 0xBC:
      case 0x9D:
      case 0x9E: {
        const char* mnemonic = nullptr;
        switch (opcode) {
          case 0xBD:
            mnemonic = "LDA";
            break;
          case 0xBC:
            mnemonic = "LDY";
            break;
          case 0x9D:
            mnemonic = "STA";
            break;
          case 0x9E:
            mnemonic = "STZ";
            break;
        }
        os << mnemonic << " ";
        append_abs_operand(os, absValue, ",X");
        return os.str();
      }

      case 0xB9:
      case 0xBE:
      case 0x99: {
        const char* mnemonic = nullptr;
        switch (opcode) {
          case 0xB9:
            mnemonic = "LDA";
            break;
          case 0xBE:
            mnemonic = "LDX";
            break;
          case 0x99:
            mnemonic = "STA";
            break;
        }
        os << mnemonic << " ";
        append_abs_operand(os, absValue, ",Y");
        return os.str();
      }

      case 0xA1:
      case 0x81: {
        const char* mnemonic = (opcode == 0xA1) ? "LDA" : "STA";
        os << mnemonic << " ($";
        write_hex(os, b1, 2);
        os << ",X)";
        append_symbol_suffix_for_address(os, b1);
        return os.str();
      }

      case 0xB1:
      case 0x91: {
        const char* mnemonic = (opcode == 0xB1) ? "LDA" : "STA";
        os << mnemonic << " ($";
        write_hex(os, b1, 2);
        os << "),Y";
        append_symbol_suffix_for_address(os, b1);
        return os.str();
      }

      case 0xB2:
      case 0x92: {
        const char* mnemonic = (opcode == 0xB2) ? "LDA" : "STA";
        os << mnemonic << " ($";
        write_hex(os, b1, 2);
        os << ")";
        append_symbol_suffix_for_address(os, b1);
        return os.str();
      }

      case 0xC0:
      case 0xE0: {
        const char* mnemonic = (opcode == 0xC0) ? "CPY" : "CPX";
        os << mnemonic << " #$";
        write_hex(os, b1, 2);
        return os.str();
      }

      default:
        break;
    }

    if ((opcode & 0x1F) == 0x07) {
      const uint8_t bit = static_cast<uint8_t>((opcode >> 4) & 0x07);
      os << (((opcode & 0x80) == 0) ? "RMB" : "SMB") << static_cast<unsigned>(bit) << " ";
      append_zp_operand(os, b1);
      return os.str();
    }

    if ((opcode & 0x1F) == 0x0F) {
      const uint8_t  bit    = static_cast<uint8_t>((opcode >> 4) & 0x07);
      const uint16_t target = static_cast<uint16_t>(pc + 3 + static_cast<int8_t>(b2));
      os << (((opcode & 0x80) == 0) ? "BBR" : "BBS") << static_cast<unsigned>(bit) << " ";
      append_zp_operand(os, b1);
      os << ",";
      append_abs_operand(os, target);
      return os.str();
    }

    const BitFamilyMetadata* bitMetadata = find_bit_family_metadata(opcode);
    if (bitMetadata != nullptr) {
      const char* mnemonic = nullptr;
      switch (bitMetadata->kind) {
        case BitFamilyKind::Bit:
          mnemonic = "BIT";
          break;
        case BitFamilyKind::Tsb:
          mnemonic = "TSB";
          break;
        case BitFamilyKind::Trb:
          mnemonic = "TRB";
          break;
      }
      os << mnemonic << " ";
      switch (bitMetadata->mode) {
        case BitFamilyModeMeta::Immediate:
          os << "#$";
          write_hex(os, b1, 2);
          break;
        case BitFamilyModeMeta::Zp:
          append_zp_operand(os, b1);
          break;
        case BitFamilyModeMeta::Abs:
          append_abs_operand(os, absValue);
          break;
        case BitFamilyModeMeta::Zpx:
          append_zp_operand(os, b1, ",X");
          break;
        case BitFamilyModeMeta::Absx:
          append_abs_operand(os, absValue, ",X");
          break;
      }
      return os.str();
    }

    const char* branchMnemonic = control_flow_branch_mnemonic(opcode);
    if (branchMnemonic != nullptr) {
      const uint16_t target = static_cast<uint16_t>(pc + 2 + static_cast<int8_t>(b1));
      os << branchMnemonic << " ";
      append_abs_operand(os, target);
      return os.str();
    }

    const uint8_t mode  = static_cast<uint8_t>(opcode & 0x1F);
    const uint8_t group = static_cast<uint8_t>(opcode & 0xE0);

    const char*               aluName      = alu_mnemonic(group);
    const AluModeMetadata*    aluMode      = find_alu_mode_metadata(mode);
    const RmwModeMetadata*    rmwMode      = find_rmw_mode_metadata(mode);
    const RmwGroupMetadata*   rmwGroup     = find_rmw_group_metadata(group);
    const NopVariantMetadata* nopVariant   = find_nop_variant_metadata(opcode);
    const char*               rmwGroupName = alu_group_mnemonic(group);

    if (aluName != nullptr && aluMode != nullptr) {
      os << aluName << " ";
      switch (aluMode->operandMode) {
        case AluOperandMode::Immediate:
          os << "#$";
          write_hex(os, b1, 2);
          break;
        case AluOperandMode::Zp:
          append_zp_operand(os, b1);
          break;
        case AluOperandMode::ZpX:
          append_zp_operand(os, b1, ",X");
          break;
        case AluOperandMode::Abs:
          append_abs_operand(os, absValue);
          break;
        case AluOperandMode::AbsX:
          append_abs_operand(os, absValue, ",X");
          break;
        case AluOperandMode::AbsY:
          append_abs_operand(os, absValue, ",Y");
          break;
        case AluOperandMode::IndX:
          os << "($";
          write_hex(os, b1, 2);
          os << ",X)";
          append_symbol_suffix_for_address(os, b1);
          break;
        case AluOperandMode::IndY:
          os << "($";
          write_hex(os, b1, 2);
          os << "),Y";
          append_symbol_suffix_for_address(os, b1);
          break;
        case AluOperandMode::ZpInd:
          os << "($";
          write_hex(os, b1, 2);
          os << ")";
          append_symbol_suffix_for_address(os, b1);
          break;
      }
      return os.str();
    }

    if (rmwMode != nullptr && rmwGroup != nullptr && rmwGroupName != nullptr) {
      os << rmwGroupName << " ";
      switch (rmwMode->targetMode) {
        case RmwTargetMode::Zp:
          append_zp_operand(os, b1);
          break;
        case RmwTargetMode::ZpX:
          append_zp_operand(os, b1, ",X");
          break;
        case RmwTargetMode::Abs:
          append_abs_operand(os, absValue);
          break;
        case RmwTargetMode::AbsX:
          append_abs_operand(os, absValue, ",X");
          break;
      }
      return os.str();
    }

    if (nopVariant != nullptr) {
      os << "NOP";
      switch (nopVariant->mode) {
        case NopVariantMode::Implied:
          break;
        case NopVariantMode::ImmediateDiscard:
          os << " #$";
          write_hex(os, b1, 2);
          break;
        case NopVariantMode::ZpRead:
          os << " ";
          append_zp_operand(os, b1);
          break;
        case NopVariantMode::ZpXRead:
          os << " ";
          append_zp_operand(os, b1, ",X");
          break;
        case NopVariantMode::AbsRead:
          os << " ";
          append_abs_operand(os, absValue);
          break;
      }
      return os.str();
    }

    os << "???";
    return os.str();
  }

  static void append_disassembly_register_snapshot(std::ostream& os, const char* label,
                                                   const CPU65C02Regs& regs) {
    os << label << " PC=$";
    write_hex(os, regs.pc, 4);
    os << " A=$";
    write_hex(os, regs.a, 2);
    os << " X=$";
    write_hex(os, regs.x, 2);
    os << " Y=$";
    write_hex(os, regs.y, 2);
    os << " SP=$";
    write_hex(os, regs.sp, 2);
    os << " P=$";
    write_hex(os, regs.p, 2);
  }

  static void append_pc_symbol_suffix_for_address(std::ostream& os, uint16_t addr) {
    const MonitorSymbol* symbol = find_monitor_symbol(addr, MonitorSymbolPc);
    if (symbol == nullptr) {
      return;
    }
    os << " (" << symbol->name << ")";
  }

  static void emit_disassembly_trace_line(std::ostream& os, uint64_t instructionCount, uint16_t pc,
                                          uint8_t opcode, const std::string& disassemblyText,
                                          const CPU65C02Regs& preRegs,
                                          const CPU65C02Regs& postRegs) {
    os << "@" << instructionCount << " PC=$";
    write_hex(os, pc, 4);
    append_pc_symbol_suffix_for_address(os, pc);
    os << " OP=$";
    write_hex(os, opcode, 2);
    os << " ";
    os << disassemblyText;
    os << " ; ";
    append_disassembly_register_snapshot(os, "PRE", preRegs);
    os << " ";
    append_disassembly_register_snapshot(os, "POST", postRegs);
    os << "\n";
  }

  static bool is_disassembly_stack_push_opcode(uint8_t opcode) {
    switch (opcode) {
      case 0x20:  // JSR
      case 0x00:  // BRK
      case 0x48:  // PHA
      case 0x08:  // PHP
      case 0xDA:  // PHX
      case 0x5A:  // PHY
        return true;
      default:
        return false;
    }
  }

  static bool is_disassembly_stack_pop_opcode(uint8_t opcode) {
    switch (opcode) {
      case 0x60:  // RTS
      case 0x40:  // RTI
      case 0x68:  // PLA
      case 0x28:  // PLP
      case 0xFA:  // PLX
      case 0x7A:  // PLY
        return true;
      default:
        return false;
    }
  }

  static void emit_disassembly_stack_dump_line(std::ostream& os, uint64_t instructionCount,
                                               const char* phase, uint8_t opcode, uint16_t pc,
                                               const ConstMemoryBanks& banks, uint8_t sp) {
    const uint16_t stackTop = 0x01FF;
    const uint16_t stackPtr = static_cast<uint16_t>(0x0100 + sp);

    os << "  STACK META[INSN=" << instructionCount << " PHASE=" << phase << " OP=$";
    write_hex(os, opcode, 2);
    os << " PC=$";
    write_hex(os, pc, 4);
    os << " SP=$";
    write_hex(os, sp, 2);
    os << "] SP=$";
    write_hex(os, sp, 2);

    if (stackPtr >= stackTop) {
      os << " EMPTY\n";
      return;
    }

    os << " USED=" << (stackTop - stackPtr) << ":";
    for (uint16_t addr = static_cast<uint16_t>(stackPtr + 1); addr <= stackTop; ++addr) {
      os << " $";
      write_hex(os, addr, 4);
      os << "=$";
      write_hex(os, read_u8(banks, addr), 2);
    }
    os << "\n";
  }

  uint32_t CPU65C02::step() {
    if (m_stopped) {
      return 0;
    }
    if (m_waiting) {
      return 0;
    }

    m_instructionCount++;

    bool track_trace = (m_traceLog != nullptr);
    if (track_trace) {
      log_step_trace_marker(m_r.pc);
    }

    const CPU65C02Regs preRegs       = m_r;
    const uint16_t     instructionPC = m_r.pc;
    const uint8_t      op            = fetch8();

    std::string disassemblyText;
    if (m_disassemblyTraceLog != nullptr) {
      disassemblyText = disassembly_text_for_opcode(m_mem.constBanks(), instructionPC, op);
    }

    if (track_trace) {
      begin_step_zp_monitor_capture();
    }

    uint32_t cycles = execute(op);

    if (m_disassemblyTraceLog != nullptr) {
      if (is_disassembly_stack_pop_opcode(op)) {
        emit_disassembly_stack_dump_line(*m_disassemblyTraceLog, m_instructionCount, "PRE", op,
                                         instructionPC, m_mem.constBanks(), preRegs.sp);
      }
      emit_disassembly_trace_line(*m_disassemblyTraceLog, m_instructionCount, instructionPC, op,
                                  disassemblyText, preRegs, m_r);
      if (is_disassembly_stack_push_opcode(op) && m_r.sp != preRegs.sp) {
        emit_disassembly_stack_dump_line(*m_disassemblyTraceLog, m_instructionCount, "POST", op,
                                         instructionPC, m_mem.constBanks(), m_r.sp);
      }
    }

    if (track_trace) {
      log_step_zp_monitor_events(op);
      end_step_zp_monitor_capture();
    }

    return cycles;
  }

}  // namespace prodos8emu
