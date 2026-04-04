#include "monitor_symbols.hpp"

namespace prodos8emu {
  namespace {
    static const MonitorSymbol kMonitorSymbols[] = {
        {0x0000, "LOC0"},                                             //
        {0x0000, "Z00"},                                              //
        {0x0001, "LOC1"},                                             //
        {0x0004, "Z04"},                                              //
        {0x0008, "Reg4"},                                             //
        {0x000A, "LoMem"},                                            //
        {0x000A, "TxtBgn"},                                           //
        {0x000C, "HiMem"},                                            //
        {0x000E, "TxtEnd"},                                           //
        {0x0010, "Z10"},                                              //
        {0x0012, "Z12"},                                              //
        {0x0016, "Reg11"},                                            //
        {0x0018, "Reg12"},                                            //
        {0x001C, "Reg14"},                                            //
        {0x001E, "Reg15"},                                            //
        {0x0020, "WNDLWFT"},                                          //
        {0x0021, "WNDWDTH"},                                          //
        {0x0022, "WNDTOP"},                                           //
        {0x0023, "WNDBTM"},                                           //
        {0x0024, "CH"},                                               //
        {0x0025, "SV"},                                               //
        {0x0028, "BASL"},                                             //
        {0x0029, "BASH"},                                             //
        {0x002A, "BAS2L"},                                            //
        {0x002B, "BAS2H"},                                            //
        {0x0032, "INVFLG"},                                           //
        {0x0033, "Prompt"},                                           //
        {0x0035, "YSAV1"},                                            //
        {0x0036, "CSWL"},                                             //
        {0x0038, "KSWL"},                                             //
        {0x003C, "A1"},                                               //
        {0x003C, "A1L"},                                              //
        {0x003D, "A1H"},                                              //
        {0x003E, "A2"},                                               //
        {0x003E, "A2L"},                                              //
        {0x003F, "A2H"},                                              //
        {0x0042, "A4"},                                               //
        {0x0042, "A4L"},                                              //
        {0x0043, "A4H"},                                              //
        {0x0048, "STATUS"},                                           //
        {0x0050, "VideoSlt"},                                         //
        {0x0051, "FileType"},                                         //
        {0x0053, "ExecMode"},                                         //
        {0x0054, "PtrMode"},                                          //
        {0x0058, "Z58"},                                              //
        {0x0059, "Z59"},                                              //
        {0x005A, "Z5A"},                                              //
        {0x005B, "Z5B"},                                              //
        {0x005C, "Z5C"},                                              //
        {0x005F, "TabChar"},                                          //
        {0x0060, "BCDNbr"},                                           //
        {0x0060, "Z60"},                                              //
        {0x0061, "PrColumn"},                                         //
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
        {0x7D07, "ParseDCS", MonitorSymbolPc},                        //
        {0x7D28, "doRTS4", MonitorSymbolPc},                          //
        {0x7D29, "PrtSetup", MonitorSymbolPc},                        //
        {0x7D2E, "IsFileLst", MonitorSymbolPc},                       //
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
        {0x9900, "L9900", MonitorSymbolPc},                           //
        {0x9912, "L9912", MonitorSymbolPc},                           //
        {0x991A, "L991A", MonitorSymbolPc},                           //
        {0x9923, "L9923", MonitorSymbolPc},                           //
        {0x9948, "L9948", MonitorSymbolPc},                           //
        {0x9957, "L9957", MonitorSymbolPc},                           //
        {0x9958, "L9958", MonitorSymbolPc},                           //
        {0x9962, "L9962", MonitorSymbolPc},                           //
        {0x996C, "L996C"},                                            //
        {0x996D, "L996D", MonitorSymbolPc},                           //
        {0x9975, "L9975", MonitorSymbolPc},                           //
        {0x9979, "L9979", MonitorSymbolPc},                           //
        {0x997B, "L997B", MonitorSymbolPc},                           //
        {0x997E, "L997E", MonitorSymbolPc},                           //
        {0x9981, "L9981", MonitorSymbolPc},                           //
        {0x9982, "L9982", MonitorSymbolPc},                           //
        {0x9989, "L9989", MonitorSymbolPc},                           //
        {0x998C, "L998C", MonitorSymbolPc},                           //
        {0x9990, "L9990", MonitorSymbolPc},                           //
        {0x9997, "L9997", MonitorSymbolPc},                           //
        {0x99A0, "L99A0", MonitorSymbolPc},                           //
        {0x99A2, "L99A2", MonitorSymbolPc},                           //
        {0x99AA, "L99AA", MonitorSymbolPc},                           //
        {0x99B1, "ClsFile", MonitorSymbolPc},                         //
        {0x99BD, "L99BD", MonitorSymbolPc},                           //
        {0x99BE, "L99BE", MonitorSymbolPc},                           //
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
        {0xB67A, "LB67A", MonitorSymbolPc},                           //
        {0xB682, "LB682", MonitorSymbolPc},                           //
        {0xB697, "LB697", MonitorSymbolPc},                           //
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
        {0xD000, "NEWSW16", MonitorSymbolPc},                         //
        {0xD007, "ChkPrtCols", MonitorSymbolPc},                      //
        {0xD00F, "UsePrtr", MonitorSymbolPc},                         //
        {0xD011, "LD011", MonitorSymbolPc},                           //
        {0xD015, "SetPtrCols", MonitorSymbolPc},                      //
        {0xD017, "LD017", MonitorSymbolPc},                           //
        {0xD01A, "ChkLoop", MonitorSymbolPc},                         //
        {0xD025, "LD025", MonitorSymbolPc},                           //
        {0xD037, "ToBR", MonitorSymbolPc},                            //
        {0xD056, "LD056", MonitorSymbolPc},                           //
        {0xD05C, "LD05C", MonitorSymbolPc},                           //
        {0xD05F, "LD05F"},                                            //
        {0xD06A, "LD06A", MonitorSymbolPc},                           //
        {0xD076, "LD076", MonitorSymbolPc},                           //
        {0xD078, "LD078", MonitorSymbolPc},                           //
        {0xD07F, "LD07F"},                                            //
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
        {0xD222, "LD222", MonitorSymbolPc},                           //
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
        {0xDE92, "IRQV"},                                             //
        {0xDE93, "IRQV"},                                             //
        {0xDE94, "IRQV"},                                             //
        {0xDE95, "IRQV"},                                             //
        {0xDE96, "IRQV"},                                             //
        {0xDE97, "IRQV"},                                             //
        {0xDE98, "IRQV"},                                             //
        {0xDE99, "IRQV"},                                             //
        {0xDE9A, "IRQV"},                                             //
        {0xDE9B, "IRQV"},                                             //
        {0xDE9C, "IRQV"},                                             //
        {0xDE9D, "IRQV"},                                             //
        {0xDE9E, "IRQV"},                                             //
        {0xDE9F, "IRQV"},                                             //
        {0xDEA0, "IRQV"},                                             //
        {0xDEA1, "IRQV"},                                             //
        {0xDEA2, "IRQV"},                                             //
        {0xDEA3, "IRQV"},                                             //
        {0xDEA4, "IRQV"},                                             //
        {0xDEA5, "IRQV"},                                             //
        {0xDEA6, "IRQV"},                                             //
        {0xDEA7, "IRQV"},                                             //
        {0xDEA8, "IRQV"},                                             //
        {0xDEA9, "IRQV"},                                             //
        {0xDEAA, "IRQV"},                                             //
        {0xDEAB, "IRQV"},                                             //
        {0xDEAC, "IRQV"},                                             //
        {0xDEAD, "IRQV"},                                             //
        {0xDEAE, "IRQV"},                                             //
        {0xDEAF, "IRQV"},                                             //
        {0xDEB0, "IRQV"},                                             //
        {0xDEB1, "IRQV"},                                             //
        {0xDEB2, "IRQV"},                                             //
        {0xDEB3, "IRQV"},                                             //
        {0xDEB4, "IRQV"},                                             //
        {0xDEB5, "IRQV"},                                             //
        {0xDEB6, "IRQV"},                                             //
        {0xDEB7, "IRQV"},                                             //
        {0xDEB8, "IRQV"},                                             //
        {0xDEB9, "IRQV"},                                             //
        {0xDEBA, "IRQV"},                                             //
        {0xDEBB, "IRQV"},                                             //
        {0xDEBC, "IRQV"},                                             //
        {0xDEBD, "IRQV"},                                             //
        {0xDEBE, "IRQV"},                                             //
        {0xDEBF, "IRQV"},                                             //
        {0xDEC0, "IRQV"},                                             //
        {0xDEC1, "IRQV"},                                             //
        {0xDEC2, "IRQV"},                                             //
        {0xDEC3, "IRQV"},                                             //
        {0xDEC4, "IRQV"},                                             //
        {0xDEC5, "IRQV"},                                             //
        {0xDEC6, "IRQV"},                                             //
        {0xDEC7, "IRQV"},                                             //
        {0xDEC8, "IRQV"},                                             //
        {0xDEC9, "IRQV"},                                             //
        {0xDECA, "IRQV"},                                             //
        {0xDECB, "IRQV"},                                             //
        {0xDECC, "IRQV"},                                             //
        {0xDECD, "IRQV"},                                             //
        {0xDECE, "IRQV"},                                             //
        {0xDECF, "IRQV"},                                             //
        {0xDED0, "IRQV"},                                             //
        {0xDED1, "IRQV"},                                             //
        {0xDED2, "IRQV"},                                             //
        {0xDED3, "IRQV"},                                             //
        {0xDED4, "IRQV"},                                             //
        {0xDED5, "IRQV"},                                             //
        {0xDED6, "IRQV"},                                             //
        {0xDED7, "IRQV"},                                             //
        {0xDED8, "IRQV"},                                             //
        {0xDED9, "IRQV"},                                             //
        {0xDEDA, "IRQV"},                                             //
        {0xDEDB, "IRQV"},                                             //
        {0xDEDC, "IRQV"},                                             //
        {0xDEDD, "IRQV"},                                             //
        {0xDEDE, "IRQV"},                                             //
        {0xDEDF, "IRQV"},                                             //
        {0xDEE0, "IRQV"},                                             //
        {0xDEE1, "IRQV"},                                             //
        {0xDEE2, "IRQV"},                                             //
        {0xDEE3, "IRQV"},                                             //
        {0xDEE4, "IRQV"},                                             //
        {0xDEE5, "IRQV"},                                             //
        {0xDEE6, "IRQV"},                                             //
        {0xDEE7, "IRQV"},                                             //
        {0xDEE8, "IRQV"},                                             //
        {0xDEE9, "IRQV"},                                             //
        {0xDEEA, "IRQV"},                                             //
        {0xDEEB, "IRQV"},                                             //
        {0xDEEC, "IRQV"},                                             //
        {0xDEED, "IRQV"},                                             //
        {0xDEEE, "IRQV"},                                             //
        {0xDEEF, "IRQV"},                                             //
        {0xDEF0, "IRQV"},                                             //
        {0xDEF1, "IRQV"},                                             //
        {0xDEF2, "IRQV"},                                             //
        {0xDEF3, "IRQV"},                                             //
        {0xDEF4, "IRQV"},                                             //
        {0xDF09, "Tbl1stLet"},                                        //
        {0xDF4E, "SvZPArea"},                                         //
        {0xDFA2, "XDFA2"},                                            //
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
  }  // namespace

  std::span<const MonitorSymbol> get_monitor_symbols() {
    return kMonitorSymbols;
  }

}  // namespace prodos8emu
