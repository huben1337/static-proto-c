#include <cassert>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

// Include your implementation header here
#include "../../../src/util/multi_alloc.hpp"

namespace test {

// =============================================================================
// Helper Types for Lifetime Tracking & Alignment Checks
// =============================================================================

struct LifetimeTracker {
    static inline int default_constructs = 0;
    static inline int destructs = 0;
    static inline void reset() { default_constructs = 0; destructs = 0; }

    int value = 42;

    LifetimeTracker() { ++default_constructs; }
    ~LifetimeTracker() { ++destructs; }
};

struct alignas(64) CacheLineAligned {
    uint64_t data[8];
};

struct alignas(128) VeryStrictAligned {
    char data[128];
};

// =============================================================================
// Unit Tests
// =============================================================================

// 1. Basic Type Traits & Constraints
static void test_traits_and_properties() {
    using MA = decltype(multi_alloc{alloc<int, 5>(), alloc<double>(10)});

    // Must be move-only
    static_assert(!std::is_copy_constructible_v<MA>);
    static_assert(!std::is_copy_assignable_v<MA>);
    static_assert(std::is_move_constructible_v<MA>);
    static_assert(std::is_move_assignable_v<MA>);
}

// 2. Empty Allocation Specialization
static void test_empty_alloc() {
    multi_alloc<> empty_ma;
    assert(std::tuple_size_v<std::remove_cvref_t<decltype(empty_ma.allocated())>> == 0);
}

// 3. Fixed vs. Dynamic Extents & Data Mutation
static void test_extents_and_data_access() {
    multi_alloc ma{
        alloc<int, 4>(),
        alloc<double>(3)
    };

    auto& [ints, doubles] = ma.allocated();

    // Verify span dimensions
    assert(ints.size() == 4);
    assert(decltype(ints)::extent == 4);

    assert(doubles.size() == 3);
    assert(decltype(doubles)::extent == std::dynamic_extent);

    // Verify data writing and reading works without memory corruption
    for (size_t i = 0; i < ints.size(); ++i) {
        ints[i] = static_cast<int>(i * 10);
    }
    for (size_t i = 0; i < doubles.size(); ++i) {
        doubles[i] = static_cast<double>(i) + 0.5;
    }

    assert(ints[0] == 0 && ints[3] == 30);
    assert(doubles[0] == 0.5 && doubles[2] == 2.5);
}

// 4. Over-Aligned Types & Strict Alignment Verification
static void test_memory_alignment() {
    multi_alloc ma{
        alloc<char, 3>(),
        alloc<CacheLineAligned, 2>(),
        alloc<int>(5),
        alloc<VeryStrictAligned, 1>()
    };

    auto [chars, cache_lines, ints, strict_aligns] = ma.allocated();

    // Verify all returned span pointers fulfill their structural alignment guarantees
    auto is_aligned = [](const void* ptr, size_t alignment) {
        return reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0;
    };

    assert(is_aligned(chars.data(), alignof(char)));
    assert(is_aligned(cache_lines.data(), alignof(CacheLineAligned)));
    assert(is_aligned(ints.data(), alignof(int)));
    assert(is_aligned(strict_aligns.data(), alignof(VeryStrictAligned)));
}

// 5. Non-Trivial Construction & Destruction Lifetimes
static void test_lifetime_management() {
    LifetimeTracker::reset();

    {
        multi_alloc ma{
            alloc<LifetimeTracker, 5>()
        };

        auto [trackers] = ma.allocated();

        // Check initialization
        assert(LifetimeTracker::default_constructs == 5);
        assert(LifetimeTracker::destructs == 0);
        assert(trackers[0].value == 42);
    }

    // Verify destructors were called properly on multi_alloc teardown
    assert(LifetimeTracker::destructs == 5);
}

// 6. Heterogeneous Layouts with Mixed Lifetimes
static void test_mixed_trivial_and_nontrivial() {
    LifetimeTracker::reset();

    {
        multi_alloc ma{
            alloc<int, 10>(),
            alloc<LifetimeTracker>(3),
            alloc<double, 2>()
        };

        [[maybe_unused]] auto [ints, trackers, doubles] = ma.allocated();
        assert(LifetimeTracker::default_constructs == 3);
    }

    assert(LifetimeTracker::destructs == 3);
}

// 7. Move Construction & Move Assignment
static void test_move_semantics() {
    multi_alloc original{
        alloc<int, 2>()
    };

    std::get<0>(original.allocated())[0] = 777;

    // Move construct
    multi_alloc moved{std::move(original)};
    assert(std::get<0>(moved.allocated())[0] == 777);

    // Move assign
    multi_alloc target{alloc<int, 2>()};
    target = std::move(moved);
    assert(std::get<0>(target.allocated())[0] == 777);
}

} // namespace test

int main() {
    test::test_traits_and_properties();
    test::test_empty_alloc();
    test::test_extents_and_data_access();
    test::test_memory_alignment();
    test::test_lifetime_management();
    test::test_mixed_trivial_and_nontrivial();
    test::test_move_semantics();

    std::println("All tests passed.");

    return 0;
}