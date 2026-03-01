#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "prodos8emu/apple2mem.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"

using namespace prodos8emu;

int main() {
  // Create test directory with files
  std::string testDir = "/tmp/prodos8emu_dirtest";
  std::filesystem::remove_all(testDir);
  std::filesystem::create_directories(testDir + "/volumes/TESTDIR");

  // Create test files
  std::ofstream f1(testDir + "/volumes/TESTDIR/FILE1.TXT");
  f1 << "Test file 1";
  f1.close();

  std::ofstream f2(testDir + "/volumes/TESTDIR/FILE2.TXT");
  f2 << "Test file 2";
  f2.close();

  std::ofstream f3(testDir + "/volumes/TESTDIR/FILE3.TXT");
  f3 << "Test file 3";
  f3.close();

  std::cout << "Created test directory: " << testDir << std::endl;

  // Setup memory and MLI
  Apple2Memory mem;
  MLIContext   mli(testDir + "/volumes");

  // Open the directory
  uint16_t openParamAddr = 0x2000;
  uint16_t pathnameAddr  = 0x2100;
  uint16_t ioBufferAddr  = 0x3000;

  std::string pathname = "/TESTDIR";
  write_u8(mem.banks(), pathnameAddr, pathname.length());
  for (size_t i = 0; i < pathname.length(); i++) {
    write_u8(mem.banks(), pathnameAddr + 1 + i, pathname[i]);
  }

  write_u8(mem.banks(), openParamAddr, 3);
  write_u16_le(mem.banks(), openParamAddr + 1, pathnameAddr);
  write_u16_le(mem.banks(), openParamAddr + 3, ioBufferAddr);

  uint8_t result = mli.openCall(mem.banks(), openParamAddr);
  if (result != 0) {
    std::cout << "OPEN failed: " << (int)result << std::endl;
    return 1;
  }

  uint8_t refNum = read_u8(mem.constBanks(), openParamAddr + 5);
  std::cout << "Opened directory with refnum: " << (int)refNum << std::endl;

  // Read from directory (this reads ProDOS directory blocks)
  uint16_t readParamAddr  = 0x2200;
  uint16_t readBufferAddr = 0x4000;

  write_u8(mem.banks(), readParamAddr, 4);
  write_u8(mem.banks(), readParamAddr + 1, refNum);
  write_u16_le(mem.banks(), readParamAddr + 2, readBufferAddr);
  write_u16_le(mem.banks(), readParamAddr + 4, 512);  // Read 512 bytes (one block)
  write_u16_le(mem.banks(), readParamAddr + 6, 0);

  result              = mli.readCall(mem.banks(), readParamAddr);
  uint16_t transCount = read_u16_le(mem.constBanks(), readParamAddr + 6);

  std::cout << "READ result: " << (int)result << ", trans_count: " << transCount << std::endl;

  if (result == 0 && transCount == 512) {
    std::cout << "\nDirectory block content (first 100 bytes):" << std::endl;
    for (int i = 0; i < 100; i++) {
      if (i % 16 == 0)
        std::cout << "\n" << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
      uint8_t byte = read_u8(mem.constBanks(), readBufferAddr + i);
      std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    }
    std::cout << std::dec << std::endl;

    // Parse directory entries manually
    std::cout << "\nDirectory entries:" << std::endl;
    for (int entryIdx = 0; entryIdx < 13; entryIdx++) {
      uint16_t entryOffset = 4 + (entryIdx * 39);
      uint8_t  byte0       = read_u8(mem.constBanks(), readBufferAddr + entryOffset);
      uint8_t  nameLen     = byte0 & 0x0F;
      uint8_t  storageType = (byte0 >> 4) & 0x0F;

      if (nameLen > 0 && nameLen <= 15) {
        std::string name;
        for (int i = 0; i < nameLen; i++) {
          uint8_t ch = read_u8(mem.constBanks(), readBufferAddr + entryOffset + 1 + i);
          name.push_back(ch & 0x7F);
        }
        std::cout << "  Entry " << entryIdx << ": storage=" << (int)storageType << " name='" << name
                  << "'" << std::endl;
      }
    }
  }

  // Close
  uint16_t closeParamAddr = 0x2300;
  write_u8(mem.banks(), closeParamAddr, 1);
  write_u8(mem.banks(), closeParamAddr + 1, refNum);
  mli.closeCall(mem.constBanks(), closeParamAddr);

  std::cout << "\nClosed directory." << std::endl;

  return 0;
}
