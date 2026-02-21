#include "prodos8emu/apple2mem.hpp"

#include <iostream>

#include "prodos8emu/memory.hpp"

int main() {
  int failures = 0;

  // Test 1: Construction zeroes all memory
  {
    std::cout << "Test 1: Construction zeroes all memory\n";
    prodos8emu::Apple2Memory mem;

    bool allZero = true;
    for (uint16_t addr = 0; addr <= 0xFFFE; addr++) {
      if (prodos8emu::read_u8(mem.constBanks(), addr) != 0) {
        allZero = false;
        break;
      }
    }
    // Also check 0xFFFF
    if (allZero && prodos8emu::read_u8(mem.constBanks(), 0xFFFF) != 0) {
      allZero = false;
    }

    if (!allZero) {
      std::cerr << "FAIL: Memory not zeroed on construction\n";
      failures++;
    } else {
      std::cout << "PASS: Construction zeroes all memory\n";
    }
  }

  // Test 2: Initial LC state is disabled, bank 1 selected
  {
    std::cout << "Test 2: Initial LC state\n";
    prodos8emu::Apple2Memory mem;

    bool ok = !mem.isLCReadEnabled() && !mem.isLCWriteEnabled() && mem.isLCBank1();
    if (!ok) {
      std::cerr << "FAIL: Initial LC state incorrect\n";
      failures++;
    } else {
      std::cout << "PASS: Initial LC state\n";
    }
  }

  // Test 3: Main RAM is always accessible (banks 0-12)
  {
    std::cout << "Test 3: Main RAM always accessible\n";
    prodos8emu::Apple2Memory mem;

    prodos8emu::write_u8(mem.banks(), 0x0000, 0xAA);
    prodos8emu::write_u8(mem.banks(), 0x0800, 0xBB);
    prodos8emu::write_u8(mem.banks(), 0xBFFF, 0xCC);
    prodos8emu::write_u8(mem.banks(), 0xC000, 0xDD);

    bool ok = prodos8emu::read_u8(mem.constBanks(), 0x0000) == 0xAA &&
              prodos8emu::read_u8(mem.constBanks(), 0x0800) == 0xBB &&
              prodos8emu::read_u8(mem.constBanks(), 0xBFFF) == 0xCC &&
              prodos8emu::read_u8(mem.constBanks(), 0xC000) == 0xDD;

    if (!ok) {
      std::cerr << "FAIL: Main RAM not accessible\n";
      failures++;
    } else {
      std::cout << "PASS: Main RAM always accessible\n";
    }
  }

  // Test 4: LC disabled - reads from $D000-$FFFF return zero (ROM area)
  {
    std::cout << "Test 4: LC disabled - ROM area reads zero\n";
    prodos8emu::Apple2Memory mem;
    // LC read disabled by default

    bool allZero = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0 &&
                   prodos8emu::read_u8(mem.constBanks(), 0xDFFF) == 0 &&
                   prodos8emu::read_u8(mem.constBanks(), 0xE000) == 0 &&
                   prodos8emu::read_u8(mem.constBanks(), 0xFFFF) == 0;

    if (!allZero) {
      std::cerr << "FAIL: ROM area not zero when LC disabled\n";
      failures++;
    } else {
      std::cout << "PASS: LC disabled - ROM area reads zero\n";
    }
  }

  // Test 5: LC bank 1 read/write - enable LC, write bank 1, read back
  {
    std::cout << "Test 5: LC bank 1 read/write\n";
    prodos8emu::Apple2Memory mem;

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);
    // Bank 1 selected by default

    prodos8emu::write_u8(mem.banks(), 0xD000, 0x11);
    prodos8emu::write_u8(mem.banks(), 0xDFFF, 0x22);
    prodos8emu::write_u8(mem.banks(), 0xE000, 0x33);
    prodos8emu::write_u8(mem.banks(), 0xFFFF, 0x44);

    bool ok = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0x11 &&
              prodos8emu::read_u8(mem.constBanks(), 0xDFFF) == 0x22 &&
              prodos8emu::read_u8(mem.constBanks(), 0xE000) == 0x33 &&
              prodos8emu::read_u8(mem.constBanks(), 0xFFFF) == 0x44;

    if (!ok) {
      std::cerr << "FAIL: LC bank 1 read/write failed\n";
      failures++;
    } else {
      std::cout << "PASS: LC bank 1 read/write\n";
    }
  }

  // Test 6: LC bank 2 is independent of bank 1
  {
    std::cout << "Test 6: LC bank 1 and bank 2 are independent\n";
    prodos8emu::Apple2Memory mem;

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    // Write to bank 1
    mem.setLCBank1(true);
    prodos8emu::write_u8(mem.banks(), 0xD000, 0x11);
    prodos8emu::write_u8(mem.banks(), 0xD100, 0x12);

    // Write different data to bank 2
    mem.setLCBank1(false);
    prodos8emu::write_u8(mem.banks(), 0xD000, 0x21);
    prodos8emu::write_u8(mem.banks(), 0xD100, 0x22);

    // Read bank 2 - should get the second set of values
    bool bank2ok = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0x21 &&
                   prodos8emu::read_u8(mem.constBanks(), 0xD100) == 0x22;

    // Switch back to bank 1 and verify original values
    mem.setLCBank1(true);
    bool bank1ok = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0x11 &&
                   prodos8emu::read_u8(mem.constBanks(), 0xD100) == 0x12;

    if (!bank1ok || !bank2ok) {
      std::cerr << "FAIL: LC banks not independent\n";
      failures++;
    } else {
      std::cout << "PASS: LC bank 1 and bank 2 are independent\n";
    }
  }

  // Test 7: LC high RAM ($E000-$FFFF) is shared across bank 1/2 switches
  {
    std::cout << "Test 7: LC high RAM shared across bank switches\n";
    prodos8emu::Apple2Memory mem;

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);
    mem.setLCBank1(true);

    // Write to LC high RAM in bank 1 context
    prodos8emu::write_u8(mem.banks(), 0xE000, 0x55);
    prodos8emu::write_u8(mem.banks(), 0xFFFF, 0x66);

    // Switch to bank 2 - high RAM should still be visible
    mem.setLCBank1(false);
    bool ok = prodos8emu::read_u8(mem.constBanks(), 0xE000) == 0x55 &&
              prodos8emu::read_u8(mem.constBanks(), 0xFFFF) == 0x66;

    if (!ok) {
      std::cerr << "FAIL: LC high RAM not shared across bank switches\n";
      failures++;
    } else {
      std::cout << "PASS: LC high RAM shared across bank switches\n";
    }
  }

  // Test 8: Switching LC read off hides LC data (ROM area returns zero)
  {
    std::cout << "Test 8: Disabling LC read hides LC data\n";
    prodos8emu::Apple2Memory mem;

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);
    prodos8emu::write_u8(mem.banks(), 0xD000, 0x77);
    prodos8emu::write_u8(mem.banks(), 0xE800, 0x88);

    // Disable LC read - should see ROM area (zero) again
    mem.setLCReadEnabled(false);
    bool ok = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0 &&
              prodos8emu::read_u8(mem.constBanks(), 0xE800) == 0;

    if (!ok) {
      std::cerr << "FAIL: Disabling LC read did not hide LC data\n";
      failures++;
    } else {
      std::cout << "PASS: Disabling LC read hides LC data\n";
    }
  }

  // Test 9: Re-enabling LC read restores LC data
  {
    std::cout << "Test 9: Re-enabling LC read restores LC data\n";
    prodos8emu::Apple2Memory mem;

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);
    prodos8emu::write_u8(mem.banks(), 0xD400, 0x99);

    mem.setLCReadEnabled(false);
    mem.setLCReadEnabled(true);

    bool ok = prodos8emu::read_u8(mem.constBanks(), 0xD400) == 0x99;
    if (!ok) {
      std::cerr << "FAIL: Re-enabling LC read did not restore data\n";
      failures++;
    } else {
      std::cout << "PASS: Re-enabling LC read restores LC data\n";
    }
  }

  // Test 10: reset() zeroes all memory and resets LC state
  {
    std::cout << "Test 10: reset() zeroes memory and LC state\n";
    prodos8emu::Apple2Memory mem;

    // Write data and configure LC
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);
    mem.setLCBank1(false);
    prodos8emu::write_u8(mem.banks(), 0x0100, 0xAB);
    prodos8emu::write_u8(mem.banks(), 0xD000, 0xCD);

    mem.reset();

    // LC should be disabled and bank 1 selected
    bool lcStateOk = !mem.isLCReadEnabled() && !mem.isLCWriteEnabled() && mem.isLCBank1();

    // Main RAM should be zero
    bool mainRamZero = prodos8emu::read_u8(mem.constBanks(), 0x0100) == 0;

    // LC data should be gone (LC is now disabled so we see ROM area = zero)
    bool lcDataGone = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0;

    // Enable LC to verify LC RAM is also zeroed
    mem.setLCReadEnabled(true);
    bool lcRamZero = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0;

    if (!lcStateOk || !mainRamZero || !lcDataGone || !lcRamZero) {
      std::cerr << "FAIL: reset() did not fully zero memory or LC state\n";
      failures++;
    } else {
      std::cout << "PASS: reset() zeroes memory and LC state\n";
    }
  }

  // Test 11: constBanks() reflects the same data as banks()
  {
    std::cout << "Test 11: constBanks() reflects banks()\n";
    prodos8emu::Apple2Memory mem;

    prodos8emu::write_u8(mem.banks(), 0x0200, 0x42);
    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);
    prodos8emu::write_u8(mem.banks(), 0xD000, 0x43);

    bool ok = prodos8emu::read_u8(mem.constBanks(), 0x0200) == 0x42 &&
              prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0x43;

    if (!ok) {
      std::cerr << "FAIL: constBanks() does not reflect banks()\n";
      failures++;
    } else {
      std::cout << "PASS: constBanks() reflects banks()\n";
    }
  }

  // Test 12: Bank switch during LC bank 2 selected - writes go to bank 2
  {
    std::cout << "Test 12: Bank 2 selected - writes target bank 2\n";
    prodos8emu::Apple2Memory mem;

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);

    // Write distinct values to both banks at same address
    mem.setLCBank1(true);
    prodos8emu::write_u8(mem.banks(), 0xD800, 0xB1);
    mem.setLCBank1(false);
    prodos8emu::write_u8(mem.banks(), 0xD800, 0xB2);

    // Verify bank 2 value
    bool bank2ok = prodos8emu::read_u8(mem.constBanks(), 0xD800) == 0xB2;

    // Switch to bank 1 and verify
    mem.setLCBank1(true);
    bool bank1ok = prodos8emu::read_u8(mem.constBanks(), 0xD800) == 0xB1;

    if (!bank1ok || !bank2ok) {
      std::cerr << "FAIL: Bank 2 writes not independent\n";
      failures++;
    } else {
      std::cout << "PASS: Bank 2 selected - writes target bank 2\n";
    }
  }

  // Test 13: applySoftSwitch returns false for non-LC-switch addresses
  {
    std::cout << "Test 13: applySoftSwitch rejects non-switch addresses\n";
    prodos8emu::Apple2Memory mem;

    bool ok = !mem.applySoftSwitch(0xC000, true) && !mem.applySoftSwitch(0xC07F, true) &&
              !mem.applySoftSwitch(0xC090, true) && !mem.applySoftSwitch(0x0000, true);

    if (!ok) {
      std::cerr << "FAIL: applySoftSwitch accepted non-switch address\n";
      failures++;
    } else {
      std::cout << "PASS: applySoftSwitch rejects non-switch addresses\n";
    }
  }

  // Test 14: $C080 read - enable LC bank 2 read, write protect
  {
    std::cout << "Test 14: $C080 read - LC bank 2 read, write protect\n";
    prodos8emu::Apple2Memory mem;

    bool valid = mem.applySoftSwitch(0xC080, true);
    bool ok    = valid && mem.isLCReadEnabled() && !mem.isLCWriteEnabled() && !mem.isLCBank1();

    if (!ok) {
      std::cerr << "FAIL: $C080 did not set expected state\n";
      failures++;
    } else {
      std::cout << "PASS: $C080 read - LC bank 2 read, write protect\n";
    }
  }

  // Test 15: $C082 read - ROM read (LC disabled), write protect
  {
    std::cout << "Test 15: $C082 read - ROM read, write protect\n";
    prodos8emu::Apple2Memory mem;

    mem.applySoftSwitch(0xC082, true);
    bool ok = !mem.isLCReadEnabled() && !mem.isLCWriteEnabled() && !mem.isLCBank1();

    if (!ok) {
      std::cerr << "FAIL: $C082 did not set expected state\n";
      failures++;
    } else {
      std::cout << "PASS: $C082 read - ROM read, write protect\n";
    }
  }

  // Test 16: $C088 read - enable LC bank 1 read, write protect
  {
    std::cout << "Test 16: $C088 read - LC bank 1 read, write protect\n";
    prodos8emu::Apple2Memory mem;

    mem.applySoftSwitch(0xC088, true);
    bool ok = mem.isLCReadEnabled() && !mem.isLCWriteEnabled() && mem.isLCBank1();

    if (!ok) {
      std::cerr << "FAIL: $C088 did not set expected state\n";
      failures++;
    } else {
      std::cout << "PASS: $C088 read - LC bank 1 read, write protect\n";
    }
  }

  // Test 17: $C081 two-read protocol enables write
  {
    std::cout << "Test 17: $C081 two reads enable write\n";
    prodos8emu::Apple2Memory mem;

    // First read: prequalify
    mem.applySoftSwitch(0xC081, true);
    bool afterFirst = !mem.isLCWriteEnabled() && mem.isLCWritePrequalified();

    // Second read: write-enable activated
    mem.applySoftSwitch(0xC081, true);
    bool afterSecond = mem.isLCWriteEnabled() && !mem.isLCWritePrequalified();

    if (!afterFirst || !afterSecond) {
      std::cerr << "FAIL: $C081 two-read write-enable did not work\n";
      failures++;
    } else {
      std::cout << "PASS: $C081 two reads enable write\n";
    }
  }

  // Test 18: Write access clears write-enable pre-qualification
  {
    std::cout << "Test 18: Write to soft switch clears pre-qualification\n";
    prodos8emu::Apple2Memory mem;

    // First read: prequalify
    mem.applySoftSwitch(0xC081, true);
    bool prequalSet = mem.isLCWritePrequalified();

    // Write access: should clear pre-qualification
    mem.applySoftSwitch(0xC081, false);
    bool prequalCleared = !mem.isLCWritePrequalified() && !mem.isLCWriteEnabled();

    if (!prequalSet || !prequalCleared) {
      std::cerr << "FAIL: Write did not clear pre-qualification\n";
      failures++;
    } else {
      std::cout << "PASS: Write to soft switch clears pre-qualification\n";
    }
  }

  // Test 19: $C083 two-read protocol enables LC read+write (bank 2)
  {
    std::cout << "Test 19: $C083 two reads enable LC read+write bank 2\n";
    prodos8emu::Apple2Memory mem;

    mem.applySoftSwitch(0xC083, true);
    mem.applySoftSwitch(0xC083, true);
    bool ok = mem.isLCReadEnabled() && mem.isLCWriteEnabled() && !mem.isLCBank1();

    if (!ok) {
      std::cerr << "FAIL: $C083 two reads did not enable LC read+write\n";
      failures++;
    } else {
      std::cout << "PASS: $C083 two reads enable LC read+write bank 2\n";
    }
  }

  // Test 20: $C08B two-read protocol enables LC read+write (bank 1)
  {
    std::cout << "Test 20: $C08B two reads enable LC read+write bank 1\n";
    prodos8emu::Apple2Memory mem;

    mem.applySoftSwitch(0xC08B, true);
    mem.applySoftSwitch(0xC08B, true);
    bool ok = mem.isLCReadEnabled() && mem.isLCWriteEnabled() && mem.isLCBank1();

    if (!ok) {
      std::cerr << "FAIL: $C08B two reads did not enable LC read+write bank 1\n";
      failures++;
    } else {
      std::cout << "PASS: $C08B two reads enable LC read+write bank 1\n";
    }
  }

  // Test 21: Mirror addresses $C084-$C087 behave like $C080-$C083
  {
    std::cout << "Test 21: Mirror addresses $C084-$C087 work like $C080-$C083\n";
    prodos8emu::Apple2Memory mem;

    // $C084 mirrors $C080: LC bank 2 read, write protect
    mem.applySoftSwitch(0xC084, true);
    bool c084ok = mem.isLCReadEnabled() && !mem.isLCWriteEnabled() && !mem.isLCBank1();

    // $C086 mirrors $C082: ROM read, write protect
    mem.applySoftSwitch(0xC086, true);
    bool c086ok = !mem.isLCReadEnabled() && !mem.isLCWriteEnabled() && !mem.isLCBank1();

    if (!c084ok || !c086ok) {
      std::cerr << "FAIL: Mirror addresses do not work correctly\n";
      failures++;
    } else {
      std::cout << "PASS: Mirror addresses $C084-$C087 work like $C080-$C083\n";
    }
  }

  // Test 22: Non-write-enable read clears pre-qualification
  {
    std::cout << "Test 22: Non-write-enable read clears pre-qualification\n";
    prodos8emu::Apple2Memory mem;

    // Prequalify
    mem.applySoftSwitch(0xC081, true);
    bool prequal = mem.isLCWritePrequalified();

    // Read non-write-enable switch: should clear pre-qual
    mem.applySoftSwitch(0xC080, true);
    bool cleared = !mem.isLCWritePrequalified();

    if (!prequal || !cleared) {
      std::cerr << "FAIL: Non-write-enable read did not clear pre-qualification\n";
      failures++;
    } else {
      std::cout << "PASS: Non-write-enable read clears pre-qualification\n";
    }
  }

  // Test 23: reset() clears write-enable pre-qualification
  {
    std::cout << "Test 23: reset() clears write-enable pre-qualification\n";
    prodos8emu::Apple2Memory mem;

    mem.applySoftSwitch(0xC081, true);  // prequalify
    bool prequal = mem.isLCWritePrequalified();

    mem.reset();
    bool cleared = !mem.isLCWritePrequalified();

    if (!prequal || !cleared) {
      std::cerr << "FAIL: reset() did not clear write-enable pre-qualification\n";
      failures++;
    } else {
      std::cout << "PASS: reset() clears write-enable pre-qualification\n";
    }
  }

  // Test 24: ROMIN2 mode reads ROM but writes LC RAM
  {
    std::cout << "Test 24: ROMIN2 reads ROM, writes LC RAM\n";
    prodos8emu::Apple2Memory mem;

    // ROMIN2: bank 2 selected, ROM reads, write-enable after two reads
    mem.applySoftSwitch(0xC081, true);
    mem.applySoftSwitch(0xC081, true);

    bool stateOk = !mem.isLCReadEnabled() && mem.isLCWriteEnabled() && !mem.isLCBank1();

    // Writes should go to LC RAM even though reads are from ROM
    prodos8emu::write_u8(mem.banks(), 0xD000, 0x5A);
    prodos8emu::write_u8(mem.banks(), 0xE000, 0x6B);
    prodos8emu::write_u8(mem.banks(), 0xFFFF, 0x7C);

    bool readsRomOk = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0 &&
                      prodos8emu::read_u8(mem.constBanks(), 0xE000) == 0 &&
                      prodos8emu::read_u8(mem.constBanks(), 0xFFFF) == 0;

    // Enabling LC read should reveal the RAM that was written in ROMIN2
    mem.setLCReadEnabled(true);
    bool readsRamOk = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0x5A &&
                      prodos8emu::read_u8(mem.constBanks(), 0xE000) == 0x6B &&
                      prodos8emu::read_u8(mem.constBanks(), 0xFFFF) == 0x7C;

    if (!stateOk || !readsRomOk || !readsRamOk) {
      std::cerr << "FAIL: ROMIN2 did not behave as read-ROM/write-RAM\n";
      failures++;
    } else {
      std::cout << "PASS: ROMIN2 reads ROM, writes LC RAM\n";
    }
  }

  // Test 25: RDROM2 mode ignores writes
  {
    std::cout << "Test 25: RDROM2 ignores writes\n";
    prodos8emu::Apple2Memory mem;

    // RDROM2: bank 2 selected, ROM reads, write protected
    mem.applySoftSwitch(0xC082, true);
    bool stateOk = !mem.isLCReadEnabled() && !mem.isLCWriteEnabled() && !mem.isLCBank1();

    // Writes should be ignored (must not modify LC RAM)
    prodos8emu::write_u8(mem.banks(), 0xD000, 0xAA);
    prodos8emu::write_u8(mem.banks(), 0xE000, 0xBB);

    bool readsRomOk = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0 &&
                      prodos8emu::read_u8(mem.constBanks(), 0xE000) == 0;

    // Enable LC reads (bank 2) and confirm LC RAM remains unchanged
    mem.setLCReadEnabled(true);
    mem.setLCBank1(false);
    bool readsRamOk = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0 &&
                      prodos8emu::read_u8(mem.constBanks(), 0xE000) == 0;

    if (!stateOk || !readsRomOk || !readsRamOk) {
      std::cerr << "FAIL: RDROM2 did not ignore writes\n";
      failures++;
    } else {
      std::cout << "PASS: RDROM2 ignores writes\n";
    }
  }

  // Test 26: LC read enabled with write protect ignores writes
  {
    std::cout << "Test 26: LC read + write protect ignores writes\n";
    prodos8emu::Apple2Memory mem;

    mem.setLCReadEnabled(true);
    mem.setLCWriteEnabled(true);
    mem.setLCBank1(true);

    prodos8emu::write_u8(mem.banks(), 0xD000, 0x11);
    bool baselineOk = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0x11;

    mem.setLCWriteEnabled(false);
    prodos8emu::write_u8(mem.banks(), 0xD000, 0x22);
    bool protectedOk = prodos8emu::read_u8(mem.constBanks(), 0xD000) == 0x11;

    if (!baselineOk || !protectedOk) {
      std::cerr << "FAIL: LC write protect did not ignore writes\n";
      failures++;
    } else {
      std::cout << "PASS: LC read + write protect ignores writes\n";
    }
  }

  // Summary
  if (failures == 0) {
    std::cout << "\nAll Apple2Memory tests passed!\n";
  } else {
    std::cout << "\n" << failures << " test(s) failed!\n";
  }

  return failures;
}
