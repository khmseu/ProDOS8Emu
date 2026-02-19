#pragma once

#include <cstdint>
#include <string>
#include <filesystem>
#include "memory.hpp"

namespace prodos8emu {

/**
 * MLIContext - Main context for ProDOS 8 MLI emulation.
 * 
 * This class manages the emulator state including:
 * - Open file table (ref_num management)
 * - I/O buffer pointers
 * - Current prefix
 * - Memory bank access
 * - Volume root mapping
 * 
 * Future phases will add MLI call methods like:
 * - uint8_t create(const uint8_t* memoryBanks, uint16_t paramBlockOffset);
 * - uint8_t open(const uint8_t* memoryBanks, uint16_t paramBlockOffset);
 * etc.
 */
class MLIContext {
public:
    /**
     * Default constructor - uses current directory as volumes root.
     */
    MLIContext();
    
    /**
     * Constructor with volumes root path.
     * 
     * @param volumesRoot Root directory where ProDOS volumes are mapped
     */
    explicit MLIContext(const std::filesystem::path& volumesRoot);
    
    ~MLIContext();
    
    // Smoke test method to verify instantiation
    bool isInitialized() const;
    
    /**
     * MLI Call: SET_PREFIX ($C6)
     * 
     * Set the current pathname prefix.
     * 
     * Parameter block:
     *   +0: param_count (1 byte) - must be 1
     *   +1: pathname pointer (2 bytes, little-endian) - pointer to counted string
     * 
     * @param banks Memory banks (const)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t setPrefixCall(const ConstMemoryBanks& banks, uint16_t paramBlockAddr);
    
    /**
     * MLI Call: GET_PREFIX ($C7)
     * 
     * Get the current pathname prefix.
     * 
     * Parameter block:
     *   +0: param_count (1 byte) - must be 1
     *   +1: data_buffer pointer (2 bytes, little-endian) - where to write counted string
     * 
     * @param banks Memory banks (mutable)
     * @param paramBlockAddr Address of parameter block
     * @return ProDOS error code
     */
    uint8_t getPrefixCall(MemoryBanks& banks, uint16_t paramBlockAddr);
    
    /**
     * Get the current prefix (for testing).
     * 
     * @return Current prefix string
     */
    const std::string& getPrefix() const { return m_prefix; }
    
    /**
     * Get the volumes root path (for testing).
     * 
     * @return Volumes root path
     */
    const std::filesystem::path& getVolumesRoot() const { return m_volumesRoot; }
    
private:
    bool m_initialized;
    std::string m_prefix;
    std::filesystem::path m_volumesRoot;
};

/**
 * Get library version string.
 */
std::string getVersion();

} // namespace prodos8emu
