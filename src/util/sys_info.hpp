#pragma once


#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <type_traits>
#include <utility>

#include "../base.hpp"
#include "../estd/bound.hpp"
#include "../estd/empty.hpp"
#include "string_literal.hpp"

#if IS_MINGW

#include <windows.h>

#else

#include <unistd.h>
#include <sys/sysinfo.h>
#include <linux/sysinfo.h>

#endif

namespace {

template <typename ReturnT, typename... ArgsT>
consteval ReturnT result_of_ (ReturnT(func)(ArgsT...)) {}
template <typename T>
using result_of_t = decltype(result_of_(std::declval<T>()));

using sysconf_result_bound = estd::bound<result_of_t<decltype(::sysconf)>>;

template <StringLiteral target_name, typename BoundT = estd::empty>
auto sysconf_checked (decltype(_SC_PAGESIZE) name, BoundT&& bound = estd::empty{}) {
    auto result = ::sysconf(name);
    if (result == -1) {
        std::perror(("[sysconf_checked] sysconf had error, when initilizing "_sl + target_name).data);
        std::exit(1);
    }
    if constexpr (!std::is_same_v<estd::empty, BoundT>) {
        if (!std::forward<BoundT>(bound)(result)) {
            std::puts(("[sysconf_checked] sysconf result out of specifed bounds , when initilizing "_sl + target_name).data);
            std::exit(1);
        }
    }
    return result;
}

struct ::sysinfo sysinfo_checked () {
    struct ::sysinfo info;
    auto sysinfo_result = ::sysinfo(&info);
    if (sysinfo_result != 0) {
        if (sysinfo_result == -1) {
            std::perror("[config::create] sysinfo had error");
        } else {
            std::print("[config::create] sysinfo returned unexpected value: {}", sysinfo_result);
        }
        std::exit(1);
    }
    return info;
}

struct info_t {
    size_t total_ram;

    static auto get_free_ram () {
        return sysinfo_checked().freeram;
    }

    static info_t create () {
        #ifdef _WINDOWS_

        MEMORYSTATUSEX statex;
        statex.dwLength = sizeof(statex);

        if (!GlobalMemoryStatusEx(&statex)) {
            std::perror("[config::create] GlobalMemoryStatusEx had error");
            std::exit(1);
        }

        return {
            statex.ullTotalPhys
        };

        #else



        auto info = sysinfo_checked();
        std::println("info.totalram: {}", info.totalram);

        return {
            info.totalram
        };

        #endif
    }
};

};

static const info_t sys_info = info_t::create();