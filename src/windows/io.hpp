#pragma once

#include "../base.hpp"

#if IS_MINGW

#include <cstddef>
#include <cstdio>
#include <format>
#include <print>
#include <type_traits>
#include <utility>
#include "../estd/empty.hpp"
#include "./string_literal.hpp"


// #include <_mingw_mac.h>
// #include <handleapi.h>
// #include <minwinbase.h>
// #include <minwindef.h>
// #include <winnt.h>
// #include <windows.h>


// These are simple wrappers which are only meant to be used for this project. I will extend features as needed
namespace windows::io {

    namespace {
        template <bool use_exceptions>
        requires (use_exceptions)
        void throw_error (const std::string& message) {
            throw std::runtime_error{message};
        }

        template <bool use_exceptions>
        requires (!use_exceptions)
        [[noreturn]] void throw_error (const std::string_view& message) noexcept {
            std::println("{}", message.data());
            std::exit(1);
        }
    }


/* Optional non-type template parameter */
class optional_nttp_t {};

constexpr optional_nttp_t optional_template_parameter {};

template <typename T, template <typename TT> typename satisfies_non_optional>
concept optional_nttp_or_satisfies =
    std::is_same_v<T, optional_nttp_t>
    || satisfies_non_optional<T>::value;


template <
    auto _file_name,
    DWORD _desired_access,
    DWORD _share_mode,
    DWORD _creation_disposition,
    DWORD _flags_and_attributes
>
requires(optional_nttp_or_satisfies<decltype(_file_name), is_string_literal_t>)
class CreateFileParams {
    public:
    static constexpr auto file_name = _file_name;
    static constexpr DWORD desired_access = _desired_access;
    static constexpr DWORD share_mode = _share_mode;
    static constexpr DWORD creation_disposition = _creation_disposition;
    static constexpr DWORD flags_and_attributes = _flags_and_attributes;

    static constexpr bool has_file_name = is_string_literal_v<decltype(_file_name)>;
};

template <typename Params>
concept CreateFileParamsConcept =
    std::is_same_v<std::remove_cvref_t<decltype(Params::desired_access)>, DWORD> &&
    std::is_same_v<std::remove_cvref_t<decltype(Params::share_mode)>, DWORD> &&
    std::is_same_v<std::remove_cvref_t<decltype(Params::creation_disposition)>, DWORD> &&
    std::is_same_v<std::remove_cvref_t<decltype(Params::flags_and_attributes)>, DWORD>;

template <
    StringLiteral _file_name,
    DWORD _desired_access,
    DWORD _share_mode,
    DWORD _creation_disposition,
    DWORD _flags_and_attributes
>
class CreateFileParamsWithName : public CreateFileParams<_file_name, _desired_access, _share_mode, _creation_disposition, _flags_and_attributes> {};

template <typename Params>
concept CreateFileParamsWithNameConcept = CreateFileParamsConcept<Params> && is_string_literal_v<std::remove_cvref_t<decltype(Params::file_name)>>;


template <CreateFileParamsConcept Params>
class _FileHandle {
    public:

    inline _FileHandle (
        LPSECURITY_ATTRIBUTES security_attributes,
        HANDLE template_file
    )
    :
    value (
        CreateFileA(
            Params::file_name.data,
            Params::desired_access,
            Params::share_mode,
            std::move(security_attributes),
            Params::creation_disposition,
            Params::flags_and_attributes,
            std::move(template_file)
        )
    )
    {}

    template <bool use_exceptions = false>
    inline void thorw_if_invalid () {
        if (this->value == INVALID_HANDLE_VALUE) [[unlikely]] {
            throw_error<use_exceptions>("[_FileHandle::thorw_if_invalid] Invalid handle.");
        }
    }

    inline constexpr _FileHandle (HANDLE&& value) : value(std::move(value)) {}
    inline constexpr _FileHandle (const HANDLE& value) : value(value) {}

    const HANDLE value;
    typedef Params params;
};

template <CreateFileParamsConcept Params>
class FileHandle;

template <CreateFileParamsConcept Params>
class UncheckedFileHandle : public _FileHandle<Params> {
    public:

    using _FileHandle<Params>::_FileHandle;

    template <bool use_exceptions = false>
    inline FileHandle<Params> check () const noexcept(!use_exceptions) {
        this->template thorw_if_invalid<use_exceptions>();
        return {std::move(*this)};
    }

    friend class FileWriter;
    friend class AsyncFileWriter;
};

template <CreateFileParamsConcept Params>
class FileHandle : public _FileHandle<Params> {
    public:
    template <bool use_exceptions = false>
    inline FileHandle (
        LPSECURITY_ATTRIBUTES security_attributes,
        HANDLE template_file
    ) : 
    _FileHandle<Params>(security_attributes, template_file)
    {
        this->template thorw_if_invalid<use_exceptions>();
    }

    inline UncheckedFileHandle<Params> as_unchecked () const {
        return {std::move(*this)};
    }

    friend class FileWriter;
    friend class AsyncFileWriter;
};



class WriteFileResult {
    private:
    WINBOOL _success;

    protected:
    constexpr WriteFileResult (WINBOOL success) : _success(success) {}

    public:
    inline bool success () const { return _success; }

    friend class FileWriter;
    friend class AsyncFileWriter;
};

class WriteFileResultWithBytesWritten : public WriteFileResult {

    constexpr WriteFileResultWithBytesWritten (WINBOOL success, DWORD bytes_written) : WriteFileResult(success), bytes_written(bytes_written) {}

    public:
    DWORD bytes_written;

    friend class FileWriter;
    friend class AsyncFileWriter;
};



class FileWriter {
    private:
    const HANDLE handle;
    public:

    template <CreateFileParamsConcept Params>
    constexpr FileWriter (_FileHandle<Params> file_handle) : handle(file_handle.value)
    {
        static_assert((Params::desired_access & GENERIC_WRITE) != 0, "FileWriter: GENERIC_WRITE is required");
        if constexpr ((Params::share_mode & FILE_SHARE_WRITE) == 0) {
            static_warn("[FileWriter::FileWriter] FILE_SHARE_WRITE is not set. This is only of concern if multiple threads are writing to the same file.");
        }
        if constexpr ((Params::flags_and_attributes & FILE_FLAG_OVERLAPPED) != 0) {
            static_warn("[FileWriter::FileWriter] FILE_FLAG_OVERLAPPED is set. You might want to use FileWriterAsync instead.");
        }
    }

    template <bool get_bytes_written = false>
    inline std::conditional_t<get_bytes_written, WriteFileResultWithBytesWritten, WriteFileResult> write (LPCVOID src, DWORD size) const {
        DWORD bytes_written;
        WINBOOL success = WriteFile(
            handle,
            src,
            size,
            &bytes_written,
            nullptr   
        );
        if constexpr (get_bytes_written) {
            return WriteFileResultWithBytesWritten{success, bytes_written};
        } else {
            return WriteFileResult{success};
        }
    }


    template <bool get_bytes_written = false, bool use_exceptions = false>
    inline std::conditional_t<get_bytes_written, DWORD, void> write_handled (LPCVOID src, DWORD size) const noexcept(!use_exceptions) {
        auto write_result = write<get_bytes_written>(src, size);
        if (!write_result.success()) [[unlikely]] {
            throw_error<use_exceptions>(std::format("[FileWriter::write] WriteFile failed: {}", GetLastError()));
        }
        if constexpr (get_bytes_written) {
            return write_result.bytes_written;
        }
    }

};

class AsyncFileWriter {
    private:
    const HANDLE handle;
    OVERLAPPED overlapped;

    template <CreateFileParamsConcept Params>
    inline AsyncFileWriter (const _FileHandle<Params>& file_handle, estd::empty) : handle(file_handle.value) {
        static_assert((Params::desired_access & GENERIC_WRITE) != 0, "AsyncFileWriter: GENERIC_WRITE is required");
        static_assert((Params::flags_and_attributes & FILE_FLAG_OVERLAPPED) != 0, "AsyncFileWriter: FILE_FLAG_OVERLAPPED is required");

        overlapped = {
            0,
            0,
            {0, 0},
            CreateEventA(NULL, FALSE, TRUE, NULL),
        };
    }

    public:
    template <CreateFileParamsConcept Params>
    inline AsyncFileWriter (const FileHandle<Params>& file_handle) : AsyncFileWriter(file_handle, {}) {}

    template <CreateFileParamsConcept Params>
    inline AsyncFileWriter (const UncheckedFileHandle<Params>& file_handle) : AsyncFileWriter(file_handle, {}) {
        static_warn("AsyncFileWriter created from unchecked handle.")
    }

    template <bool use_exceptions = false>
    inline void wait_handled (DWORD timeout = INFINITE) const noexcept(!use_exceptions) {
        DWORD wait_result = WaitForSingleObject(overlapped.hEvent, timeout);
        switch (wait_result) {
            [[likely]] case WAIT_OBJECT_0:
                return;
            [[unlikely]] case WAIT_ABANDONED:
                throw_error<use_exceptions>("[AsyncFileWriter::wait] WaitForSingleObject returned WAIT_ABANDONED.");
            [[unlikely]] case WAIT_TIMEOUT:
                throw_error<use_exceptions>("[AsyncFileWriter::wait] WaitForSingleObject timed out.");
            [[unlikely]] case WAIT_FAILED:
                throw_error<use_exceptions>("[AsyncFileWriter::wait] WaitForSingleObject failed.");
            [[unlikely]] default:
                throw_error<use_exceptions>("[AsyncFileWriter::wait] WaitForSingleObject returned unexpected value.");
        }
    }

    enum WAIT_FOR_SINGLE_OBJECT_RESULT : DWORD {
        _WAIT_OBJECT_0 = WAIT_OBJECT_0,
        _WAIT_ABANDONED = WAIT_ABANDONED,
        _WAIT_TIMEOUT = WAIT_TIMEOUT,
        _WAIT_FAILED = WAIT_FAILED,
        _WAIT_IO_COMPLETION = WAIT_IO_COMPLETION,
    };

    inline WAIT_FOR_SINGLE_OBJECT_RESULT wait (DWORD timeout = INFINITE) const {
        return static_cast<WAIT_FOR_SINGLE_OBJECT_RESULT>(WaitForSingleObject(overlapped.hEvent, timeout));
    }


    template <bool get_bytes_written = false>
    requires (get_bytes_written)
    inline WriteFileResultWithBytesWritten write (LPCVOID src, DWORD size) {
        DWORD bytes_written;
        WINBOOL success = WriteFile(
            handle,
            src,
            size,
            &bytes_written,
            &overlapped   
        );
        return {success, bytes_written};
    }

    template <bool get_bytes_written = false>
    requires (!get_bytes_written)
    inline WriteFileResult write (LPCVOID src, DWORD size) {
        WINBOOL success = WriteFile(
            handle,
            src,
            size,
            nullptr,
            &overlapped   
        );
        return {success};
    }

    template <bool get_bytes_written = false, bool use_exceptions = false>
    inline std::conditional_t<get_bytes_written, DWORD, void> write_handled (LPCVOID src, DWORD size, DWORD wait_timeout = INFINITE) noexcept(!use_exceptions) {
        wait_handled<use_exceptions>(wait_timeout);
        auto write_result = write<get_bytes_written>(src, size);
        if (!write_result.success()) {
            DWORD last_error = GetLastError();
            if (last_error != ERROR_IO_PENDING) {
                throw_error<use_exceptions>(std::format("[AsyncFileWriter::write] WriteFile failed: {}", last_error));
            }
        }
        if constexpr (get_bytes_written) {
            return write_result.bytes_written;
        }
    }
};

}

#else

#error "Windows only Header"

#endif