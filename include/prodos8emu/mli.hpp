#pragma once

#include <cstdint>
#include <string>

namespace prodos8emu {

/**
 * MLIContext - Main context for ProDOS 8 MLI emulation.
 * 
 * This class manages the emulator state including:
 * - Open file table (ref_num management)
 * - I/O buffer pointers
 * - Current prefix
 * - Memory bank access
 * 
 * Future phases will add MLI call methods like:
 * - uint8_t create(const uint8_t* memoryBanks, uint16_t paramBlockOffset);
 * - uint8_t open(const uint8_t* memoryBanks, uint16_t paramBlockOffset);
 * etc.
 */
class MLIContext {
public:
    MLIContext();
    ~MLIContext();
    
    // Smoke test method to verify instantiation
    bool isInitialized() const;
    
private:
    bool m_initialized;
};

/**
 * Get library version string.
 */
std::string getVersion();

} // namespace prodos8emu
