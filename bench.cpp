#include <cstddef>
#include <iostream>
#include <chrono>
#include <utility>
#include "src/logger.cpp"
#include "src/string_literal.cpp"

// Logger concept
template<typename T>
concept logger_t = requires {
    T::info("test");
};

// CoutLogger: prints to std::cout
struct CoutLogger {
    template<typename... Args>
    static void info(Args&&... args) {
        (std::cout << ... << args) << '\n';
    }
};

// NullLogger: does nothing
struct NullLogger {
    template<typename... Args>
    static void info(Args&&...) {}
};

// Huge string literals for stress test
constexpr char longStr1[] = "This is a very long string literal intended to simulate real-world log data. It could represent JSON, XML, stack traces, or debug info. ";
constexpr char longStr2[] = "Another long string segment that might contain variable metadata, timestamps, thread IDs, error messages, or performance counters. ";
constexpr char longStr3[] = "Even more verbose content to simulate real-world bloated logs like web service responses, logs from distributed systems, or verbose debug data. ";

template<typename Logger>
std::chrono::milliseconds bench() {
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < 5'000; ++i) {
        Logger::info(
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
    typedef T target;
    static constexpr auto name = _name;
};


template <BenchTarget ...Targets, size_t ...Indecies>
void _multi_bench (std::index_sequence<Indecies...>) {
    std::chrono::milliseconds times[sizeof...(Targets)];
    ((
        times[Indecies] = bench<typename decltype(Targets)::target>()
    ), ...);

    ((
        std::cout << Targets.name.value << ": " << times[Indecies].count() << " ms\n"
    ), ...);
}

template <BenchTarget ...Targets>
void multi_bench () {
    _multi_bench<Targets...>(std::make_index_sequence<sizeof...(Targets)>{});
}

int main() {
    multi_bench<
        BenchTarget<logger, "Logger">{},
        BenchTarget<CoutLogger, "CoutLogger">{},
        BenchTarget<NullLogger, "NullLogger">{}
    >();
    return 0;
}