#pragma once

#include <cstdint>
#include <string_view>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdexcept>
#include <format>
#include "base.hpp"
#include "./util/logger.hpp"

#if !IS_MINGW
#include <linux/limits.h>
#endif


namespace fs {


inline char* realpath (const std::string_view& path, char* resolved_path) {
    #if IS_MINGW
    const char* const res = _fullpath(resolved_path, path.data(), PATH_MAX);
    #else
    char* res = ::realpath(path.data(), resolved_path);
    #endif
    if (res == nullptr) {
        throw std::runtime_error{std::format("could not get real path for {}, ERRNO: {}", path, errno)};
    }
    return res;
}

inline std::string realpath (const std::string_view& path) {
    return std::string{realpath(path, static_cast<char*>(alloca(PATH_MAX)))};
}

#ifdef O_NONBLOCK
int set_nonblocking (int fd) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        throw std::runtime_error{std::format("fcntl could not get file flags, ERRNO: {}", errno)};
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif

struct OpenWithStatsResult {
    int fd;
    struct stat stat;
};

inline struct stat get_stat (const std::string_view& path, int fd) {
    struct stat stat{};
    if (fstat(fd, &stat) != 0) {
        throw std::runtime_error{std::format("could not get file status for {}, ERRNO: {}", path, errno)};
    }
    return stat;
}

inline void fail_negative_fd (const std::string_view& path, int fd)  {
    if (fd < 0) {
        throw std::runtime_error{std::format("could not open file {}, ERRNO: {}\n", path, errno)};
    }
}


template <int flags>
inline OpenWithStatsResult open_with_stat (const std::string_view& path, uint16_t create_flags){
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int fd = open(path.data(), flags, create_flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};
template <int flags>
inline OpenWithStatsResult open_with_stat (const std::string_view& path) {
    if constexpr (flags & O_CREAT) {
        static_warn("File will be created but creating flags are not set");
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int fd = open(path.data(), flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};

inline OpenWithStatsResult open_with_stat (const std::string_view& path, int flags) {
    if ((flags & O_CREAT) != 0) {
        logger::warn("File will be created but creating flags are not set!");
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int fd = open(path.data(), flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};
inline OpenWithStatsResult open_with_stat (const std::string_view& path, int flags, uint16_t create_flags) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int fd = open(path.data(), flags, create_flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};




inline void throw_not_regular (const std::string_view& path, const struct stat& stat) {
    if (!S_ISREG(stat.st_mode)) {
        throw std::runtime_error{std::format("file {} is not a regular file", path)};
    }
}

inline void throw_not_regular (const struct stat& stat) {
    if (!S_ISREG(stat.st_mode)) {
        throw std::runtime_error{"not a regular file"};
    }
}


}