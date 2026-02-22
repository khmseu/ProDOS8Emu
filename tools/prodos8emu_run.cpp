#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <charconv>
#include <exception>

#include "prodos8emu/apple2mem.hpp"
#include "prodos8emu/cpu65c02.hpp"
#include "prodos8emu/mli.hpp"
#include "prodos8emu/memory.hpp"
#include "prodos8emu/system_loader.hpp"

enum class ParseResult {
    Ok,
    Help,
    Error
};

struct CliOptions {
    std::string rom_path;
    std::string system_file_path;
    std::string volume_root;
    uint64_t max_instructions = 1000000;  // Default: 1 million instructions
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name 
              << " [OPTIONS] ROM_PATH SYSTEM_FILE_PATH\n\n"
              << "Run a ProDOS 8 system file in emulation.\n\n"
              << "Arguments:\n"
              << "  ROM_PATH          Path to Apple II ROM file\n"
              << "  SYSTEM_FILE_PATH  Path to ProDOS 8 system file to execute\n\n"
              << "Options:\n"
              << "  -h, --help                Show this help message\n"
              << "  --max-instructions N      Stop execution after N instructions\n"
              << "  --volume-root PATH        Root directory for volume mappings\n";
}

ParseResult parse_args(int argc, char* argv[], CliOptions& opts) {
    if (argc < 2) {
        return ParseResult::Error;
    }

    int positional_count = 0;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return ParseResult::Help;
        } else if (arg == "--max-instructions") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --max-instructions requires an argument\n";
                return ParseResult::Error;
            }
            ++i;
            const char* str = argv[i];
            const char* str_end = str + std::strlen(str);
            uint64_t value = 0;
            auto [ptr, ec] = std::from_chars(str, str_end, value);
            if (ec != std::errc{} || ptr != str_end) {
                std::cerr << "Error: --max-instructions must be a valid unsigned integer\n";
                return ParseResult::Error;
            }
            opts.max_instructions = value;
        } else if (arg == "--volume-root") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --volume-root requires an argument\n";
                return ParseResult::Error;
            }
            ++i;
            opts.volume_root = argv[i];
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            return ParseResult::Error;
        } else {
            // Positional argument
            if (positional_count == 0) {
                opts.rom_path = arg;
            } else if (positional_count == 1) {
                opts.system_file_path = arg;
            } else {
                std::cerr << "Error: Too many positional arguments\n";
                return ParseResult::Error;
            }
            ++positional_count;
        }
    }
    
    if (positional_count < 2) {
        std::cerr << "Error: Both ROM_PATH and SYSTEM_FILE_PATH are required\n";
        return ParseResult::Error;
    }
    
    return ParseResult::Ok;
}

int main(int argc, char* argv[]) {
    CliOptions opts;
    
    ParseResult result = parse_args(argc, argv, opts);
    
    if (result == ParseResult::Help) {
        return 0;
    }
    
    if (result == ParseResult::Error) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Set default volume root if not provided
    if (opts.volume_root.empty()) {
        opts.volume_root = ".";
    }
    
    // Print parsed configuration
    std::cout << "Configuration:\n"
              << "  rom=" << opts.rom_path << "\n"
              << "  sys=" << opts.system_file_path << "\n"
              << "  max=" << opts.max_instructions << "\n"
              << "  volroot=" << opts.volume_root << "\n\n";
    
    try {
        // Initialize emulator components
        prodos8emu::Apple2Memory mem;
        prodos8emu::MLIContext ctx(opts.volume_root);
        prodos8emu::CPU65C02 cpu(mem);
        cpu.attachMLI(ctx);
        
        std::cout << "Loading ROM from " << opts.rom_path << "...\n";
        mem.loadROM(opts.rom_path);
        
        std::cout << "Loading system file from " << opts.system_file_path << "...\n";
        prodos8emu::loadSystemFile(mem, opts.system_file_path, 0x2000);
        
        std::cout << "Initializing warm restart vector...\n";
        prodos8emu::initWarmStartVector(mem, 0x2000);
        
        std::cout << "Setting reset vector to $2000...\n";
        // Enable LC read/write to modify reset vector area
        mem.setLCReadEnabled(true);
        mem.setLCWriteEnabled(true);
        
        // Write $2000 to reset vector at $FFFC/$FFFD
        prodos8emu::write_u16_le(mem.banks(), 0xFFFC, 0x2000);
        
        // Reset CPU (loads PC from reset vector)
        cpu.reset();
        
        // Restore LC to ROM mode for execution
        mem.setLCReadEnabled(false);
        mem.setLCWriteEnabled(false);
        
        std::cout << "Starting CPU execution (max " << opts.max_instructions << " instructions)...\n\n";
        
        // Run CPU with bounded instruction count
        uint64_t instructionCount = cpu.run(opts.max_instructions);
        
        // Print final status
        std::cout << "\n=== Execution Summary ===\n";
        std::cout << "Instructions executed: " << instructionCount << "\n";
        std::cout << "CPU Status:\n";
        std::cout << "  Stopped: " << (cpu.isStopped() ? "yes" : "no") << "\n";
        std::cout << "  Waiting: " << (cpu.isWaiting() ? "yes (WAI)" : "no") << "\n";
        std::cout << "Registers:\n";
        std::cout << "  PC: $" << std::hex << std::uppercase << cpu.regs().pc << "\n";
        std::cout << "  A:  $" << std::hex << std::uppercase << (int)cpu.regs().a << "\n";
        std::cout << "  X:  $" << std::hex << std::uppercase << (int)cpu.regs().x << "\n";
        std::cout << "  Y:  $" << std::hex << std::uppercase << (int)cpu.regs().y << "\n";
        std::cout << "  SP: $" << std::hex << std::uppercase << (int)cpu.regs().sp << "\n";
        std::cout << "  P:  $" << std::hex << std::uppercase << (int)cpu.regs().p << std::dec << "\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }
}
