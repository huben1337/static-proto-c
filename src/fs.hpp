#pragma once

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <gsl/util>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <alloca.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "base.hpp"
#include "./util/logger.hpp"
#include "estd/class_constraints.hpp"
#include "estd/empty.hpp"
#include "estd/utility.hpp"
#include "nameof.hpp"
#include "util/string_literal.hpp"

#if !IS_MINGW
#include <linux/limits.h>
#endif


namespace fs {

enum class OPEN_FLAGS {
    ACCMODE = O_ACCMODE,
    RDONLY = O_RDONLY,
    WRONLY = O_WRONLY,
    RDWR = O_RDWR,
    CREAT = O_CREAT,
    EXCL = O_EXCL,
    NOCTTY = O_NOCTTY,
    TRUNC = O_TRUNC,
    APPEND = O_APPEND,
    NONBLOCK = O_NONBLOCK,
    NDELAY = O_NDELAY,
    SYNC = O_SYNC,
    FSYNC = O_FSYNC,
    ASYNC = O_ASYNC,
    LARGEFILE = __O_LARGEFILE,
    DIRECTORY = __O_DIRECTORY,
    NOFOLLOW = __O_NOFOLLOW,
    CLOEXEC = __O_CLOEXEC,
    DIRECT = __O_DIRECT,
    NOATIME = __O_NOATIME,
    PATH = __O_PATH,
    DSYNC = __O_DSYNC,
    TMPFILE = __O_TMPFILE,
};

enum class PERMISSION_MODE : uint16_t {
    IFMT = S_IFMT,
    IFDIR = S_IFDIR,
    IFCHR = S_IFCHR,
    IFBLK = S_IFBLK,
    IFREG = S_IFREG,

    # ifdef S_IFIFO
    IFIFO = S_IFIFO,
    # endif

    # ifdef S_IFLNK
    IFLNK = S_IFLNK,
    # endif

    # ifdef S_IFSOCK
    FSOCK = S_IFSOCK,
    # endif

    ISUID = S_ISUID, /* Set user ID on execution.  */
    ISGID = S_ISGID, /* Set group ID on execution.  */

    # ifdef S_ISVTX
    ISVTX = S_ISVTX, /* Save swapped text after use (sticky bit).  This is pretty well obsolete.  */
    # endif

    IRUSR = S_IRUSR, /* Read by owner.  */
    IWUSR = S_IWUSR, /* Write by owner.  */
    IXUSR = S_IXUSR, /* Execute by owner.  */
    IRWXU = S_IRWXU, /* Read, write, and execute by owner.  */
    IRGRP = S_IRGRP, /* Read by group.  */
    IWGRP = S_IWGRP, /* Write by group.  */
    IXGRP = S_IXGRP, /* Execute by group.  */
    IRWXG = S_IRWXG, /* Read, write, and execute by group.  */

    IROTH = S_IROTH, /* Read by others.  */
    IWOTH = S_IWOTH, /* Write by others.  */
    IXOTH = S_IXOTH, /* Execute by others.  */
    IRWXO = S_IRWXO, /* Read, write, and execute by others.  */
};

template<OPEN_FLAGS flag>
struct IsRWFlag {
    static constexpr bool value =
        flag == OPEN_FLAGS::RDONLY ||
        flag == OPEN_FLAGS::WRONLY ||
        flag == OPEN_FLAGS::RDWR;
};

enum class CLOSE_ERROR : uint8_t {
    NONE = 0,
    BADF = EBADF,
    INT = EINTR,
    IO = EIO,
    NOSPC = ENOSPC,
    DQUOT = EDQUOT
};

enum class OPEN_ERROR : uint8_t {
    NONE = 0,
    ACCES = EACCES,
    BADF = EBADF,
    BUSY = EBUSY,
    DQUOT = EDQUOT,
    EXIST = EEXIST,
    FAULT = EFAULT,
    FBIG = EFBIG,
    INTR = EINTR,
    INVAL = EINVAL,
    ISDIR = EISDIR,
    LOOP = ELOOP,
    MFILE = EMFILE,
    NAMETOOLONG = ENAMETOOLONG,
    NFILE = ENFILE,
    NODEV = ENODEV,
    NOENT = ENOENT,
    NOMEM = ENOMEM,
    NOSPC = ENOSPC,
    NOTDIR = ENOTDIR,
    NXIO = ENXIO,
    OPNOTSUPP = EOPNOTSUPP,
    OVERFLOW = EOVERFLOW,
    PERM = EPERM,
    ROFS = EROFS,
    TXTBSY = ETXTBSY,
    WOULDBLOCK = EWOULDBLOCK,
};

enum STAT_ERROR : uint8_t {
    NONE = 0,
    BADF = EBADF,
    IO = EIO,
    OVERFLOW = EOVERFLOW,
};

template <typename Data = std::string>
struct FailErrorHandler {
    Data data;
};

namespace _detail {
    template <bool is_callable, typename Func, typename... Args>
    constexpr bool returns_void_with = false;
    template <typename Func, typename... Args>
    constexpr bool returns_void_with<true, Func, Args...> = std::is_same_v<void, decltype(std::declval<Func>()(std::declval<Args>()...))>;

    struct passthrough {
        template <typename T>
        [[nodiscard]] T operator()(T&& t) const {
            return std::forward<T>(t);
        }
    };

    template <typename Value, typename Error>
    struct to_expected {
        template <typename T>
        [[nodiscard]] auto operator()(T&& t) const {
            return std::expected<Value, Error>{std::forward<T>(t)};
        }
    };

    template <estd::discouraged_annotation, OPEN_FLAGS... flags, PERMISSION_MODE... permission_modes>
    [[nodiscard]] inline int open_direct (const std::string& path, estd::variadic_v<flags...> /*unused*/ = {}, estd::variadic_v<permission_modes...> /*unused*/ = {}) {
        using rwflags = estd::variadic_v_where<IsRWFlag>::apply<flags...>;
        static_assert(rwflags::size > 0, "missing read/write flag");
        static_assert(rwflags::template apply<estd::are_same_variadic_v<>::check>::value, "conflicting read/write flags");
        constexpr int combined_flags = (... | static_cast<int>(flags));
        static_assert(
            ((combined_flags & static_cast<int>(OPEN_FLAGS::CREAT)) == 0) || sizeof...(permission_modes) > 0,
            "File will be created but creating flags are not set");
        if constexpr (sizeof...(permission_modes) > 0) {
            constexpr uint16_t combined_modes = (... | static_cast<uint16_t>(permission_modes));
            return ::open(path.c_str(), combined_flags, combined_modes);
        } else {
            return ::open(path.c_str(), combined_flags);
        }
    }

    template <estd::discouraged_annotation>
    [[nodiscard]] CLOSE_ERROR direct_close (const int fd) {
        const int close_result = ::close(fd);
        if (close_result == 0) return CLOSE_ERROR::NONE;
        return static_cast<CLOSE_ERROR>(errno);
    }
}

struct File {
    friend struct UncheckedFile;
    static constexpr int empty_fd = -1;
private:
    int _fd = empty_fd;

    constexpr explicit File(const int fd) : _fd(fd) {}

public:
    template <typename InvalidHandler = estd::empty, typename ValidHandler = estd::empty, OPEN_FLAGS... flags, PERMISSION_MODE... permission_modes>
    [[nodiscard]] static decltype(auto) open (
        const std::string& path,
        estd::variadic_v<flags...> /*unused*/ = {},
        estd::variadic_v<permission_modes...> /*unused*/ = {},
        InvalidHandler&& on_invalid = {},
        ValidHandler&& on_valid = {}
    ) {
        const int result = _detail::open_direct<estd::discouraged, flags...>(path, {}, estd::variadic_v<permission_modes...>{});
        if (result >= 0) {
            if constexpr (std::is_same_v<estd::empty, ValidHandler>) {
                return File{result};
            } else {
                return std::forward<ValidHandler>(on_valid)(File{result}, path);
            }
        }
        if constexpr (std::is_same_v<estd::empty, InvalidHandler>) {
            std::perror("[File::open] failed.");
            std::exit(1);
        } else {
            if constexpr (_detail::returns_void_with<true, InvalidHandler, OPEN_ERROR, const std::string&>) {
                std::forward<InvalidHandler>(on_invalid)(static_cast<OPEN_ERROR>(errno), path);
                std::exit(1);
            } else {
                return std::forward<InvalidHandler>(on_invalid)(static_cast<OPEN_ERROR>(errno), path);
            }
        }
    }

    constexpr void reset () {
        _fd = empty_fd;
    }

    [[nodiscard]] constexpr bool has_fd () const {
        return _fd >= 0;
    }

    File(const File&) = delete;

    File& operator=(const File&) = delete;

    constexpr File(File&& other) : _fd(other._fd) {
        other.reset();
    }

    [[nodiscard]] constexpr File& operator= (this File& self, File&& other) {
        if (&self == &other) return self;

        const CLOSE_ERROR close_error = _detail::direct_close<estd::discouraged>(self._fd);
        if (close_error != CLOSE_ERROR::NONE) {
            std::perror("[File.operator=] failed to close file.");
        }

        self._fd = other._fd;
        other.reset();
        
        return self;      
    }

    [[nodiscard]] constexpr CLOSE_ERROR close () {
        if (!has_fd()) return CLOSE_ERROR::NONE;
        const CLOSE_ERROR result = _detail::direct_close<estd::discouraged>(_fd);
        reset();
        return result;
    }
    
    constexpr ~File () {
        const CLOSE_ERROR close_error = close();
        if (close_error == CLOSE_ERROR::NONE) return;
        std::perror("[File.~File] failed to close file.");
    }

    [[nodiscard]] ssize_t write (const void* const buf, size_t nbytes) const {
        return ::write(_fd, buf, nbytes);
    }

    [[nodiscard]] ssize_t read (void* const buf, size_t nbytes) const {
        return ::read(_fd, buf, nbytes);
    }

    template <
        typename OperationProvider,
        typename Data,
        typename Error,
        typename ErrorHandler,
        typename SuccessHandler,
        typename... Args
    >
    [[nodiscard]] auto handled_operation (
        ErrorHandler&& error_handler,
        SuccessHandler&& success_handler,
        Args&&... args
    ) const {
        constexpr bool custom_error_handler = !std::is_same_v<estd::empty, ErrorHandler>;
        constexpr bool custom_success_handler = !std::is_same_v<estd::empty, SuccessHandler>;
        constexpr bool noexit_on_error =
            _detail::returns_void_with<custom_error_handler, ErrorHandler, Error>;

        Data data;
        auto result = OperationProvider::execute(data, std::forward<Args>(args)...);
        if (OperationProvider::is_success(result)) {
            if constexpr (custom_success_handler) {
                return std::forward<SuccessHandler>(success_handler)(std::move(data));
            } else {
                if constexpr (noexit_on_error) {
                    return std::move(data);
                } else {
                    return std::expected<Data, Error>{std::move(data)};
                }
            }
        }
        if constexpr (custom_error_handler) {
            if constexpr (noexit_on_error) {
                std::forward<ErrorHandler>(error_handler)(static_cast<Error>(errno));
                std::exit(1);
            } else {
                return std::forward<ErrorHandler>(error_handler)(static_cast<Error>(errno));
                
            }
        } else {
            return std::expected<Data, Error>{static_cast<Error>(errno)};
        }
        
    }

    template <
        typename ErrorHandler = estd::empty,
        typename SuccessHandler = estd::empty
    >
    [[nodiscard]] decltype(auto) stat (
        ErrorHandler&& error_handler = {},
        SuccessHandler&& success_handler = {}
    ) const {
        struct StatOperation {
            [[nodiscard]] static int execute (struct ::stat& stats, const int fd) {
                return fstat(fd, &stats);
            }
            [[nodiscard]] static bool is_success (const int result) {
                return result == 0;
            }
        };
        return handled_operation<StatOperation, struct ::stat, STAT_ERROR>(
            std::forward<ErrorHandler>(error_handler),
            std::forward<SuccessHandler>(success_handler),
            _fd
        );
    }

    /* template <
        typename ErrorHandler = estd::empty,
        typename SuccessHandler = estd::empty
    >
    [[nodiscard]] auto stat (
        ErrorHandler&& error_handler = {},
        SuccessHandler&& success_handler = {}) const {
        constexpr bool custom_error_handler = !std::is_same_v<estd::empty, ErrorHandler>;
        constexpr bool custom_success_handler = !std::is_same_v<estd::empty, SuccessHandler>;
        constexpr bool noexit_on_error =
            _detail::returns_void_with<custom_error_handler, ErrorHandler, STAT_ERROR>;

        struct ::stat stat{};
        if (::fstat(_fd, &stat)  == 0) {
            if constexpr (custom_success_handler) {
                return std::forward<SuccessHandler>(success_handler)(std::move(stat));
            } else {
                if constexpr (noexit_on_error) {
                    return stat;
                } else {
                    return std::expected<struct ::stat, STAT_ERROR>{stat};
                }
            }
        }
        if constexpr (custom_error_handler) {
            if constexpr (noexit_on_error) {
                std::forward<ErrorHandler>(error_handler)(static_cast<STAT_ERROR>(errno));
                std::exit(1);
            } else {
                return std::forward<ErrorHandler>(error_handler)(static_cast<STAT_ERROR>(errno));
                
            }
        } else {
            return std::expected<struct ::stat, STAT_ERROR>{static_cast<STAT_ERROR>(errno)};
        }
        
    } */
};

namespace _detail {
    [[nodiscard]] static constexpr int errno_to_data (const OPEN_ERROR e) {
        return -static_cast<int>(e) - 1;
    }

    [[nodiscard]] static constexpr OPEN_ERROR data_to_errno (const int data) {
        return gsl::narrow_cast<OPEN_ERROR>(-data - 1);
    }
}

struct UncheckedFile {
private:
    static constexpr int empty_fd = _detail::errno_to_data(OPEN_ERROR::NONE);

    static_assert(empty_fd < 0, "Empty file descriptor must not be valid file descriptor");

    int _data = empty_fd;

    constexpr explicit UncheckedFile(const int data) : _data(data) {}
public:
    consteval UncheckedFile() = default;

    UncheckedFile(const UncheckedFile&) = delete;

    UncheckedFile& operator=(const UncheckedFile&) = delete;

    constexpr UncheckedFile(UncheckedFile&& other) : _data(other._data) {
        other.reset();
    }

    [[nodiscard]] constexpr UncheckedFile& operator= (this UncheckedFile& self, UncheckedFile&& other) {
        if (&self == &other) return self;

        if (self.has_fd()) {
            const CLOSE_ERROR close_error = _detail::direct_close<estd::discouraged>(self._fd());
            if (close_error != CLOSE_ERROR::NONE) {
                std::perror("[UncheckedFile.operator=] failed to close file.");
            }
        }

        self._data = other._data;
        other.reset();
        
        return self;      
    }

    template <OPEN_FLAGS... flags, PERMISSION_MODE... permission_modes>
    [[nodiscard]] static UncheckedFile open (
        const std::string& path,
        estd::variadic_v<flags...> /*unused*/ = {},
        estd::variadic_v<permission_modes...> /*unused*/ = {}
    ) {
        const int result = _detail::open_direct<estd::discouraged, flags..., permission_modes...>(path);
        if (result >= 0) return UncheckedFile{result};
        return UncheckedFile{_detail::errno_to_data(static_cast<OPEN_ERROR>(errno))};
    }

    constexpr void reset () {
        _data = empty_fd;
    }

    [[nodiscard]] constexpr const int& _fd () const {
        return _data;
    }

    [[nodiscard]] constexpr bool has_fd () const {
        return _data >= 0;
    }

    [[nodiscard]] constexpr bool has_error () const {
        return _data < empty_fd;
    }

    [[nodiscard]] constexpr OPEN_ERROR get_error () const {
        if (!has_error()) return OPEN_ERROR::NONE;
        return gsl::narrow_cast<OPEN_ERROR>(_data);
    }

    [[nodiscard]] constexpr CLOSE_ERROR close () {
        if (!has_fd()) return CLOSE_ERROR::NONE;
        const CLOSE_ERROR result = _detail::direct_close<estd::discouraged>(_fd());
        reset();
        return result;
    }

    constexpr ~UncheckedFile () {
        const CLOSE_ERROR close_error = close();
        if (close_error == CLOSE_ERROR::NONE) return;
        std::perror("[UncheckedFile.~UncheckedFile] failed to close file.");
    }

    template <typename ValidHandler, typename InvalidHandler>
    [[nodiscard]] constexpr decltype(auto) check (ValidHandler&& on_valid, InvalidHandler&& on_invalid) && {
        if (has_fd()) {
            const int fd = _fd();
            reset();
            return std::forward<ValidHandler>(on_valid)(File{fd});
        }
        return std::forward<InvalidHandler>(on_invalid)();
    }

    template <estd::discouraged_annotation>
    [[nodiscard]] constexpr File to_checked () && {
        assert(has_fd());
        const int fd = _fd();
        reset();
        return File{fd};
    }
};

static_assert(!UncheckedFile{}.has_fd(), "Empty unchecked file must not have a file descrptor");
static_assert(!UncheckedFile{}.has_error(), "Empty unchecked file must not have an error");



inline char* realpath (const std::string& path, char* resolved_path) {
    #if IS_MINGW
    const char* const res = _fullpath(resolved_path, path.data(), PATH_MAX);
    #else
    char* res = ::realpath(path.data(), resolved_path);
    #endif
    if (res == nullptr) {
        console.error("could not get real path for: ", path, " ERRNO: ", errno);
        std::exit(1);
    }
    
    return res;
}

inline std::string realpath (const std::string& path) {
    return std::string{realpath(path, static_cast<char*>(alloca(PATH_MAX)))};
}

#ifdef O_NONBLOCK
[[nodiscard]] inline int set_nonblocking (int fd, int current_flags) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    return fcntl(fd, F_SETFL, current_flags | O_NONBLOCK);
}

[[nodiscard]] inline int set_nonblocking (int fd) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        console.error("fcntl could not get file flags, ERRNO: ", errno);
    }
    return set_nonblocking(fd, flags);
}
#endif

inline void assert_regular (const std::string_view& path, const struct ::stat& stat) {
    if (S_ISREG(stat.st_mode)) return;
    console.error("file ", path, " is not a regular file");
    std::exit(1);
}

}