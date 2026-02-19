#include "prodos8emu/memory.hpp"
#include "prodos8emu/errors.hpp"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <array>

int main() {
    int failures = 0;
    
    // Test setup: Create 16 banks of 4KB each
    constexpr size_t BANK_SIZE = 4096;
    constexpr size_t NUM_BANKS = 16;
    
    // Allocate 16 separate banks
    std::array<std::unique_ptr<uint8_t[]>, NUM_BANKS> bank_storage;
    prodos8emu::MemoryBanks banks;
    for (size_t i = 0; i < NUM_BANKS; i++) {
        bank_storage[i] = std::make_unique<uint8_t[]>(BANK_SIZE);
        std::memset(bank_storage[i].get(), 0, BANK_SIZE);
        banks[i] = bank_storage[i].get();
    }
    
    // Test 1: Bank pointer array can be created
    {
        std::cout << "PASS: Memory banks setup\n";
    }
    
    // Test 2: Read/Write uint8_t
    {
        prodos8emu::write_u8(banks, 0x0000, 0x42);
        prodos8emu::write_u8(banks, 0x0FFF, 0xAB);  // Last byte of bank 0
        prodos8emu::write_u8(banks, 0x1000, 0xCD);  // First byte of bank 1
        prodos8emu::write_u8(banks, 0xFFFF, 0xEF);  // Last byte of bank 15
        
        if (prodos8emu::read_u8(banks, 0x0000) != 0x42) {
            std::cerr << "FAIL: read_u8 at 0x0000\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0FFF) != 0xAB) {
            std::cerr << "FAIL: read_u8 at 0x0FFF (bank 0 boundary)\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x1000) != 0xCD) {
            std::cerr << "FAIL: read_u8 at 0x1000 (bank 1 start)\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0xFFFF) != 0xEF) {
            std::cerr << "FAIL: read_u8 at 0xFFFF (bank 15 end)\n";
            failures++;
        } else {
            std::cout << "PASS: read_u8/write_u8 basic operations\n";
        }
    }
    
    // Test 3: Bank boundary translation
    {
        // Bank 0: 0x0000-0x0FFF
        // Bank 1: 0x1000-0x1FFF
        // Bank 15: 0xF000-0xFFFF
        
        prodos8emu::write_u8(banks, 0x0000, 0x00);  // Bank 0 start
        prodos8emu::write_u8(banks, 0x0FFF, 0x01);  // Bank 0 end
        prodos8emu::write_u8(banks, 0x1000, 0x10);  // Bank 1 start
        prodos8emu::write_u8(banks, 0x1FFF, 0x11);  // Bank 1 end
        prodos8emu::write_u8(banks, 0xF000, 0xF0);  // Bank 15 start
        prodos8emu::write_u8(banks, 0xFFFF, 0xFF);  // Bank 15 end
        
        bool pass = true;
        pass = pass && (prodos8emu::read_u8(banks, 0x0000) == 0x00);
        pass = pass && (prodos8emu::read_u8(banks, 0x0FFF) == 0x01);
        pass = pass && (prodos8emu::read_u8(banks, 0x1000) == 0x10);
        pass = pass && (prodos8emu::read_u8(banks, 0x1FFF) == 0x11);
        pass = pass && (prodos8emu::read_u8(banks, 0xF000) == 0xF0);
        pass = pass && (prodos8emu::read_u8(banks, 0xFFFF) == 0xFF);
        
        if (!pass) {
            std::cerr << "FAIL: Bank boundary translation\n";
            failures++;
        } else {
            std::cout << "PASS: Bank boundary translation\n";
        }
    }
    
    // Test 4: Read/Write uint16_t little-endian
    {
        prodos8emu::write_u16_le(banks, 0x0100, 0x1234);
        
        uint16_t val = prodos8emu::read_u16_le(banks, 0x0100);
        if (val != 0x1234) {
            std::cerr << "FAIL: read_u16_le/write_u16_le basic\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0100) != 0x34) {
            std::cerr << "FAIL: write_u16_le low byte order\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0101) != 0x12) {
            std::cerr << "FAIL: write_u16_le high byte order\n";
            failures++;
        } else {
            std::cout << "PASS: read_u16_le/write_u16_le little-endian\n";
        }
    }
    
    // Test 5: uint16_t wrap-around at 0xFFFF
    {
        prodos8emu::write_u16_le(banks, 0xFFFF, 0xABCD);
        
        // Should wrap: 0xFFFF stores low byte 0xCD, 0x0000 stores high byte 0xAB
        if (prodos8emu::read_u8(banks, 0xFFFF) != 0xCD) {
            std::cerr << "FAIL: u16 wrap-around low byte at 0xFFFF\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0000) != 0xAB) {
            std::cerr << "FAIL: u16 wrap-around high byte at 0x0000\n";
            failures++;
        } else if (prodos8emu::read_u16_le(banks, 0xFFFF) != 0xABCD) {
            std::cerr << "FAIL: read_u16_le wrap-around\n";
            failures++;
        } else {
            std::cout << "PASS: uint16_t wrap-around at 0xFFFF\n";
        }
    }
    
    // Test 5b: uint16_t cross-bank boundary at 0x0FFF
    {
        prodos8emu::write_u16_le(banks, 0x0FFF, 0x5678);
        
        // Should span banks: 0x0FFF (bank 0) stores low byte 0x78, 0x1000 (bank 1) stores high byte 0x56
        if (prodos8emu::read_u8(banks, 0x0FFF) != 0x78) {
            std::cerr << "FAIL: u16 cross-bank low byte at 0x0FFF\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x1000) != 0x56) {
            std::cerr << "FAIL: u16 cross-bank high byte at 0x1000\n";
            failures++;
        } else if (prodos8emu::read_u16_le(banks, 0x0FFF) != 0x5678) {
            std::cerr << "FAIL: read_u16_le cross-bank\n";
            failures++;
        } else {
            std::cout << "PASS: uint16_t cross-bank boundary at 0x0FFF\n";
        }
    }
    
    // Test 6: Read/Write uint24_t little-endian
    {
        prodos8emu::write_u24_le(banks, 0x0200, 0x123456);
        
        uint32_t val = prodos8emu::read_u24_le(banks, 0x0200);
        if (val != 0x123456) {
            std::cerr << "FAIL: read_u24_le/write_u24_le value mismatch\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0200) != 0x56) {
            std::cerr << "FAIL: write_u24_le low byte\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0201) != 0x34) {
            std::cerr << "FAIL: write_u24_le mid byte\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0202) != 0x12) {
            std::cerr << "FAIL: write_u24_le high byte\n";
            failures++;
        } else {
            std::cout << "PASS: read_u24_le/write_u24_le little-endian\n";
        }
    }
    
    // Test 7: uint24_t wrap-around
    {
        prodos8emu::write_u24_le(banks, 0xFFFE, 0xABCDEF);
        
        // Should wrap: 0xFFFE=0xEF, 0xFFFF=0xCD, 0x0000=0xAB
        if (prodos8emu::read_u8(banks, 0xFFFE) != 0xEF) {
            std::cerr << "FAIL: u24 wrap-around byte 0\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0xFFFF) != 0xCD) {
            std::cerr << "FAIL: u24 wrap-around byte 1\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0000) != 0xAB) {
            std::cerr << "FAIL: u24 wrap-around byte 2\n";
            failures++;
        } else if (prodos8emu::read_u24_le(banks, 0xFFFE) != 0xABCDEF) {
            std::cerr << "FAIL: read_u24_le wrap-around\n";
            failures++;
        } else {
            std::cout << "PASS: uint24_t wrap-around\n";
        }
    }
    
    // Test 7b: uint24_t cross-bank boundary at 0x0FFE
    {
        prodos8emu::write_u24_le(banks, 0x0FFE, 0x9ABCDE);
        
        // Should span banks: 0x0FFE (bank 0)=0xDE, 0x0FFF (bank 0)=0xBC, 0x1000 (bank 1)=0x9A
        if (prodos8emu::read_u8(banks, 0x0FFE) != 0xDE) {
            std::cerr << "FAIL: u24 cross-bank byte 0\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x0FFF) != 0xBC) {
            std::cerr << "FAIL: u24 cross-bank byte 1\n";
            failures++;
        } else if (prodos8emu::read_u8(banks, 0x1000) != 0x9A) {
            std::cerr << "FAIL: u24 cross-bank byte 2\n";
            failures++;
        } else if (prodos8emu::read_u24_le(banks, 0x0FFE) != 0x9ABCDE) {
            std::cerr << "FAIL: read_u24_le cross-bank\n";
            failures++;
        } else {
            std::cout << "PASS: uint24_t cross-bank boundary at 0x0FFE\n";
        }
    }
    
    // Test 8: Read ProDOS counted string
    {
        // Write a counted string: count + "HELLO"
        prodos8emu::write_u8(banks, 0x0300, 5);  // count
        prodos8emu::write_u8(banks, 0x0301, 'H');
        prodos8emu::write_u8(banks, 0x0302, 'E');
        prodos8emu::write_u8(banks, 0x0303, 'L');
        prodos8emu::write_u8(banks, 0x0304, 'L');
        prodos8emu::write_u8(banks, 0x0305, 'O');
        
        std::string str = prodos8emu::read_counted_string(banks, 0x0300);
        if (str != "HELLO") {
            std::cerr << "FAIL: read_counted_string got '" << str << "'\n";
            failures++;
        } else {
            std::cout << "PASS: read_counted_string basic\n";
        }
    }
    
    // Test 9: Read ProDOS counted string with maxLen enforcement
    {
        // Write a string claiming to be 100 chars but enforce max 10
        prodos8emu::write_u8(banks, 0x0400, 100);
        for (int i = 0; i < 20; i++) {
            prodos8emu::write_u8(banks, 0x0401 + i, 'A' + (i % 26));
        }
        
        std::string str = prodos8emu::read_counted_string(banks, 0x0400, 10);
        if (str.length() != 10) {
            std::cerr << "FAIL: read_counted_string maxLen not enforced, got " << str.length() << " chars\n";
            failures++;
        } else {
            std::cout << "PASS: read_counted_string maxLen enforcement\n";
        }
    }
    
    // Test 10: Read empty counted string
    {
        prodos8emu::write_u8(banks, 0x0500, 0);  // count = 0
        
        std::string str = prodos8emu::read_counted_string(banks, 0x0500);
        if (!str.empty()) {
            std::cerr << "FAIL: read_counted_string empty, got '" << str << "'\n";
            failures++;
        } else {
            std::cout << "PASS: read_counted_string empty\n";
        }
    }
    
    // Test 11: ProDOS error codes exist and have correct canonical values
    {
        if (prodos8emu::ERR_NO_ERROR != 0x00) {
            std::cerr << "FAIL: ERR_NO_ERROR != 0x00\n";
            failures++;
        } else if (prodos8emu::ERR_BAD_CALL_NUMBER != 0x01) {
            std::cerr << "FAIL: ERR_BAD_CALL_NUMBER != 0x01\n";
            failures++;
        } else if (prodos8emu::ERR_BAD_CALL_PARAM_COUNT != 0x04) {
            std::cerr << "FAIL: ERR_BAD_CALL_PARAM_COUNT != 0x04\n";
            failures++;
        } else if (prodos8emu::ERR_IO_ERROR != 0x27) {
            std::cerr << "FAIL: ERR_IO_ERROR != 0x27\n";
            failures++;
        } else if (prodos8emu::ERR_NO_DEVICE != 0x28) {
            std::cerr << "FAIL: ERR_NO_DEVICE != 0x28\n";
            failures++;
        } else if (prodos8emu::ERR_TOO_MANY_FILES_OPEN != 0x42) {
            std::cerr << "FAIL: ERR_TOO_MANY_FILES_OPEN != 0x42\n";
            failures++;
        } else if (prodos8emu::ERR_BAD_REF_NUM != 0x43) {
            std::cerr << "FAIL: ERR_BAD_REF_NUM != 0x43\n";
            failures++;
        } else if (prodos8emu::ERR_PATH_NOT_FOUND != 0x44) {
            std::cerr << "FAIL: ERR_PATH_NOT_FOUND != 0x44\n";
            failures++;
        } else if (prodos8emu::ERR_VOL_NOT_FOUND != 0x45) {
            std::cerr << "FAIL: ERR_VOL_NOT_FOUND != 0x45\n";
            failures++;
        } else if (prodos8emu::ERR_FILE_NOT_FOUND != 0x46) {
            std::cerr << "FAIL: ERR_FILE_NOT_FOUND != 0x46\n";
            failures++;
        } else if (prodos8emu::ERR_DUPLICATE_FILENAME != 0x47) {
            std::cerr << "FAIL: ERR_DUPLICATE_FILENAME != 0x47\n";
            failures++;
        } else if (prodos8emu::ERR_VOLUME_FULL != 0x48) {
            std::cerr << "FAIL: ERR_VOLUME_FULL != 0x48\n";
            failures++;
        } else if (prodos8emu::ERR_VOL_DIR_FULL != 0x49) {
            std::cerr << "FAIL: ERR_VOL_DIR_FULL != 0x49\n";
            failures++;
        } else if (prodos8emu::ERR_UNSUPPORTED_STOR_TYPE != 0x4B) {
            std::cerr << "FAIL: ERR_UNSUPPORTED_STOR_TYPE != 0x4B\n";
            failures++;
        } else if (prodos8emu::ERR_EOF_ENCOUNTERED != 0x4C) {
            std::cerr << "FAIL: ERR_EOF_ENCOUNTERED != 0x4C\n";
            failures++;
        } else if (prodos8emu::ERR_ACCESS_ERROR != 0x4E) {
            std::cerr << "FAIL: ERR_ACCESS_ERROR != 0x4E\n";
            failures++;
        } else if (prodos8emu::ERR_FILE_OPEN != 0x50) {
            std::cerr << "FAIL: ERR_FILE_OPEN != 0x50\n";
            failures++;
        } else if (prodos8emu::ERR_DIR_COUNT_ERROR != 0x51) {
            std::cerr << "FAIL: ERR_DIR_COUNT_ERROR != 0x51\n";
            failures++;
        } else if (prodos8emu::ERR_NOT_PRODOS_VOL != 0x52) {
            std::cerr << "FAIL: ERR_NOT_PRODOS_VOL != 0x52\n";
            failures++;
        } else if (prodos8emu::ERR_BAD_BUFFER_ADDR != 0x56) {
            std::cerr << "FAIL: ERR_BAD_BUFFER_ADDR != 0x56\n";
            failures++;
        } else if (prodos8emu::ERR_FILE_STRUCTURE_DAMAGED != 0x5A) {
            std::cerr << "FAIL: ERR_FILE_STRUCTURE_DAMAGED != 0x5A\n";
            failures++;
        } else {
            std::cout << "PASS: ProDOS error codes have correct canonical values\n";
        }
    }
    
    if (failures == 0) {
        std::cout << "\nAll memory tests passed!\n";
        return EXIT_SUCCESS;
    } else {
        std::cerr << "\n" << failures << " test(s) failed!\n";
        return EXIT_FAILURE;
    }
}
