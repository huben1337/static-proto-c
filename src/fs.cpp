#pragma once

#include <cstdint>
#include <string_view>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdexcept>
#include <format>
#include "base.cpp"
#include "logger.cpp"


namespace fs {

#ifdef __MINGW32__
INLINE void realpath (const std::string_view& path, std::array<char, PATH_MAX>& resolved_path) noexcept(false) {
    const char* res = _fullpath(resolved_path.data(), path.data(), PATH_MAX);
    if (!res) {
        throw std::runtime_error(std::format("could not get real path for {}, ERRNO: {}", path, errno));
    }
}

INLINE std::array<char, PATH_MAX> realpath (const std::string_view& path) noexcept(false) {
    std::array<char, PATH_MAX> resolved_path;
    const char* res = _fullpath(resolved_path.data(), path.data(), PATH_MAX);
    if (!res) {
        throw std::runtime_error(std::format("could not get real path for {}, ERRNO: {}", path, errno));
    }
    return resolved_path;
}
#endif

#ifdef O_NONBLOCK
void set_nonblocking (int fd) {
    int flags = fcntl(fd, F_GETFL, 0)
    if (flags == -1) {
        throw std::runtime_error(std::format("fcntl could not get file flags, ERRNO: {}", errno));
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif

struct OpenWithStatsResult {
    int fd;
    struct stat stat;
};

INLINE struct stat get_stat (const std::string_view& path, int fd) noexcept(false) {
    struct stat stat;
    if (fstat(fd, &stat) != 0) {
        throw std::runtime_error(std::format("could not get file status for {}, ERRNO: {}", path, errno));
    }
    return stat;
}

INLINE void fail_negative_fd (const std::string_view& path, int fd) noexcept(false)  {
    if (fd < 0) {
        throw std::runtime_error(std::format("could not open file {}, ERRNO: {}\n", path, errno));
    }
}


template <int flags>
inline OpenWithStatsResult open_with_stat (const std::string_view& path, uint16_t create_flags) noexcept(false) {
    int fd = open(path.data(), flags, create_flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};
template <int flags>
inline OpenWithStatsResult open_with_stat (const std::string_view& path) noexcept(false) {
    if constexpr (flags & O_CREAT) {
        static_warn("File will be created but creating flags are not set");
    }
    int fd = open(path.data(), flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};

INLINE OpenWithStatsResult open_with_stat (const std::string_view& path, int flags) noexcept(false) {
    if (flags & O_CREAT) {
        logger::warn("File will be created but creating flags are not set!");
    }
    int fd = open(path.data(), flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};
INLINE OpenWithStatsResult open_with_stat (const std::string_view& path, int flags, uint16_t create_flags) noexcept(false) {
    int fd = open(path.data(), flags, create_flags);
    fail_negative_fd(path, fd);
    return {fd, get_stat(path, fd)};
};




inline void throw_not_regular (const std::string_view& path, const struct stat& stat) noexcept(false) {
    if (!S_ISREG(stat.st_mode)) {
        throw std::runtime_error(std::format("file {} is not a regular file", path));
    }
}


}