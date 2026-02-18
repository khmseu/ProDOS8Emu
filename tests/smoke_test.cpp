#include "prodos8emu/mli.hpp"
#include <iostream>
#include <cstdlib>

int main() {
    int failures = 0;
    
    // Test 1: ProDOS8Emu library can be instantiated
    {
        prodos8emu::MLIContext context;
        if (!context.isInitialized()) {
            std::cerr << "FAIL: MLIContext not initialized\n";
            failures++;
        } else {
            std::cout << "PASS: MLIContext instantiated and initialized\n";
        }
    }
    
    // Test 2: Version information is available
    {
        std::string version = prodos8emu::getVersion();
        std::string expected = PRODOS8EMU_VERSION;
        if (version != expected) {
            std::cerr << "FAIL: Expected version '" << expected << "', got '" << version << "'\n";
            failures++;
        } else {
            std::cout << "PASS: Version is '" << expected << "'\n";
        }
    }
    
    if (failures == 0) {
        std::cout << "\nAll tests passed!\n";
        return EXIT_SUCCESS;
    } else {
        std::cerr << "\n" << failures << " test(s) failed!\n";
        return EXIT_FAILURE;
    }
}
