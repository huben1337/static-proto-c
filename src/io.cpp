#pragma once
#include "base.cpp"
#include <_mingw_mac.h>
#include <cstddef>
#include <cstdio>
#include <format>
#include <handleapi.h>
#include <minwinbase.h>
#include <minwindef.h>
#include <type_traits>
#include <utility>
#include <winnt.h>
#include "helper_types.cpp"
#include "string_literal.cpp"

#ifdef __MINGW__
#include <windows.h>
#endif


// These are simple wrappers which are only meant to be used for this project. I will extend features as needed

namespace io {

namespace windows {

template <bool use_exceptions>
void throw_error (std::string message) requires(use_exceptions) {
    throw std::runtime_error(message);
}

template <bool use_exceptions>
[[noreturn]] void throw_error (std::string_view message) requires(!use_exceptions) {
    printf("%s\n", message.data());
    exit(1);
}

class optional_template_parameter_t {};

constexpr optional_template_parameter_t optional_template_parameter {};

template <typename T, template <typename TT> typename satisifies_non_optional>
concept optional_template_parameter_c =
    std::is_same_v<T, optional_template_parameter_t>
    || satisifies_non_optional<T>::value;


template <
    auto _file_name,
    DWORD _desired_access,
    DWORD _share_mode,
    DWORD _creation_disposition,
    DWORD _flags_and_attributes
>
requires(optional_template_parameter_c<decltype(_file_name), is_string_literal_t>)
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

    INLINE _FileHandle (
        LPSECURITY_ATTRIBUTES security_attributes,
        HANDLE template_file
    )
    :
    value (
        CreateFileA(
            Params::file_name.data(),
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
    INLINE void thorw_if_invalid () {
        if (this->value == INVALID_HANDLE_VALUE) [[unlikely]] {
            throw_error<use_exceptions>("FileHandle::check: Invalid handle");
        }
    }

    INLINE constexpr _FileHandle (const HANDLE&& value) : value(std::move(value)) {}
    INLINE constexpr _FileHandle (const HANDLE& value) : value(value) {}

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
    INLINE FileHandle<Params> check () const noexcept(!use_exceptions) {
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
    INLINE FileHandle (
        LPSECURITY_ATTRIBUTES security_attributes,
        HANDLE template_file
    ) : 
    _FileHandle<Params>(security_attributes, template_file)
    {
        this->template thorw_if_invalid<use_exceptions>();
    }

    INLINE UncheckedFileHandle<Params> as_unchecked () const {
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
    INLINE bool success () const { return _success; }

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
            static_warn("FileWriter: FILE_SHARE_WRITE is not set. This is only of concern if multiple threads are writing to the same file.");
        }
        if constexpr ((Params::flags_and_attributes & FILE_FLAG_OVERLAPPED) != 0) {
            static_warn("FileWriter: FILE_FLAG_OVERLAPPED is set. You might want to use FileWriterAsync instead.");
        }
    }

    template <bool get_bytes_written = false>
    INLINE std::conditional_t<get_bytes_written, WriteFileResultWithBytesWritten, WriteFileResult> write (LPCVOID src, DWORD size) const {
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
    INLINE std::conditional_t<get_bytes_written, DWORD, void> write_handled (LPCVOID src, DWORD size) const noexcept(!use_exceptions) {
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
    INLINE AsyncFileWriter (const _FileHandle<Params>& file_handle, Empty) : handle(file_handle.value) {
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
    INLINE AsyncFileWriter (const FileHandle<Params>& file_handle) : AsyncFileWriter(file_handle, {}) {}

    template <CreateFileParamsConcept Params>
    INLINE AsyncFileWriter (const UncheckedFileHandle<Params>& file_handle) : AsyncFileWriter(file_handle, {}) {
        static_warn("AsyncFileWriter created from unchecked handle.")
    }

    template <bool use_exceptions = false>
    INLINE void wait_handled (DWORD timeout = INFINITE) const noexcept(!use_exceptions) {
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

    INLINE WAIT_FOR_SINGLE_OBJECT_RESULT wait (DWORD timeout = INFINITE) const {
        return static_cast<WAIT_FOR_SINGLE_OBJECT_RESULT>(WaitForSingleObject(overlapped.hEvent, timeout));
    }


    template <bool get_bytes_written = false>
    INLINE WriteFileResultWithBytesWritten write (LPCVOID src, DWORD size) requires(get_bytes_written) {
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
    INLINE WriteFileResult write (LPCVOID src, DWORD size) requires(!get_bytes_written) {
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
    INLINE std::conditional_t<get_bytes_written, DWORD, void> write_handled (LPCVOID src, DWORD size, DWORD wait_timeout = INFINITE) noexcept(!use_exceptions) {
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

}