#pragma once

#include <cerrno>
#include <cstdint>
#include <string_view>
#include <string>
#include <alloca.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "base.hpp"
#include "./util/logger.hpp"

#if !IS_MINGW
#include <linux/limits.h>
#endif


namespace fs {


inline char* realpath (const std::string& path, char* resolved_path) {
    #if IS_MINGW
    const char* const res = _fullpath(resolved_path, path.data(), PATH_MAX);
    #else
    char* res = ::realpath(path.data(), resolved_path);
    #endif
    if (res == nullptr) {
        logger::error("could not get real path for: ", path, " ERRNO: ", errno);
        std::exit(1);
    }
    return res;
}

inline std::string realpath (const std::string& path) {
    return std::string{realpath(path, static_cast<char*>(alloca(PATH_MAX)))};
}

#ifdef O_NONBLOCK
int set_nonblocking (int fd, int current_flags) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    return fcntl(fd, F_SETFL, current_flags | O_NONBLOCK);
}

int set_nonblocking (int fd) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        logger::error("fcntl could not get file flags, ERRNO: ", errno);
    }
    return set_nonblocking(fd, flags);
}
#endif

struct OpenWithStatsResult {
    int fd;
    struct stat stat;
};

inline struct stat get_stat (const std::string_view& path, int fd) {
    struct stat stat{};
    if (fstat(fd, &stat) != 0) {
        logger::error("could not get file status for: ", path, " ERRNO: ", errno);
        std::exit(1);
    }
    return stat;
}

inline void fail_negative_fd (const std::string_view& path, int fd)  {
    if (fd >= 0) return;
    logger::error("could not open file: ", path, " ERRNO: ", errno);
    std::exit(1);
}


template <int flags>
inline OpenWithStatsResult open_with_stat (const std::string& path, uint16_t create_flags){
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int fd = open(path.data(), flags, create_flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};
template <int flags>
inline OpenWithStatsResult open_with_stat (const std::string& path) {
    if constexpr (flags & O_CREAT) {
        static_warn("File will be created but creating flags are not set");
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int fd = open(path.data(), flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};

inline OpenWithStatsResult open_with_stat (const std::string& path, int flags) {
    if ((flags & O_CREAT) != 0) {
        logger::warn("File will be created but creating flags are not set!");
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int fd = open(path.data(), flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};
inline OpenWithStatsResult open_with_stat (const std::string& path, int flags, uint16_t create_flags) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int fd = open(path.data(), flags, create_flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};




inline void assert_regular (const std::string_view& path, const struct stat& stat) {
    if (S_ISREG(stat.st_mode)) return;
    logger::error("file ", path, " is not a regular file");
    std::exit(1);
}

}