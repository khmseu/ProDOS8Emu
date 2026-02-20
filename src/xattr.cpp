#include "prodos8emu/xattr.hpp"
#include "prodos8emu/errors.hpp"
#include <cerrno>
#include <cstring>

#ifdef __linux__
#include <sys/xattr.h>
#define HAVE_XATTR 1
#elif defined(__APPLE__)
#include <sys/xattr.h>
#define HAVE_XATTR 1
#else
#define HAVE_XATTR 0
#endif

namespace prodos8emu {

static std::string makeAttrName(const std::string& attrName) {
    return "user.prodos8." + attrName;
}

uint8_t prodos8_set_xattr(const std::string& path, const std::string& attrName, const std::string& value) {
#if HAVE_XATTR
    std::string fullName = makeAttrName(attrName);
    
#ifdef __APPLE__
    int result = setxattr(path.c_str(), fullName.c_str(), value.data(), value.size(), 0, 0);
#else
    int result = setxattr(path.c_str(), fullName.c_str(), value.data(), value.size(), 0);
#endif
    
    if (result == 0) {
        return ERR_NO_ERROR;
    }
    
    // Map errno to ProDOS error
    if (errno == EACCES || errno == EPERM) {
        return ERR_ACCESS_ERROR;
    } else if (errno == ENOSPC) {
        return ERR_VOLUME_FULL;
    } else if (errno == ENOTSUP || errno == EOPNOTSUPP) {
        return ERR_IO_ERROR;
    } else {
        return ERR_IO_ERROR;
    }
#else
    // xattr not available on this platform
    (void)path;
    (void)attrName;
    (void)value;
    return ERR_IO_ERROR;
#endif
}

uint8_t prodos8_get_xattr(const std::string& path, const std::string& attrName, std::string& value) {
#if HAVE_XATTR
    std::string fullName = makeAttrName(attrName);
    
    // First, get the size
#ifdef __APPLE__
    ssize_t size = getxattr(path.c_str(), fullName.c_str(), nullptr, 0, 0, 0);
#else
    ssize_t size = getxattr(path.c_str(), fullName.c_str(), nullptr, 0);
#endif
    
    if (size < 0) {
        if (errno == EACCES || errno == EPERM) {
            return ERR_ACCESS_ERROR;
        } else if (errno == ENOTSUP || errno == EOPNOTSUPP || errno == ENODATA) {
            return ERR_IO_ERROR;
        } else {
            return ERR_IO_ERROR;
        }
    }
    
    // Allocate and read
    value.resize(size);
#ifdef __APPLE__
    ssize_t actual = getxattr(path.c_str(), fullName.c_str(), &value[0], size, 0, 0);
#else
    ssize_t actual = getxattr(path.c_str(), fullName.c_str(), &value[0], size);
#endif
    
    if (actual < 0) {
        if (errno == EACCES || errno == EPERM) {
            return ERR_ACCESS_ERROR;
        } else {
            return ERR_IO_ERROR;
        }
    }
    
    value.resize(actual);
    return ERR_NO_ERROR;
#else
    // xattr not available on this platform
    (void)path;
    (void)attrName;
    (void)value;
    return ERR_IO_ERROR;
#endif
}

uint8_t prodos8_remove_xattr(const std::string& path, const std::string& attrName) {
#if HAVE_XATTR
    std::string fullName = makeAttrName(attrName);
    
#ifdef __APPLE__
    int result = removexattr(path.c_str(), fullName.c_str(), 0);
#else
    int result = removexattr(path.c_str(), fullName.c_str());
#endif
    
    if (result == 0) {
        return ERR_NO_ERROR;
    }
    
    // Map errno to ProDOS error
    if (errno == EACCES || errno == EPERM) {
        return ERR_ACCESS_ERROR;
    } else if (errno == ENOTSUP || errno == EOPNOTSUPP || errno == ENODATA) {
        return ERR_IO_ERROR;
    } else {
        return ERR_IO_ERROR;
    }
#else
    // xattr not available on this platform
    (void)path;
    (void)attrName;
    return ERR_IO_ERROR;
#endif
}

} // namespace prodos8emu
