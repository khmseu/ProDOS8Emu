#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

#include "prodos8emu/apple2mem.hpp"
#include "prodos8emu/cpu65c02.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/mli.hpp"
#include "prodos8emu/system_loader.hpp"

namespace fs = std::filesystem;

/**
 * Test the complete emulator startup pipeline:
 * 1. Load ROM
 * 2. Load system file at $2000
 * 3. Initialize warm restart vector
 * 4. Override reset vector to point to $2000
 * 5. Reset and run CPU
 * 6. Verify CPU executes and stops as expected
 */

static void createTestROM(const fs::path& romPath) {
  // Create a 12KB ROM file filled with deterministic pattern
  std::ofstream rom(romPath, std::ios::binary);
  if (!rom.is_open()) {
    throw std::runtime_error("Failed to create test ROM file");
  }

  // Write 12KB of deterministic content (0xD000-0xFFFF)
  // Fill with pattern that won't execute as valid code
  for (int i = 0; i < 12288; i++) {
    uint8_t byte = static_cast<uint8_t>(i & 0xFF);
    rom.write(reinterpret_cast<const char*>(&byte), 1);
  }

  if (!rom.good()) {
    throw std::runtime_error("Failed to write test ROM file");
  }
}

static void createTestSystemFile(const fs::path& sysPath) {
  // Create a minimal system file:
  //   $2000: JMP $2003  (0x4C 0x03 0x20)
  //   $2003: STP        (0xDB)
  std::ofstream sys(sysPath, std::ios::binary);
  if (!sys.is_open()) {
    throw std::runtime_error("Failed to create test system file");
  }

  uint8_t program[] = {
      0x4C, 0x03, 0x20,  // JMP $2003
      0xDB               // STP
  };

  sys.write(reinterpret_cast<const char*>(program), sizeof(program));

  if (!sys.good()) {
    throw std::runtime_error("Failed to write test system file");
  }
}

int main() {
  int failures = 0;

  // Create unique temporary directory for test files using PID + timestamp
  pid_t pid       = getpid();
  auto  timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

  std::string tempDirName =
      "prodos8emu_emulator_startup_test_" + std::to_string(pid) + "_" + std::to_string(timestamp);

  std::cout << "Test 1: Complete emulator startup pipeline\n";

  fs::path tempDir;
  bool     setupSucceeded = false;

  try {
    // Filesystem setup - use non-throwing APIs with error_code
    std::error_code ec;

    // Get temp directory path with error handling (avoid potentially-throwing call before try)
    fs::path baseTempDir = fs::temp_directory_path(ec);
    if (ec) {
      std::cerr << "FAIL: Failed to get temp directory path: " << ec.message() << "\n";
      failures++;
    } else {
      tempDir = baseTempDir / tempDirName;
      std::cout << "  Using temp directory: " << tempDir << "\n";

      // Remove any existing temp directory
      fs::remove_all(tempDir, ec);
      if (ec && ec != std::errc::no_such_file_or_directory) {
        std::cerr << "FAIL: Failed to remove existing temp directory: " << ec.message() << "\n";
        failures++;
      } else {
        // Create temp directory
        fs::create_directories(tempDir, ec);
        if (ec) {
          std::cerr << "FAIL: Failed to create temp directory: " << ec.message() << "\n";
          failures++;
        } else {
          setupSucceeded = true;
        }
      }
    }

    // Only run tests if filesystem setup succeeded
    if (setupSucceeded) {
      fs::path romPath = tempDir / "test.rom";
      fs::path sysPath = tempDir / "test.system";

      // Create test files
      createTestROM(romPath);
      createTestSystemFile(sysPath);

      // Initialize emulator components
      prodos8emu::Apple2Memory mem;
      prodos8emu::MLIContext   ctx(tempDir);
      prodos8emu::CPU65C02     cpu(mem);
      cpu.attachMLI(ctx);

      // Step 1: Load ROM
      mem.loadROM(romPath);
      std::cout << "  Loaded ROM\n";

      // Step 2: Load system file into $2000
      prodos8emu::loadSystemFile(mem, sysPath, 0x2000);
      std::cout << "  Loaded system file at $2000\n";

      // Step 3: Initialize warm restart vector
      prodos8emu::initWarmStartVector(mem, 0x2000);
      std::cout << "  Initialized warm start vector\n";

      // Verify warm start vector is set correctly
      uint16_t warmAddr        = prodos8emu::read_u16_le(mem.constBanks(), 0x03F2);
      uint8_t  warmPowerUpByte = prodos8emu::read_u8(mem.constBanks(), 0x03F4);

      if (warmAddr != 0x2000) {
        std::cerr << "FAIL: Warm start vector at $03F2 should be $2000, got $" << std::hex
                  << warmAddr << std::dec << "\n";
        failures++;
      }

      if (warmPowerUpByte != 0xA5) {
        std::cerr << "FAIL: Warm start power-up byte at $03F4 should be $A5, got $" << std::hex
                  << (int)warmPowerUpByte << std::dec << "\n";
        failures++;
      }

      // Step 4: Override reset vector to point to $2000
      // Enable LC read/write to modify the reset vector area
      mem.setLCReadEnabled(true);
      mem.setLCWriteEnabled(true);

      // Write $2000 to reset vector at $FFFC/$FFFD
      prodos8emu::write_u16_le(mem.banks(), 0xFFFC, 0x2000);
      std::cout << "  Set reset vector to $2000\n";

      // Verify reset vector
      uint16_t resetVec = prodos8emu::read_u16_le(mem.constBanks(), 0xFFFC);
      if (resetVec != 0x2000) {
        std::cerr << "FAIL: Reset vector at $FFFC should be $2000, got $" << std::hex << resetVec
                  << std::dec << "\n";
        failures++;
      }

      // Step 5: Reset CPU (loads PC from reset vector)
      cpu.reset();
      std::cout << "  CPU reset\n";

      // Verify PC is at $2000
      if (cpu.regs().pc != 0x2000) {
        std::cerr << "FAIL: PC after reset should be $2000, got $" << std::hex << cpu.regs().pc
                  << std::dec << "\n";
        failures++;
      }

      // Restore LC state to ROM mode for execution
      // (system code would typically run with LC disabled initially)
      mem.setLCReadEnabled(false);
      mem.setLCWriteEnabled(false);
      std::cout << "  Restored LC to ROM mode\n";

      // Step 6: Run CPU with bounded instruction limit
      // Expected: JMP $2003 (1 instruction) + STP (1 instruction) = 2 total
      uint64_t instructionCount = cpu.run(100);
      std::cout << "  Executed " << instructionCount << " instructions\n";

      // Verify CPU stopped
      if (!cpu.isStopped()) {
        std::cerr << "FAIL: CPU should be stopped after executing STP\n";
        failures++;
      } else {
        std::cout << "  CPU stopped as expected\n";
      }

      // Verify instruction count (should be 2: JMP + STP)
      if (instructionCount != 2) {
        std::cerr << "FAIL: Expected 2 instructions (JMP + STP), executed " << instructionCount
                  << "\n";
        failures++;
      } else {
        std::cout << "  Instruction count correct (2)\n";
      }

      // Verify PC is at $2004 (after STP at $2003)
      if (cpu.regs().pc != 0x2004) {
        std::cerr << "FAIL: PC after STP should be $2004, got $" << std::hex << cpu.regs().pc
                  << std::dec << "\n";
        failures++;
      } else {
        std::cout << "  PC correct ($2004)\n";
      }

    }  // end if (setupSucceeded)

  } catch (const std::exception& e) {
    std::cerr << "FAIL: Exception: " << e.what() << "\n";
    failures++;
  }

  // Cleanup - use non-throwing API (only if tempDir was set)
  if (!tempDir.empty()) {
    std::error_code cleanup_ec;
    fs::remove_all(tempDir, cleanup_ec);
    if (cleanup_ec) {
      std::cerr << "Warning: Failed to cleanup temp directory: " << cleanup_ec.message() << "\n";
    }
  }

  if (failures == 0) {
    std::cout << "\nAll tests passed.\n";
    return 0;
  } else {
    std::cerr << "\n" << failures << " test(s) failed.\n";
    return 1;
  }
}
