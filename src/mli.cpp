#include "prodos8emu/mli.hpp"

namespace prodos8emu {

MLIContext::MLIContext() : m_initialized(true) {
    // Placeholder constructor
}

MLIContext::~MLIContext() {
    // Placeholder destructor
}

bool MLIContext::isInitialized() const {
    return m_initialized;
}

std::string getVersion() {
    return PRODOS8EMU_VERSION;
}

} // namespace prodos8emu
