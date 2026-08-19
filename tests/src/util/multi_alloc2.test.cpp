#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/ut.hpp>

#include "../../../src/util/multi_alloc.hpp"
#include "../../utility.hpp"

int main() {

using namespace test;
using namespace boost::ut;

struct alignas(64) CacheLineAligned {
    uint64_t data[8];
};

struct alignas(128) VeryStrictAligned {
    char data[128];
};

// =============================================================================
// Type traits
// =============================================================================

"multi_alloc is move-only"_test = [] {
    using MA = decltype(multi_alloc{
        alloc<int, 5>(),
        alloc<double>(10)
    });

    static_assert(!std::is_copy_constructible_v<MA>);
    static_assert(!std::is_copy_assignable_v<MA>);
    static_assert(std::is_move_constructible_v<MA>);
    static_assert(std::is_move_assignable_v<MA>);
};

// =============================================================================
// Empty allocation
// =============================================================================

"Empty multi_alloc has an empty allocation tuple"_test = [] {
    multi_alloc<> ma;

    expect(std::tuple_size_v<
        std::remove_cvref_t<decltype(ma.allocated())>
    > == 0);
};

// =============================================================================
// Extents
// =============================================================================

"Allocated spans have the requested extents"_test = [] {
    multi_alloc ma{
        alloc<int, 4>(),
        alloc<double>(3)
    };

    auto& [ints, doubles] = ma.allocated();

    expect(ints.size() == 4u);
    expect(decltype(ints)::extent == 4u);

    expect(doubles.size() == 3u);
    expect(decltype(doubles)::extent == std::dynamic_extent);
};

"Fixed allocation has the correct extent"_test = [] {
    multi_alloc ma{
        alloc<int, 7>()
    };

    auto& [ints] = ma.allocated();

    expect(ints.size() == 7u);
    expect(decltype(ints)::extent == 7u);
};

"Dynamic allocation has the requested size"_test = [] {
    for (auto request_size : std::views::concat(
        std::views::iota(size_t{0}, size_t{20}),
        std::to_array<size_t>({101, 1001, 100001})
    )) {
        multi_alloc ma{
            alloc<int>(request_size)
        };

        auto& [ints] = ma.allocated();

        expect(ints.size() == request_size);
        expect(decltype(ints)::extent == std::dynamic_extent);
    }
};

// =============================================================================
// Data access
// =============================================================================

"Allocated spans provide mutable access to elements"_test = [] {
    multi_alloc ma{
        alloc<int, 4>(),
        alloc<double>(3)
    };

    auto& [ints, doubles] = ma.allocated();

    for (size_t i = 0; i < ints.size(); ++i)
        ints[i] = static_cast<int>(i * 10);

    for (size_t i = 0; i < doubles.size(); ++i)
        doubles[i] = static_cast<double>(i) + 0.5;

    expect(ints[0] == 0);
    expect(ints[3] == 30);

    expect(doubles[0] == 0.5);
    expect(doubles[2] == 2.5);
};

"Allocated spans provide const access to elements"_test = [] {
    multi_alloc ma{
        alloc<int, 3>()
    };

    auto& [ints] = ma.allocated();

    ints[0] = 10;
    ints[1] = 20;
    ints[2] = 30;

    const auto& cma = ma;
    const auto& [const_ints] = cma.allocated();

    expect(const_ints[0] == 10);
    expect(const_ints[1] == 20);
    expect(const_ints[2] == 30);
};

"Allocated spans return the same object references"_test = [] {
    multi_alloc ma{
        alloc<int, 3>()
    };

    auto& [ints] = ma.allocated();

    expect(&ints[0] == &ints[0]);
    expect(&ints[1] == &ints[1]);
    expect(&ints[2] == &ints[2]);
};

"Allocated spans preserve data pointer identity"_test = [] {
    multi_alloc ma{
        alloc<int, 3>()
    };

    auto& [ints] = ma.allocated();

    expect(ints.data() == &ints[0]);
};

// =============================================================================
// Alignment
// =============================================================================

"Allocated storage satisfies type alignment"_test = [] {
    multi_alloc ma{
        alloc<char, 3>(),
        alloc<CacheLineAligned, 2>(),
        alloc<int>(5),
        alloc<VeryStrictAligned, 1>()
    };

    auto [chars, cache_lines, ints, strict_aligns] = ma.allocated();

    const auto is_aligned = [](const void* ptr, size_t alignment) {
        return reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0;
    };

    expect(is_aligned(chars.data(), alignof(char)));
    expect(is_aligned(cache_lines.data(), alignof(CacheLineAligned)));
    expect(is_aligned(ints.data(), alignof(int)));
    expect(is_aligned(
        strict_aligns.data(),
        alignof(VeryStrictAligned)
    ));
};

// =============================================================================
// Lifetime management
// =============================================================================

"Fixed allocation default constructs every element"_test = [] {
    Tracked::reset();

    {
        const int n = 5;

        multi_alloc ma{
            alloc<Tracked, n>()
        };

        expect_stats(
            n,  // default_ctor
            0,  // value_ctor
            0,  // copy_ctor
            0,  // move_ctor
            0,  // copy_assign
            0,  // move_assign
            0,  // dtor
            n   // alive
        );
    }

    expect_stats(
        5,  // default_ctor
        0,  // value_ctor
        0,  // copy_ctor
        0,  // move_ctor
        0,  // copy_assign
        0,  // move_assign
        5,  // dtor
        0   // alive
    );
};

"Dynamic allocation default constructs every element"_test = [] {
    Tracked::reset();

    {
        const int n = 7;

        multi_alloc ma{
            alloc<Tracked>(n)
        };

        expect_stats(
            n,
            0,
            0,
            0,
            0,
            0,
            0,
            n
        );
    }

    expect_stats(
        7,
        0,
        0,
        0,
        0,
        0,
        7,
        0
    );
};

"Trivial and non-trivial allocations have independent lifetimes"_test = [] {
    Tracked::reset();

    {
        multi_alloc ma{
            alloc<int, 10>(),
            alloc<Tracked>(3),
            alloc<double, 2>()
        };

        auto& [ints, trackers, doubles] = ma.allocated();

        expect(ints.size() == 10u);
        expect(trackers.size() == 3u);
        expect(doubles.size() == 2u);

        expect_stats(
            3,  // default_ctor
            0,
            0,
            0,
            0,
            0,
            0,
            3
        );
    }

    expect_stats(
        3,
        0,
        0,
        0,
        0,
        0,
        3,
        0
    );
};

"Destroying multi_alloc destroys every non-trivial element exactly once"_test = [] {
    Tracked::reset();

    {
        multi_alloc ma{
            alloc<Tracked, 4>(),
            alloc<int, 10>(),
            alloc<Tracked>(6)
        };

        expect_stats(
            10, // default_ctor
            0,
            0,
            0,
            0,
            0,
            0,
            10
        );
    }

    expect_stats(
        10,
        0,
        0,
        0,
        0,
        0,
        10,
        0
    );
};

// =============================================================================
// Access through allocated()
// =============================================================================

"allocated returns the same allocation storage"_test = [] {
    multi_alloc ma{
        alloc<Tracked, 3>()
    };

    auto& first = ma.allocated();
    auto& second = ma.allocated();

    expect(&std::get<0>(first)[0] == &std::get<0>(second)[0]);
    expect(&std::get<0>(first)[1] == &std::get<0>(second)[1]);
    expect(&std::get<0>(first)[2] == &std::get<0>(second)[2]);
};

"Const allocated returns the same allocation storage"_test = [] {
    multi_alloc ma{
        alloc<Tracked, 3>()
    };

    const auto& cma = ma;

    const auto& first = cma.allocated();
    const auto& second = cma.allocated();

    expect(&std::get<0>(first)[0] == &std::get<0>(second)[0]);
    expect(&std::get<0>(first)[1] == &std::get<0>(second)[1]);
    expect(&std::get<0>(first)[2] == &std::get<0>(second)[2]);
};

// =============================================================================
// Move construction
// =============================================================================

"Move constructing multi_alloc transfers storage without moving elements"_test = [] {
    const int n = 4;

    Tracked::reset();

    {
        multi_alloc original{
            alloc<Tracked, n>()
        };

        auto& [elements] = original.allocated();
        elements[0].value = 777;

        expect_stats(
            n,
            0,
            0,
            0,
            0,
            0,
            0,
            n
        );

        multi_alloc moved{std::move(original)};

        auto& [moved_elements] = moved.allocated();

        expect_stats(
            n,
            0,
            0,
            0,
            0,
            0,
            0,
            n
        );

        expect(moved_elements.size() == n);
        expect(moved_elements[0].value == 777);
        expect(&moved_elements[0] == &elements[0]);
    }

    expect_stats(
        n,
        0,
        0,
        0,
        0,
        0,
        n,
        0
    );
};

// =============================================================================
// Move assignment
// =============================================================================

"Move assignment transfers storage without moving elements"_test = [] {
    const int src_n = 6;
    const int dst_n = 4;

    Tracked::reset();

    {
        multi_alloc source{
            alloc<Tracked>(src_n)
        };

        multi_alloc target{
            alloc<Tracked>(dst_n)
        };

        auto& [source_elements] = source.allocated();
        // [[maybe_unused]] auto& [target_elements] = target.allocated();

        source_elements[0].value = 777;

        expect_stats(
            src_n + dst_n,
            0,
            0,
            0,
            0,
            0,
            0,
            src_n + dst_n
        );

        target = std::move(source);

        auto& [moved_elements] = target.allocated();

        expect_stats(
            src_n + dst_n,
            0,
            0,
            0,
            0,
            0,
            dst_n,
            src_n
        );

        expect(moved_elements.size() == src_n);
        expect(moved_elements[0].value == 777);
        expect(&moved_elements[0] == &source_elements[0]);
    }

    expect_stats(
        src_n + dst_n,
        0,
        0,
        0,
        0,
        0,
        src_n + dst_n,
        0
    );
};

// =============================================================================
// Move-only / non-copyable element types
// =============================================================================

"multi_alloc supports non-copyable element types"_test = [] {
    multi_alloc ma{
        alloc<NonCopyable, 3>()
    };

    auto& [elements] = ma.allocated();

    expect(elements.size() == 3u);
};

"multi_alloc supports non-movable element types"_test = [] {
    multi_alloc ma{
        alloc<NonMovable, 3>()
    };

    auto& [elements] = ma.allocated();

    expect(elements.size() == 3u);
};

}