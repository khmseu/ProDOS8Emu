#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <charconv>

enum class ParseResult {
    Ok,
    Help,
    Error
};

struct CliOptions {
    std::string rom_path;
    std::string system_file_path;
    std::string volume_root;
    int max_instructions = -1;  // -1 means unlimited
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
            int value = 0;
            auto [ptr, ec] = std::from_chars(str, str_end, value);
            if (ec != std::errc{} || ptr != str_end) {
                std::cerr << "Error: --max-instructions must be a valid integer\n";
                return ParseResult::Error;
            }
            if (value < 0) {
                std::cerr << "Error: --max-instructions must be non-negative\n";
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
    
    // For now, just print the parsed options
    std::cout << "Configuration:\n"
              << "  rom=" << opts.rom_path << "\n"
              << "  sys=" << opts.system_file_path << "\n";
    
    if (opts.max_instructions >= 0) {
        std::cout << "  max=" << opts.max_instructions << "\n";
    } else {
        std::cout << "  max=unlimited\n";
    }
    
    if (!opts.volume_root.empty()) {
        std::cout << "  volroot=" << opts.volume_root << "\n";
    }
    
    std::cout << "\n(Emulator execution not yet implemented)\n";
    
    return 0;
}
