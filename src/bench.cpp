#include <cstddef>
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <string_view>
#include <utility>
#include "./util/logger.hpp"
#include "./util/string_literal.hpp"

// Logger concept
struct CustomLogger {
    template<bool is_last, typename... Args>
    static void log(Args&&... args) {
        console.log<false, !is_last>(std::forward<Args>(args)...);
    }
};

// CoutLogger: prints to std::cout
struct CoutLogger {
    template<bool is_last, typename... Args>
    static void log(Args&&... args) {
        (std::cout << ... << args) << '\n';
    }
};

// NullLogger: does nothing
struct NullLogger {
    template<bool is_last, typename... Args>
    static void log(Args&&... /*unused*/) {}
};

// Huge string literals for stress test
constexpr char longStr1[] = "This is a very long string literal intended to simulate real-world log data. It could represent JSON, XML, stack traces, or debug info. ";
constexpr char longStr2[] = "Another long string segment that might contain variable metadata, timestamps, thread IDs, error messages, or performance counters. ";
constexpr char longStr3[] = "Even more verbose content to simulate real-world bloated logs like web service responses, logs from distributed systems, or verbose debug data. ";

template<typename LoggerT, bool is_last>
std::chrono::milliseconds bench() {
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < 1000'000; ++i) {
        LoggerT::template log<is_last>(
            "[", "] ",
            longStr1, longStr2, longStr3,
            longStr1, longStr2, longStr3,
            " - END LOG ENTRY"
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = end - start;

    return std::chrono::duration_cast<std::chrono::milliseconds>(diff);
}

template <typename T, StringLiteral _name>
struct BenchTarget {
    using target = T;
    static constexpr auto name = _name;

    template <typename... ll_params>
    void log (logger::writer<ll_params...> lw) const {
        lw.template write<true, true>("BenchTarget<T = ", std::string_view{typeid(T).name()}.substr(2), ", _name = ", _name, ">{}");
    }
};


template <BenchTarget ...Targets, size_t ...Indecies>
void multi_bench_detail (std::index_sequence<Indecies...> /*unused*/) {
    std::chrono::milliseconds times[sizeof...(Targets)];
    ((
        times[Indecies] = bench<typename decltype(Targets)::target, Indecies == sizeof...(Indecies) - 1>()
    ), ...);
    ((
        console.log<false, false, "\n"_sl + Targets.name>(": ", times[Indecies].count(), "ms")
    ), ...);
}

template <BenchTarget ...Targets>
void multi_bench () {
    multi_bench_detail<Targets...>(std::make_index_sequence<sizeof...(Targets)>{});
}

void bench () {
    multi_bench<
        BenchTarget<CoutLogger, "CoutLogger">{},
        BenchTarget<CustomLogger, "Logger">{},
        BenchTarget<NullLogger, "NullLogger">{}
    >();
}

int main () {
    bench();
}