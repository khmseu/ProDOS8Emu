#include "prodos8emu/path.hpp"

#include <sstream>

namespace prodos8emu {

  std::string readNormalizedCountedString(const ConstMemoryBanks& banks, uint16_t addr) {
    uint8_t     count = read_u8(banks, addr);
    std::string result;
    result.reserve(count);

    for (size_t i = 0; i < count; i++) {
      char ch = static_cast<char>(read_u8(banks, static_cast<uint16_t>(addr + 1 + i)));
      result.push_back(normalizeChar(ch));
    }

    return result;
  }

  bool isValidComponent(const std::string& component) {
    // Length must be 1-15
    if (component.empty() || component.length() > 15) {
      return false;
    }

    // First character must be A-Z
    char first = component[0];
    if (first < 'A' || first > 'Z') {
      return false;
    }

    // Remaining characters: A-Z, 0-9, '.'
    for (size_t i = 1; i < component.length(); i++) {
      char ch = component[i];
      if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.')) {
        return false;
      }
    }

    return true;
  }

  bool isValidPathname(const std::string& pathname, size_t maxLength) {
    // Check length
    if (pathname.empty() || pathname.length() > maxLength) {
      return false;
    }

    // Split into components and validate each
    std::istringstream iss(pathname);
    std::string        component;
    bool               first = true;

    while (std::getline(iss, component, '/')) {
      // Skip empty components (e.g., leading / or //)
      if (component.empty()) {
        // Leading '/' is OK for first component
        if (first && pathname[0] == '/') {
          first = false;
          continue;
        }
        // Otherwise, empty components (like //) are invalid
        if (!first) {
          return false;
        }
      } else {
        // Validate component
        if (!isValidComponent(component)) {
          return false;
        }
      }
      first = false;
    }

    return true;
  }

  std::string resolveFullPath(const std::string& pathname, const std::string& prefix) {
    std::string fullPath;

    // If pathname starts with '/', it's already a full path
    if (!pathname.empty() && pathname[0] == '/') {
      fullPath = pathname;
    } else {
      // Partial path - prepend prefix
      fullPath = prefix;
      if (!fullPath.empty() && fullPath.back() != '/' && !pathname.empty()) {
        fullPath += '/';
      }
      fullPath += pathname;
    }

    // Check if result exceeds maximum
    if (fullPath.length() > 128) {
      return "";  // Invalid - too long
    }

    return fullPath;
  }

  std::filesystem::path mapToHostPath(const std::string&           prodosPath,
                                      const std::filesystem::path& volumesRoot) {
    // Defensive check: require leading '/' (absolute ProDOS path)
    if (prodosPath.empty() || prodosPath[0] != '/') {
      // Return empty path to indicate error
      return std::filesystem::path();
    }

    // Defensive check: reject '.' or '..' segments
    // Split and check each component
    std::istringstream iss(prodosPath);
    std::string        component;
    bool               first = true;

    while (std::getline(iss, component, '/')) {
      // Skip empty components (leading '/')
      if (component.empty() && first) {
        first = false;
        continue;
      }

      // Reject '.' and '..' segments
      if (component == "." || component == "..") {
        return std::filesystem::path();
      }

      first = false;
    }

    // Remove leading '/'
    std::string relativePath = prodosPath.substr(1);

    return volumesRoot / relativePath;
  }

}  // namespace prodos8emu
