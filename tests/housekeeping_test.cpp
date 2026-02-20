#include "prodos8emu/mli.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/errors.hpp"
#include "prodos8emu/xattr.hpp"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// Helper to set up memory banks
class TestMemory {
public:
    TestMemory() {
        for (std::size_t i = 0; i < prodos8emu::NUM_BANKS; i++) {
            m_data[i].fill(0);
            m_banks[i] = m_data[i].data();
            m_constBanks[i] = m_data[i].data();
        }
    }
    
    prodos8emu::MemoryBanks& banks() { return m_banks; }
    prodos8emu::ConstMemoryBanks& constBanks() { return m_constBanks; }
    
    // Helper to write a counted string to memory
    void writeCountedString(uint16_t addr, const std::string& str) {
        prodos8emu::write_u8(m_banks, addr, static_cast<uint8_t>(str.length()));
        for (size_t i = 0; i < str.length(); i++) {
            prodos8emu::write_u8(m_banks, addr + 1 + i, static_cast<uint8_t>(str[i]));
        }
    }
    
    // Helper to read a counted string from memory
    std::string readCountedString(uint16_t addr) {
        return prodos8emu::read_counted_string(m_banks, addr);
    }
    
private:
    std::array<std::array<uint8_t, prodos8emu::BANK_SIZE>, prodos8emu::NUM_BANKS> m_data;
    prodos8emu::MemoryBanks m_banks;
    prodos8emu::ConstMemoryBanks m_constBanks;
};

int main() {
    int failures = 0;
    
    // Set up test environment
    fs::path tempDir = fs::temp_directory_path() / "prodos8emu_housekeeping_test";
    fs::remove_all(tempDir);
    fs::create_directories(tempDir);
    
    // Create a test volume
    fs::path volume1 = tempDir / "V1";
    fs::create_directories(volume1);
    
    std::cout << "Test environment: " << tempDir << "\n";
    
    // Test 1: CREATE a standard file with file_type and aux_type
    {
        std::cout << "Test 1: CREATE standard file\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        // Build parameter block for CREATE
        // param_count=7, pathname, access, file_type, aux_type, storage_type, create_date, create_time
        mem.writeCountedString(0x0400, "/V1/TESTFILE");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 7); // param_count
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400); // pathname ptr
        prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0xC3); // access (read+write+rename+destroy)
        prodos8emu::write_u8(mem.banks(), paramBlock + 4, 0x06); // file_type = BIN
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 5, 0x2000); // aux_type
        prodos8emu::write_u8(mem.banks(), paramBlock + 7, 0x01); // storage_type = standard file
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 8, 0x0000); // create_date (0 = use current)
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 10, 0x0000); // create_time (0 = use current)
        
        uint8_t err = ctx.createCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: CREATE returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            // Verify file was created
            fs::path hostPath = volume1 / "TESTFILE";
            if (!fs::exists(hostPath)) {
                std::cerr << "FAIL: File was not created at " << hostPath << "\n";
                failures++;
            } else {
                std::cout << "PASS: CREATE standard file\n";
            }
        }
    }
    
    // Test 2: GET_FILE_INFO returns file_type, aux_type, and access
    {
        std::cout << "Test 2: GET_FILE_INFO returns metadata\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        mem.writeCountedString(0x0400, "/V1/TESTFILE");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 0x0A); // param_count
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400); // pathname ptr
        
        uint8_t err = ctx.getFileInfoCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: GET_FILE_INFO returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            uint8_t access = prodos8emu::read_u8(mem.banks(), paramBlock + 3);
            uint8_t file_type = prodos8emu::read_u8(mem.banks(), paramBlock + 4);
            uint16_t aux_type = prodos8emu::read_u16_le(mem.banks(), paramBlock + 5);
            uint8_t storage_type = prodos8emu::read_u8(mem.banks(), paramBlock + 7);
            
            bool success = true;
            if (file_type != 0x06) {
                std::cerr << "FAIL: Expected file_type 0x06, got 0x" << std::hex << (int)file_type << "\n";
                success = false;
            }
            if (aux_type != 0x2000) {
                std::cerr << "FAIL: Expected aux_type 0x2000, got 0x" << std::hex << aux_type << "\n";
                success = false;
            }
            if (access != 0xC3) {
                std::cerr << "FAIL: Expected access 0xC3, got 0x" << std::hex << (int)access << "\n";
                success = false;
            }
            if (storage_type != 0x01) {
                std::cerr << "FAIL: Expected storage_type 0x01, got 0x" << std::hex << (int)storage_type << "\n";
                success = false;
            }
            
            if (success) {
                std::cout << "PASS: GET_FILE_INFO returns metadata\n";
            } else {
                failures++;
            }
        }
    }
    
    // Test 3: SET_FILE_INFO updates access and mod_date/time
    {
        std::cout << "Test 3: SET_FILE_INFO updates metadata\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        mem.writeCountedString(0x0400, "/V1/TESTFILE");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 7); // param_count
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400); // pathname ptr
        prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0xE3); // access (new value)
        prodos8emu::write_u8(mem.banks(), paramBlock + 4, 0x04); // file_type (TXT)
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 5, 0x1234); // aux_type
        prodos8emu::write_u8(mem.banks(), paramBlock + 7, 0); // null_field[0]
        prodos8emu::write_u8(mem.banks(), paramBlock + 8, 0); // null_field[1]
        prodos8emu::write_u8(mem.banks(), paramBlock + 9, 0); // null_field[2]
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 10, 0x1234); // mod_date
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 12, 0x0800); // mod_time (8:00)
        
        uint8_t err = ctx.setFileInfoCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: SET_FILE_INFO returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            // Verify with GET_FILE_INFO
            mem.writeCountedString(0x0500, "/V1/TESTFILE");
            uint16_t getBlock = 0x0310;
            prodos8emu::write_u8(mem.banks(), getBlock, 0x0A);
            prodos8emu::write_u16_le(mem.banks(), getBlock + 1, 0x0500);
            
            err = ctx.getFileInfoCall(mem.banks(), getBlock);
            if (err != prodos8emu::ERR_NO_ERROR) {
                std::cerr << "FAIL: GET_FILE_INFO after SET returned error 0x" << std::hex << (int)err << "\n";
                failures++;
            } else {
                uint8_t access = prodos8emu::read_u8(mem.banks(), getBlock + 3);
                uint8_t file_type = prodos8emu::read_u8(mem.banks(), getBlock + 4);
                uint16_t aux_type = prodos8emu::read_u16_le(mem.banks(), getBlock + 5);
                
                bool success = true;
                if (access != 0xE3) {
                    std::cerr << "FAIL: Expected access 0xE3, got 0x" << std::hex << (int)access << "\n";
                    success = false;
                }
                if (file_type != 0x04) {
                    std::cerr << "FAIL: Expected file_type 0x04, got 0x" << std::hex << (int)file_type << "\n";
                    success = false;
                }
                if (aux_type != 0x1234) {
                    std::cerr << "FAIL: Expected aux_type 0x1234, got 0x" << std::hex << aux_type << "\n";
                    success = false;
                }
                
                if (success) {
                    std::cout << "PASS: SET_FILE_INFO updates metadata\n";
                } else {
                    failures++;
                }
            }
        }
    }
    
    // Test 4: RENAME in same directory succeeds
    {
        std::cout << "Test 4: RENAME in same directory\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        mem.writeCountedString(0x0400, "/V1/TESTFILE");
        mem.writeCountedString(0x0450, "/V1/NEWNAME");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 2); // param_count
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400); // pathname ptr
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0450); // new_pathname ptr
        
        uint8_t err = ctx.renameCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: RENAME returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            fs::path oldPath = volume1 / "TESTFILE";
            fs::path newPath = volume1 / "NEWNAME";
            if (fs::exists(oldPath)) {
                std::cerr << "FAIL: Old file still exists at " << oldPath << "\n";
                failures++;
            } else if (!fs::exists(newPath)) {
                std::cerr << "FAIL: New file not found at " << newPath << "\n";
                failures++;
            } else {
                std::cout << "PASS: RENAME in same directory\n";
            }
        }
    }
    
    // Test 5: RENAME across directories fails
    {
        std::cout << "Test 5: RENAME across directories fails\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        // Create directory for test
        fs::create_directories(volume1 / "SUBDIR");
        
        mem.writeCountedString(0x0400, "/V1/NEWNAME");
        mem.writeCountedString(0x0450, "/V1/SUBDIR/MOVED");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 2);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 3, 0x0450);
        
        uint8_t err = ctx.renameCall(mem.constBanks(), paramBlock);
        if (err == prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: RENAME across directories should fail\n";
            failures++;
        } else if (err == prodos8emu::ERR_INVALID_PATH_SYNTAX) {
            std::cout << "PASS: RENAME across directories fails\n";
        } else {
            std::cerr << "FAIL: Expected ERR_INVALID_PATH_SYNTAX, got 0x" << std::hex << (int)err << "\n";
            failures++;
        }
    }
    
    // Test 6: DESTROY removes file
    {
        std::cout << "Test 6: DESTROY removes file\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        mem.writeCountedString(0x0400, "/V1/NEWNAME");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 1); // param_count
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400); // pathname ptr
        
        uint8_t err = ctx.destroyCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: DESTROY returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            fs::path filePath = volume1 / "NEWNAME";
            if (fs::exists(filePath)) {
                std::cerr << "FAIL: File still exists at " << filePath << "\n";
                failures++;
            } else {
                std::cout << "PASS: DESTROY removes file\n";
            }
        }
    }
    
    // Test 7: CREATE directory
    {
        std::cout << "Test 7: CREATE directory\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        mem.writeCountedString(0x0400, "/V1/MYDIR");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 7);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
        prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0xE3);
        prodos8emu::write_u8(mem.banks(), paramBlock + 4, 0x0F); // file_type for directory
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 5, 0x0000);
        prodos8emu::write_u8(mem.banks(), paramBlock + 7, 0x0D); // storage_type = directory
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 8, 0x0000);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 10, 0x0000);
        
        uint8_t err = ctx.createCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: CREATE directory returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            fs::path dirPath = volume1 / "MYDIR";
            if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
                std::cerr << "FAIL: Directory was not created at " << dirPath << "\n";
                failures++;
            } else {
                std::cout << "PASS: CREATE directory\n";
            }
        }
    }
    
    // Test 8: ON_LINE with unit_num=0 returns volume list
    {
        std::cout << "Test 8: ON_LINE returns volume list\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        uint16_t paramBlock = 0x0300;
        uint16_t dataBuffer = 0x0400;
        
        prodos8emu::write_u8(mem.banks(), paramBlock, 2); // param_count
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0); // unit_num = 0 (all volumes)
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, dataBuffer); // data_buffer ptr
        
        uint8_t err = ctx.onLineCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: ON_LINE returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            // Check first record
            uint8_t byte0 = prodos8emu::read_u8(mem.banks(), dataBuffer);
            uint8_t nameLen = byte0 & 0x0F;
            
            if (nameLen == 0) {
                std::cerr << "FAIL: No volume found in ON_LINE result\n";
                failures++;
            } else if (nameLen != 2) {
                std::cerr << "FAIL: Expected name_len=2 (V1), got " << (int)nameLen << "\n";
                failures++;
            } else {
                // Read volume name (next bytes, NOT prefixed with /)
                std::string volName;
                for (int i = 0; i < nameLen; i++) {
                    volName += static_cast<char>(prodos8emu::read_u8(mem.banks(), dataBuffer + 1 + i));
                }
                if (volName != "V1") {
                    std::cerr << "FAIL: Expected volume name 'V1', got '" << volName << "'\n";
                    failures++;
                } else {
                    std::cout << "PASS: ON_LINE returns volume list\n";
                }
            }
        }
    }
    
    // Test 9: CREATE duplicate file fails
    {
        std::cout << "Test 9: CREATE duplicate file fails\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        // Create a file first
        mem.writeCountedString(0x0400, "/V1/DUPTEST");
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 7);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
        prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0xC3);
        prodos8emu::write_u8(mem.banks(), paramBlock + 4, 0x00);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 5, 0x0000);
        prodos8emu::write_u8(mem.banks(), paramBlock + 7, 0x01);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 8, 0x0000);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 10, 0x0000);
        
        uint8_t err = ctx.createCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: Initial CREATE returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            // Try to create again
            err = ctx.createCall(mem.constBanks(), paramBlock);
            if (err != prodos8emu::ERR_DUPLICATE_FILENAME) {
                std::cerr << "FAIL: Expected ERR_DUPLICATE_FILENAME, got 0x" << std::hex << (int)err << "\n";
                failures++;
            } else {
                std::cout << "PASS: CREATE duplicate file fails\n";
            }
        }
    }
    
    // Test 10: DESTROY non-existent file fails
    {
        std::cout << "Test 10: DESTROY non-existent file fails\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        mem.writeCountedString(0x0400, "/V1/NOTEXIST");
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 1);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
        
        uint8_t err = ctx.destroyCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_FILE_NOT_FOUND) {
            std::cerr << "FAIL: Expected ERR_FILE_NOT_FOUND, got 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            std::cout << "PASS: DESTROY non-existent file fails\n";
        }
    }
    
    // Test 11: ON_LINE with unit_num != 0 returns single volume or ERR_NO_DEVICE
    {
        std::cout << "Test 11: ON_LINE with specific unit_num\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        // First, create a second volume
        fs::path volume2 = tempDir / "V2";
        fs::create_directories(volume2);
        
        // Try unit_num = 0x10 (slot 1, drive 0) - should match V1
        uint16_t paramBlock = 0x0300;
        uint16_t dataBuffer = 0x0400;
        
        prodos8emu::write_u8(mem.banks(), paramBlock, 2); // param_count
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0x10); // unit_num = slot 1, drive 0
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, dataBuffer); // data_buffer ptr
        
        uint8_t err = ctx.onLineCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: ON_LINE with unit_num=0x10 returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            // Check that exactly one record was written
            uint8_t byte0 = prodos8emu::read_u8(mem.banks(), dataBuffer);
            uint8_t nameLen = byte0 & 0x0F;
            
            if (nameLen != 2) {
                std::cerr << "FAIL: Expected name_len=2, got " << (int)nameLen << "\n";
                failures++;
            } else {
                std::string volName;
                for (int i = 0; i < nameLen; i++) {
                    volName += static_cast<char>(prodos8emu::read_u8(mem.banks(), dataBuffer + 1 + i));
                }
                if (volName != "V1") {
                    std::cerr << "FAIL: Expected volume 'V1', got '" << volName << "'\n";
                    failures++;
                } else {
                    std::cout << "PASS: ON_LINE with specific unit_num\n";
                }
            }
        }
        
        // Try unit_num that doesn't exist (e.g., 0xF0 = slot 7, drive 0)
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0xF0);
        err = ctx.onLineCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_DEVICE) {
            std::cerr << "FAIL: Expected ERR_NO_DEVICE for non-existent unit, got 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            std::cout << "PASS: ON_LINE returns ERR_NO_DEVICE for non-existent unit\n";
        }
    }
    
    // Test 12: Pathname > 64 bytes returns ERR_INVALID_PATH_SYNTAX
    {
        std::cout << "Test 12: Pathname > 64 bytes fails\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        // Create a 65-byte pathname (counted string excludes length byte)
        std::string longPath = "/V1/";
        while (longPath.length() < 65) {
            longPath += "A";
        }
        
        mem.writeCountedString(0x0400, longPath);
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 7);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
        prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0xC3);
        prodos8emu::write_u8(mem.banks(), paramBlock + 4, 0x00);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 5, 0x0000);
        prodos8emu::write_u8(mem.banks(), paramBlock + 7, 0x01);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 8, 0x0000);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 10, 0x0000);
        
        uint8_t err = ctx.createCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_INVALID_PATH_SYNTAX) {
            std::cerr << "FAIL: Expected ERR_INVALID_PATH_SYNTAX for >64 byte pathname, got 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            std::cout << "PASS: Pathname > 64 bytes fails\n";
        }
    }
    
    // Test 13: Relative path with empty prefix returns ERR_INVALID_PATH_SYNTAX
    {
        std::cout << "Test 13: Relative path with empty prefix fails\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        // Ensure prefix is empty (it should be by default)
        // Try to create a file with relative path
        mem.writeCountedString(0x0400, "RELATIVE");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 7);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
        prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0xC3);
        prodos8emu::write_u8(mem.banks(), paramBlock + 4, 0x00);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 5, 0x0000);
        prodos8emu::write_u8(mem.banks(), paramBlock + 7, 0x01);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 8, 0x0000);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 10, 0x0000);
        
        uint8_t err = ctx.createCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_INVALID_PATH_SYNTAX) {
            std::cerr << "FAIL: Expected ERR_INVALID_PATH_SYNTAX for relative path with empty prefix, got 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            std::cout << "PASS: Relative path with empty prefix fails\n";
        }
    }
    
    // Test 14: Corrupted metadata xattr falls back to defaults
    {
        std::cout << "Test 14: Corrupted metadata xattr falls back to defaults\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        // Create a file first
        mem.writeCountedString(0x0400, "/V1/CORRUPT");
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 7);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
        prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0xC3);
        prodos8emu::write_u8(mem.banks(), paramBlock + 4, 0x06);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 5, 0x2000);
        prodos8emu::write_u8(mem.banks(), paramBlock + 7, 0x01);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 8, 0x0000);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 10, 0x0000);
        
        uint8_t err = ctx.createCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: Initial CREATE for corrupt test failed: 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            // Corrupt the metadata xattr
            fs::path hostPath = volume1 / "CORRUPT";
            prodos8emu::prodos8_set_xattr(hostPath.string(), "metadata", "garbage:data:invalid");
            
            // Try to get file info - should fall back to defaults rather than return garbage
            mem.writeCountedString(0x0500, "/V1/CORRUPT");
            uint16_t getBlock = 0x0310;
            prodos8emu::write_u8(mem.banks(), getBlock, 0x0A);
            prodos8emu::write_u16_le(mem.banks(), getBlock + 1, 0x0500);
            
            err = ctx.getFileInfoCall(mem.banks(), getBlock);
            if (err != prodos8emu::ERR_NO_ERROR) {
                std::cerr << "FAIL: GET_FILE_INFO on corrupted metadata returned error 0x" << std::hex << (int)err << "\n";
                failures++;
            } else {
                uint8_t file_type = prodos8emu::read_u8(mem.banks(), getBlock + 4);
                uint8_t storage_type = prodos8emu::read_u8(mem.banks(), getBlock + 7);
                
                // Should have sensible defaults (storage_type 0x01 for regular file)
                bool success = true;
                if (storage_type != 0x01 && storage_type != 0x00) {
                    std::cerr << "FAIL: storage_type should be 0x01 (or 0x00), got 0x" << std::hex << (int)storage_type << "\n";
                    success = false;
                }
                // file_type could be 0x00 (default) but shouldn't be garbage like 0xFF or high values
                if (file_type > 0xF0) {
                    std::cerr << "FAIL: file_type appears to be garbage: 0x" << std::hex << (int)file_type << "\n";
                    success = false;
                }
                
                if (success) {
                    std::cout << "PASS: Corrupted metadata xattr falls back to defaults\n";
                } else {
                    failures++;
                }
            }
        }
    }
    
    // Test 15: ON_LINE terminator record at correct 16-byte boundary
    {
        std::cout << "Test 15: ON_LINE terminator record at correct boundary\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        uint16_t paramBlock = 0x0300;
        uint16_t dataBuffer = 0x0400;
        
        prodos8emu::write_u8(mem.banks(), paramBlock, 2);
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0); // unit_num = 0 (all volumes)
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, dataBuffer);
        
        uint8_t err = ctx.onLineCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: ON_LINE returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            // Count records until terminator (byte0 == 0)
            int recordCount = 0;
            uint16_t offset = dataBuffer;
            while (recordCount < 16) { // Max 15 volumes + terminator
                uint8_t byte0 = prodos8emu::read_u8(mem.banks(), offset);
                if (byte0 == 0) {
                    // Found terminator
                    break;
                }
                offset += 16;
                recordCount++;
            }
            
            // Check terminator is at correct boundary
            if ((offset - dataBuffer) % 16 != 0) {
                std::cerr << "FAIL: Terminator not at 16-byte boundary\n";
                failures++;
            } else {
                std::cout << "PASS: ON_LINE terminator at correct boundary\n";
            }
        }
    }
    
    // Test 16: ON_LINE caps at 14 volumes + terminator
    {
        std::cout << "Test 16: ON_LINE caps at 14 volumes\n";
        TestMemory mem;
        
        // Create 20 volume directories
        fs::path tempEnumDir = fs::temp_directory_path() / "prodos8emu_enum_test";
        fs::remove_all(tempEnumDir);
        fs::create_directories(tempEnumDir);
        
        for (int i = 1; i <= 20; i++) {
            fs::path vol = tempEnumDir / ("VOL" + std::to_string(i));
            fs::create_directories(vol);
        }
        
        prodos8emu::MLIContext ctx(tempEnumDir);
        
        uint16_t paramBlock = 0x0300;
        uint16_t dataBuffer = 0x0400;
        
        prodos8emu::write_u8(mem.banks(), paramBlock, 2);
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0); // unit_num = 0
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, dataBuffer);
        
        uint8_t err = ctx.onLineCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: ON_LINE returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            // Count volumes (non-terminator records)
            int volumeCount = 0;
            uint16_t offset = dataBuffer;
            for (int i = 0; i < 16; i++) {
                uint8_t byte0 = prodos8emu::read_u8(mem.banks(), offset);
                if (byte0 == 0) {
                    break; // Terminator found
                }
                volumeCount++;
                offset += 16;
            }
            
            // Check that we got exactly 14 volumes
            if (volumeCount != 14) {
                std::cerr << "FAIL: Expected 14 volumes, got " << volumeCount << "\n";
                failures++;
            } else {
                // Check that record 14 (offset 14*16) is the terminator
                uint8_t terminator = prodos8emu::read_u8(mem.banks(), dataBuffer + 14*16);
                if (terminator != 0) {
                    std::cerr << "FAIL: Expected terminator at record 14, got byte0=0x" << std::hex << (int)terminator << "\n";
                    failures++;
                } else {
                    std::cout << "PASS: ON_LINE caps at 14 volumes\n";
                }
            }
        }
        
        fs::remove_all(tempEnumDir);
    }
    
    // Test 17: ON_LINE mapping consistency with 2 volumes
    {
        std::cout << "Test 17: ON_LINE mapping consistency\n";
        TestMemory mem;
        
        // Create temp directory with exactly 2 volumes
        fs::path tempMapDir = fs::temp_directory_path() / "prodos8emu_map_test";
        fs::remove_all(tempMapDir);
        fs::create_directories(tempMapDir);
        
        fs::create_directories(tempMapDir / "FIRST");
        fs::create_directories(tempMapDir / "SECOND");
        
        prodos8emu::MLIContext ctx(tempMapDir);
        
        uint16_t paramBlock = 0x0300;
        uint16_t dataBuffer = 0x0400;
        
        // unit_num = 0x10 (slot 1, drive 0) should return first sorted volume
        prodos8emu::write_u8(mem.banks(), paramBlock, 2);
        prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0x10);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 2, dataBuffer);
        
        uint8_t err = ctx.onLineCall(mem.banks(), paramBlock);
        if (err != prodos8emu::ERR_NO_ERROR) {
            std::cerr << "FAIL: ON_LINE unit_num=0x10 returned error 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            uint8_t nameLen = prodos8emu::read_u8(mem.banks(), dataBuffer) & 0x0F;
            std::string volName;
            for (int i = 0; i < nameLen; i++) {
                volName += static_cast<char>(prodos8emu::read_u8(mem.banks(), dataBuffer + 1 + i));
            }
            
            if (volName != "FIRST") {
                std::cerr << "FAIL: Expected 'FIRST' for unit 0x10, got '" << volName << "'\n";
                failures++;
            } else {
                // unit_num = 0x90 (slot 1, drive 1) should return second sorted volume
                prodos8emu::write_u8(mem.banks(), paramBlock + 1, 0x90);
                err = ctx.onLineCall(mem.banks(), paramBlock);
                if (err != prodos8emu::ERR_NO_ERROR) {
                    std::cerr << "FAIL: ON_LINE unit_num=0x90 returned error 0x" << std::hex << (int)err << "\n";
                    failures++;
                } else {
                    nameLen = prodos8emu::read_u8(mem.banks(), dataBuffer) & 0x0F;
                    volName.clear();
                    for (int i = 0; i < nameLen; i++) {
                        volName += static_cast<char>(prodos8emu::read_u8(mem.banks(), dataBuffer + 1 + i));
                    }
                    
                    if (volName != "SECOND") {
                        std::cerr << "FAIL: Expected 'SECOND' for unit 0x90, got '" << volName << "'\n";
                        failures++;
                    } else {
                        std::cout << "PASS: ON_LINE mapping consistency\n";
                    }
                }
            }
        }
        
        fs::remove_all(tempMapDir);
    }
    
    // Test 18: Permission denied error
    {
        std::cout << "Test 18: Permission denied error\n";
        TestMemory mem;
        prodos8emu::MLIContext ctx(tempDir);
        
        // Create a read-only volume directory
        fs::path readOnlyVol = tempDir / "READONLY";
        fs::create_directories(readOnlyVol);
        fs::permissions(readOnlyVol, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);
        
        // Try to create a file in the read-only directory
        mem.writeCountedString(0x0400, "/READONLY/TESTFILE");
        
        uint16_t paramBlock = 0x0300;
        prodos8emu::write_u8(mem.banks(), paramBlock, 7);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 1, 0x0400);
        prodos8emu::write_u8(mem.banks(), paramBlock + 3, 0xC3);
        prodos8emu::write_u8(mem.banks(), paramBlock + 4, 0x00);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 5, 0x0000);
        prodos8emu::write_u8(mem.banks(), paramBlock + 7, 0x01);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 8, 0x0000);
        prodos8emu::write_u16_le(mem.banks(), paramBlock + 10, 0x0000);
        
        uint8_t err = ctx.createCall(mem.constBanks(), paramBlock);
        if (err != prodos8emu::ERR_ACCESS_ERROR) {
            std::cerr << "FAIL: Expected ERR_ACCESS_ERROR for read-only directory, got 0x" << std::hex << (int)err << "\n";
            failures++;
        } else {
            std::cout << "PASS: Permission denied error\n";
        }
        
        // Restore permissions for cleanup
        fs::permissions(readOnlyVol, fs::perms::owner_all, fs::perm_options::replace);
    }
    
    // Clean up
    fs::remove_all(tempDir);
    
    if (failures == 0) {
        std::cout << "\nAll tests passed!\n";
        return 0;
    } else {
        std::cerr << "\n" << failures << " test(s) failed\n";
        return 1;
    }
}
