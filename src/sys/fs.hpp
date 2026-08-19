#pragma once

#include <cassert>
#include <cerrno>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <gsl/util>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/limits.h>

#include "../sys/errno.hpp"
#include "../estd/empty.hpp"
#include "../estd/utility.hpp"

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

namespace _detail {
    template<OPEN_FLAGS flag>
    struct IsRWFlag {
        static constexpr bool value =
            flag == OPEN_FLAGS::RDONLY ||
            flag == OPEN_FLAGS::WRONLY ||
            flag == OPEN_FLAGS::RDWR;
    };

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

        using result_t = std::expected<Value, Error>;

        template <typename T>
        [[nodiscard]] result_t operator()(T&& t) const {
            return result_t{std::unexpect_t{}, std::forward<T>(t)};
        }
    };

    template <typename T, typename Specific>
    struct CovariantError {
    private:
        using underlying_specific_t = std::underlying_type_t<Specific>;

        static_assert(
            std::numeric_limits<underlying_specific_t>::max() <= std::numeric_limits<T>::max() &&
            std::numeric_limits<underlying_specific_t>::min() >= std::numeric_limits<T>::min()
        );

        T _value;

    public:
        constexpr explicit CovariantError(auto value) : _value(gsl::narrow_cast<T>(value)) {}
        
        // NOLINTNEXTLINE(google-explicit-constructor)
        constexpr operator T() const { return _value; }

        // NOLINTNEXTLINE(google-explicit-constructor)
        constexpr operator Specific() const { return static_cast<Specific>(_value); }
        
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
    [[nodiscard]] sys::CLOSE_ERROR direct_close (const int fd) {
        const int close_result = ::close(fd);
        if (close_result == 0) return sys::CLOSE_ERROR::NONE;
        return gsl::narrow_cast<sys::CLOSE_ERROR>(errno);
    }

    template <
        typename ResultHandler,
        typename ErrorHandler,
        typename SuccessHandler,
        typename Result
    >
    [[nodiscard]] constexpr decltype(auto) handle_result (
        SuccessHandler&& success_handler,
        ErrorHandler&& error_handler,
        Result&& result
    ) {
        using Value = decltype(ResultHandler::get_value(std::forward<Result>(result)));
        using Error = decltype(ResultHandler::get_error(std::forward<Result>(result)));

        constexpr bool custom_success_handler = !std::is_same_v<estd::empty, SuccessHandler>;
        constexpr bool custom_error_handler = !std::is_same_v<estd::empty, ErrorHandler>;
        constexpr bool exit_on_error =
            !_detail::returns_void_with<custom_success_handler, SuccessHandler, Value&&> &&
            _detail::returns_void_with<custom_error_handler, ErrorHandler, Error&&>;

        if (ResultHandler::has_value(result)) {
            if constexpr (custom_success_handler) {
                return std::forward<SuccessHandler>(success_handler)(ResultHandler::get_value(std::forward<Result>(result)));
            } else {
                if constexpr (exit_on_error) {
                    return ResultHandler::get_value(std::forward<Result>(result));
                } else {
                    return std::expected<Value, Error>{ResultHandler::get_value(std::forward<Result>(result))};
                }
            }
        }

        if constexpr (custom_error_handler) {
            if constexpr (exit_on_error) {
                std::forward<ErrorHandler>(error_handler)(ResultHandler::get_error(std::forward<Result>(result)));
                std::exit(1);
            } else {
                return std::forward<ErrorHandler>(error_handler)(ResultHandler::get_error(std::forward<Result>(result)));
            }
        } else {
            return std::expected<Value, Error>{std::unexpect, ResultHandler::get_error(std::forward<Result>(result))};
        }
    }
}

template<std::signed_integral T, typename Error>
struct simple_operation_result {
    using unsigned_t = std::make_unsigned_t<T>;

    friend struct File;
    friend struct UncheckedFile;

private:
    T _data;

    constexpr explicit simple_operation_result(const T data) : _data(data) {}

    [[nodiscard]] static constexpr simple_operation_result from_result (const T result) {
        if (result >= 0) {
            return simple_operation_result{result};
        }

        const T e = -gsl::narrow_cast<T>(errno);
        return simple_operation_result{e};
    }

    [[nodiscard]] static constexpr simple_operation_result from_error (const Error error) {
        const T e = -gsl::narrow_cast<T>(error);
        assert(e < 0);
        return simple_operation_result{e};
    }
    

public:
    [[nodiscard]] constexpr bool operator==(const simple_operation_result& other) const {
        return _data == other._data;
    }

    template <
        typename ValueHandler = estd::empty,
        typename ErrorHandler = estd::empty
    >
    [[nodiscard]] constexpr decltype(auto) match (
        this const simple_operation_result& self,
        ValueHandler&& on_value = {},
        ErrorHandler&& on_error = {}
    ) {
        struct ResultHandler {
            [[nodiscard]] static constexpr bool has_value (const simple_operation_result& result) {
                return result._data >= 0;
            }

            [[nodiscard]] static constexpr T get_value (const simple_operation_result& result) {
                return result._data;
            }

            [[nodiscard]] static constexpr Error get_error (const simple_operation_result& result) {
                return gsl::narrow_cast<Error>(-result._data);
            }
        };

        return _detail::handle_result<ResultHandler>(
            std::forward<ValueHandler>(on_value),
            std::forward<ErrorHandler>(on_error),
            self
        );
    }
};

struct ReadError : _detail::CovariantError<uint8_t, sys::READ_ERROR> {
    using Base = CovariantError<uint8_t, sys::READ_ERROR>;
    using Base::Base;
};

struct WriteError : _detail::CovariantError<uint8_t, sys::WRITE_ERROR> {
    using Base = CovariantError<uint8_t, sys::WRITE_ERROR>;
    using Base::Base;
};

struct File {
    friend struct UncheckedFile;
    static constexpr int empty_fd = -1;
private:
    int _fd;

    constexpr explicit File(const int fd) : _fd(fd) {}

    constexpr void reset () {
        _fd = empty_fd;
    }

public:
    template <
        typename ErrorHandler = estd::empty,
        typename SuccessHandler = estd::empty,
        OPEN_FLAGS... flags,
        PERMISSION_MODE... permission_modes
    >
    [[nodiscard]] static decltype(auto) open (
        const std::string& path,
        estd::variadic_v<flags...> /*unused*/ = {},
        estd::variadic_v<permission_modes...> /*unused*/ = {},
        ErrorHandler&& error_handler = {},
        SuccessHandler&& success_handler = {}
    ) {
        struct OpenHandler {
            [[nodiscard]] static constexpr bool has_value (const int& result) {
                return result >= 0;
            }

            [[nodiscard]] static constexpr File get_value (const int& result) {
                return File{result};
            }

            [[nodiscard]] static constexpr sys::OPEN_ERROR get_error (const int& /*unused*/) {
                return static_cast<sys::OPEN_ERROR>(errno);
            }
        };

        return _detail::handle_result<OpenHandler>(
            std::forward<SuccessHandler>(success_handler),
            std::forward<ErrorHandler>(error_handler),
            _detail::open_direct<estd::discouraged, flags...>(path, {}, estd::variadic_v<permission_modes...>{})
        );
    }

    File(const File&) = delete;

    File& operator=(const File&) = delete;

    constexpr File(File&& other) : _fd(other._fd) {
        other.reset();
    }

    [[nodiscard]] constexpr File& operator= (File&& other) {
        if (_fd == other._fd) return *this;

        if (_fd >= 0) {
            const sys::CLOSE_ERROR close_error = _detail::direct_close<estd::discouraged>(_fd);
            if (close_error != sys::CLOSE_ERROR::NONE) {
                std::perror("[File.operator=] failed to close file.");
            }
        }

        _fd = other._fd;
        other.reset();
        
        return *this;      
    }

    [[nodiscard]] constexpr sys::CLOSE_ERROR close () {
        if (_fd < 0) return sys::CLOSE_ERROR::NONE;
        const sys::CLOSE_ERROR result = _detail::direct_close<estd::discouraged>(_fd);
        reset();
        return result;
    }
    
    constexpr ~File () {
        const sys::CLOSE_ERROR close_error = close();
        if (close_error == sys::CLOSE_ERROR::NONE) return;
        std::perror("[File.~File] failed to close file.");
    }

    [[nodiscard]] simple_operation_result<ssize_t, WriteError> write (const void* const buf, const size_t nbytes) const {
        return simple_operation_result<ssize_t, WriteError>::from_result(::write(_fd, buf, nbytes));
    }
        

    [[nodiscard]] simple_operation_result<ssize_t, ReadError> read (void* const buf, const size_t nbytes) const {
        return simple_operation_result<ssize_t, ReadError>::from_result(::read(_fd, buf, nbytes));
    }


    template <
        typename ErrorHandler = estd::empty,
        typename SuccessHandler = estd::empty
    >
    [[nodiscard]] decltype(auto) stat (
        ErrorHandler&& error_handler = {},
        SuccessHandler&& success_handler = {}
    ) const {
        struct StatsHandler {
            [[nodiscard]] static constexpr bool has_value (const std::pair<int, struct ::stat>& result) {
                return result.first == 0;
            }

            [[nodiscard]] static constexpr struct ::stat get_value (const std::pair<int, struct ::stat>& result) {
                return result.second;
            }

            [[nodiscard]] static constexpr sys::STAT_ERROR get_error (const std::pair<int, struct ::stat>& /*unused*/) {
                return static_cast<sys::STAT_ERROR>(errno);
            }
        };

        struct ::stat stats {};

        return _detail::handle_result<StatsHandler>(
            std::forward<SuccessHandler>(success_handler),
            std::forward<ErrorHandler>(error_handler),
            std::pair{fstat(_fd, &stats), stats}
        );
    }

    template <
        typename ErrorHandler = estd::empty,
        typename SuccessHandler = estd::empty
    >
    [[nodiscard]] decltype(auto) read (
        void* const buf,
        const size_t nbytes,
        ErrorHandler&& error_handler = {},
        SuccessHandler&& success_handler = {}
    ) const {
        struct ReadHandler {
            [[nodiscard]] static constexpr bool has_value (const ssize_t& result) {
                return result >= 0;
            }

            [[nodiscard]] static constexpr size_t get_value (const ssize_t& result) {
                return gsl::narrow_cast<size_t>(result);
            }

            [[nodiscard]] static constexpr ReadError get_error (const ssize_t& /*unused*/) {
                return ReadError{errno};
            }
        };

        return _detail::handle_result<ReadHandler>(
            std::forward<SuccessHandler>(success_handler),
            std::forward<ErrorHandler>(error_handler),
            ::read(_fd, buf, nbytes)
        );
    }

    template <
        typename ErrorHandler = estd::empty,
        typename SuccessHandler = estd::empty
    >
    [[nodiscard]] decltype(auto) write (
        const void* const buf,
        const size_t nbytes,
        ErrorHandler&& error_handler = {},
        SuccessHandler&& success_handler = {}
    ) const {
        struct WriteHandler {
            [[nodiscard]] static constexpr bool has_value (const ssize_t& result) {
                return result >= 0;
            }

            [[nodiscard]] static constexpr size_t get_value (const ssize_t& result) {
                return gsl::narrow_cast<size_t>(result);
            }

            [[nodiscard]] static constexpr WriteError get_error (const ssize_t& /*unused*/) {
                return WriteError{errno};
            }
        };

        return _detail::handle_result<WriteHandler>(
            std::forward<SuccessHandler>(success_handler),
            std::forward<ErrorHandler>(error_handler),
            ::write(_fd, buf, nbytes)
        );
    }
};

struct UncheckedFile {
    using result_t = simple_operation_result<int, sys::OPEN_ERROR>;
private:

    result_t _result;


    constexpr void reset () {
        _result = result_t::from_error(sys::OPEN_ERROR::NONE);
    }

    constexpr explicit UncheckedFile(const result_t result) : _result(result) {}
public:

    UncheckedFile(const UncheckedFile&) = delete;

    UncheckedFile& operator=(const UncheckedFile&) = delete;

    constexpr UncheckedFile(UncheckedFile&& other) : _result(other._result) {
        other.reset();
    }

    [[nodiscard]] constexpr UncheckedFile& operator= (UncheckedFile&& other) {
        if (_result == other._result) return *this;

        _result.match(
            [](const int& fd) {
                const sys::CLOSE_ERROR close_error = _detail::direct_close<estd::discouraged>(fd);
                if (close_error != sys::CLOSE_ERROR::NONE) {
                    std::perror("[UncheckedFile.operator=] failed to close file.");
                }
            },
            [](const sys::OPEN_ERROR) {}
        );

        _result = other._result;
        other.reset();
        
        return *this;      
    }

    template <OPEN_FLAGS... flags, PERMISSION_MODE... permission_modes>
    [[nodiscard]] static UncheckedFile open (
        const std::string& path,
        estd::variadic_v<flags...> /*unused*/ = {},
        estd::variadic_v<permission_modes...> /*unused*/ = {}
    ) {
        return UncheckedFile{result_t{
            _detail::open_direct<estd::discouraged, flags..., permission_modes...>(path)}};
    }

    [[nodiscard]] constexpr sys::CLOSE_ERROR close () {
        return _result.match(
            [this](const int& fd) {
                const sys::CLOSE_ERROR result = _detail::direct_close<estd::discouraged>(fd);
                reset();
                return result;
            },
            [](const sys::OPEN_ERROR) {
                return sys::CLOSE_ERROR::NONE;
            }
        );
    }

    constexpr ~UncheckedFile () {
        const sys::CLOSE_ERROR close_error = close();
        if (close_error == sys::CLOSE_ERROR::NONE) return;
        std::perror("[UncheckedFile.~UncheckedFile] failed to close file.");
    }

    template <typename ValidHandler, typename InvalidHandler>
    [[nodiscard]] constexpr decltype(auto) check (
        ValidHandler&& on_valid,
        InvalidHandler&& on_invalid
    ) && {
        return _result.match(
            [this, &on_valid](const int& fd) {
                reset();
                return std::forward<ValidHandler>(on_valid)(File{fd});
            },
            [&on_invalid](const sys::OPEN_ERROR) {
                return std::forward<InvalidHandler>(on_invalid)();
            }
        );
    }
};

static inline std::string realpath (const std::string& path) {
    static char path_buffer[PATH_MAX];

    char* const result = ::realpath(path.data(), path_buffer);
    if (result == nullptr) {
        std::perror("failed to get real path");
        std::exit(1);
    }

    return std::string{result};
}

constexpr bool is_regular (const struct ::stat& stat) {
    return S_ISREG(stat.st_mode);
}

}