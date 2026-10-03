
#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <vector>
#include <gsl/pointers>

#include <boost/ut.hpp>

#include "../../../src/estd/ranges.hpp"

using namespace boost::ut;


template<typename Range, typename Values>
concept HasAccessSubspan = requires (
    const Range& range,
    Values& values
) {
    range.access_subspan(values);
};

template<typename Range, typename Values>
concept HasAccessSubrange = requires (
    const Range& range,
    Values& values
) {
    range.access_subrange(values);
};

int main () {

// ============================================================================
// Construction and object state
// ============================================================================

"Default construction creates an empty range"_test = [] {
    {
        estd::integral_range<int> range;

        expect(range.size() == 0U);
        expect(range.begin() == range.end());
    }
    {
        estd::integral_range<int> range {};

        expect(range.size() == 0U);
        expect(range.begin() == range.end());
    }
    {
        estd::integral_range<unsigned> range;

        expect(range.size() == 0U);
        expect(range.begin() == range.end());
    }
};


"Construction from equal bounds creates an empty range"_test = [] {
    {
        estd::integral_range range {0, 0};

        expect(range.size() == 0U);
        expect(range.begin() == range.end());
    }
    {
        estd::integral_range range {5, 5};

        expect(range.size() == 0U);
        expect(range.begin() == range.end());
    }
    {
        estd::integral_range range {-5, -5};

        expect(range.size() == 0U);
        expect(range.begin() == range.end());
    }
};


"Construction stores the specified bounds"_test = [] {
    {
        estd::integral_range range {0, 5};

        expect(*range.begin() == 0);
        expect(*range.end() == 5);
    }
    {
        estd::integral_range range {-5, 5};

        expect(*range.begin() == -5);
        expect(*range.end() == 5);
    }
    {
        estd::integral_range range {-10, -5};

        expect(*range.begin() == -10);
        expect(*range.end() == -5);
    }
    {
        estd::integral_range range {5U, 10U};

        expect(*range.begin() == 5U);
        expect(*range.end() == 10U);
    }
};


"Construction from a starting value and size creates the expected range"_test = [] {
    {
        estd::integral_range range {
            0,
            estd::integral_range_size<unsigned> {0}
        };

        expect(range.size() == 0U);
        expect(range.begin() == range.end());
    }
    {
        estd::integral_range range {
            5U,
            estd::integral_range_size<unsigned> {1}
        };

        expect(range.size() == 1U);
        expect(*range.begin() == 5);
        expect(*range.end() == 6);
    }
    {
        estd::integral_range range {
            10,
            estd::integral_range_size<unsigned> {5}
        };

        expect(range.size() == 5U);
        expect(*range.begin() == 10);
        expect(*range.end() == 15);
    }
    {
        estd::integral_range range {
            -5,
            estd::integral_range_size<unsigned> {10}
        };

        expect(range.size() == 10U);
        expect(*range.begin() == -5);
        expect(*range.end() == 5);
    }
};


"Class template argument deduction deduces the integral type"_test = [] {
    static_assert(std::same_as<
        decltype(estd::integral_range {0, 5}),
        estd::integral_range<int>
    >);

    static_assert(std::same_as<
        decltype(estd::integral_range {0U, 5U}),
        estd::integral_range<unsigned>
    >);

    static_assert(std::same_as<
        decltype(estd::integral_range {0L, 5L}),
        estd::integral_range<long>
    >);

    static_assert(std::same_as<
        decltype(estd::integral_range {
            0,
            estd::integral_range_size<unsigned> {5}
        }),
        estd::integral_range<int>
    >);

    expect(true);
};


// ============================================================================
// Size and wrapped size
// ============================================================================

"Size returns the distance between bounds"_test = [] {
    {
        estd::integral_range range {0, 0};
        expect(range.size() == 0U);
    }
    {
        estd::integral_range range {0, 1};
        expect(range.size() == 1U);
    }
    {
        estd::integral_range range {0, 10};
        expect(range.size() == 10U);
    }
    {
        estd::integral_range range {5, 15};
        expect(range.size() == 10U);
    }
};


"Size works with signed integral types"_test = [] {
    {
        estd::integral_range range {-5, 5};
        expect(range.size() == 10U);
    }
    {
        estd::integral_range range {-10, -5};
        expect(range.size() == 5U);
    }
    {
        estd::integral_range range {-100, 100};
        expect(range.size() == 200U);
    }
};


"Size works with unsigned integral types"_test = [] {
    {
        estd::integral_range range {0U, 0U};
        expect(range.size() == 0U);
    }
    {
        estd::integral_range range {0U, 10U};
        expect(range.size() == 10U);
    }
    {
        estd::integral_range range {10U, 20U};
        expect(range.size() == 10U);
    }
};


"Wrapped size preserves the range size"_test = [] {
    {
        estd::integral_range range {0, 0};
        expect(range.wrapped_size().get() == 0U);
    }
    {
        estd::integral_range range {0, 1};
        expect(range.wrapped_size().get() == 1U);
    }
    {
        estd::integral_range range {-10, 10};
        expect(range.wrapped_size().get() == 20U);
    }
};


"Wrapped size can be used to construct an equivalent range"_test = [] {
    {
        estd::integral_range original {-5, 5};

        estd::integral_range reconstructed {
            -5,
            original.wrapped_size()
        };

        expect(reconstructed.size() == original.size());
        expect(*reconstructed.begin() == *original.begin());
        expect(*reconstructed.end() == *original.end());
    }
    {
        estd::integral_range original {10U, 30U};

        estd::integral_range reconstructed {
            10U,
            original.wrapped_size()
        };

        expect(reconstructed.size() == original.size());
        expect(*reconstructed.begin() == *original.begin());
        expect(*reconstructed.end() == *original.end());
    }
};


// ============================================================================
// Iteration
// ============================================================================

"Begin and end are equal for an empty range"_test = [] {
    {
        estd::integral_range range {0, 0};
        expect(range.begin() == range.end());
    }
    {
        estd::integral_range range {10, 10};
        expect(range.begin() == range.end());
    }
    {
        estd::integral_range range {-10, -10};
        expect(range.begin() == range.end());
    }
};


"Begin and end represent the specified bounds"_test = [] {
    {
        estd::integral_range range {0, 5};

        expect(*range.begin() == 0);
        expect(*range.end() == 5);
    }
    {
        estd::integral_range range {-5, 5};

        expect(*range.begin() == -5);
        expect(*range.end() == 5);
    }
    {
        estd::integral_range range {10U, 20U};

        expect(*range.begin() == 10U);
        expect(*range.end() == 20U);
    }
};


"Iteration visits every value in ascending order"_test = [] {
    {
        estd::integral_range range {0, 5};

        std::array expected {0, 1, 2, 3, 4};
        std::size_t index = 0;

        for (auto value : range) {
            expect(value == expected[index]);
            ++index;
        }

        expect(index == expected.size());
    }
    {
        estd::integral_range range {-3, 3};

        std::array expected {-3, -2, -1, 0, 1, 2};
        std::size_t index = 0;

        for (auto value : range) {
            expect(value == expected[index]);
            ++index;
        }

        expect(index == expected.size());
    }
    {
        estd::integral_range range {5U, 10U};

        std::array expected {5U, 6U, 7U, 8U, 9U};
        std::size_t index = 0;

        for (auto value : range) {
            expect(value == expected[index]);
            ++index;
        }

        expect(index == expected.size());
    }
};


"Iteration visits exactly the expected number of values"_test = [] {
    {
        estd::integral_range range {0, 0};

        std::size_t count = 0;
        for ([[maybe_unused]] auto value : range) {
            ++count;
        }

        expect(count == 0U);
    }
    {
        estd::integral_range range {5, 6};

        std::size_t count = 0;
        for ([[maybe_unused]] auto value : range) {
            ++count;
        }

        expect(count == 1U);
    }
    {
        estd::integral_range range {-10, 10};

        std::size_t count = 0;
        for ([[maybe_unused]] auto value : range) {
            ++count;
        }

        expect(count == 20U);
    }
};


"Dereferencing an iterator returns the current position"_test = [] {
    estd::integral_range range {10, 15};

    auto it = range.begin();

    expect(*it == 10);

    ++it;
    expect(*it == 11);

    ++it;
    expect(*it == 12);

    ++it;
    expect(*it == 13);

    ++it;
    expect(*it == 14);
};


"Prefix increment advances the iterator"_test = [] {
    estd::integral_range range {0, 5};

    auto it = range.begin();

    expect(*it == 0);

    ++it;
    expect(*it == 1);

    ++it;
    expect(*it == 2);

    ++it;
    expect(*it == 3);

    ++it;
    expect(*it == 4);

    ++it;
    expect(it == range.end());
};


"Iterators compare equal when positions match"_test = [] {
    estd::integral_range range {0, 10};

    auto first = range.begin();
    auto second = range.begin();

    expect(first == second);

    ++first;
    ++second;

    expect(first == second);

    ++first;
    ++second;

    expect(first == second);
};


"Iterators compare unequal when positions differ"_test = [] {
    estd::integral_range range {0, 10};

    auto first = range.begin();
    auto second = range.begin();

    ++second;

    expect(!(first == second));

    ++first;

    expect(first == second);
};

// TODO
// "Range satisfies the expected standard range concepts"_test = [] {
//     using range_type = estd::integral_range<int>;
//     using iterator_type = range_type::iterator;

//     static_assert(std::ranges::range<range_type>);
//     static_assert(std::ranges::sized_range<range_type>);

//     static_assert(std::input_or_output_iterator<iterator_type>);
//     static_assert(std::input_iterator<iterator_type>);

//     static_assert(!std::random_access_iterator<iterator_type>);

//     expect(true);
// };


// ============================================================================
// access_subspan: contiguous ranges
// ============================================================================

"Full subspan returns all elements"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {0U, 5U};

    auto result = range.access_subspan(values);

    expect(result.size() == 5U);
    expect(result[0] == 10);
    expect(result[1] == 20);
    expect(result[2] == 30);
    expect(result[3] == 40);
    expect(result[4] == 50);
};


"Middle subspan returns the selected elements"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subspan(values);

    expect(result.size() == 3U);
    expect(result[0] == 20);
    expect(result[1] == 30);
    expect(result[2] == 40);
};


"Empty subspan returns an empty span"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    {
        estd::integral_range range {0U, 0U};
        auto result = range.access_subspan(values);

        expect(result.empty());
        expect(result.data() == values.data());
    }
    {
        estd::integral_range range {2U, 2U};
        auto result = range.access_subspan(values);

        expect(result.empty());
        expect(result.data() == values.data() + 2);
    }
    {
        estd::integral_range range {5U, 5U};
        auto result = range.access_subspan(values);

        expect(result.empty());
        expect(result.data() == values.data() + 5);
    }
};


"Singleton subspan returns one element"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    {
        estd::integral_range range {0U, 1U};
        auto result = range.access_subspan(values);

        expect(result.size() == 1U);
        expect(result[0] == 10);
    }
    {
        estd::integral_range range {2U, 3U};
        auto result = range.access_subspan(values);

        expect(result.size() == 1U);
        expect(result[0] == 30);
    }
    {
        estd::integral_range range {4U, 5U};
        auto result = range.access_subspan(values);

        expect(result.size() == 1U);
        expect(result[0] == 50);
    }
};


"Subspan has the correct data pointer"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {2U, 4U};

    auto result = range.access_subspan(values);

    expect(result.data() == values.data() + 2);
};


"Subspan allows modification of underlying elements"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subspan(values);

    result[0] = 100;
    result[1] = 200;
    result[2] = 300;

    expect(values[0] == 10);
    expect(values[1] == 100);
    expect(values[2] == 200);
    expect(values[3] == 300);
    expect(values[4] == 50);
};


"Subspan works with different contiguous container types"_test = [] {
    {
        std::array values {1, 2, 3, 4};

        estd::integral_range range {1U, 3U};
        auto result = range.access_subspan(values);

        expect(result.size() == 2U);
        expect(result[0] == 2);
        expect(result[1] == 3);
    }
    {
        std::vector values {1, 2, 3, 4};

        estd::integral_range range {1U, 3U};
        auto result = range.access_subspan(values);

        expect(result.size() == 2U);
        expect(result[0] == 2);
        expect(result[1] == 3);
    }
    {
        int values[] {1, 2, 3, 4};

        estd::integral_range range {1U, 3U};
        auto result = range.access_subspan(values);

        expect(result.size() == 2U);
        expect(result[0] == 2);
        expect(result[1] == 3);
    }
};


"Subspan works with const containers"_test = [] {
    const std::array values {10, 20, 30, 40};

    estd::integral_range range {1U, 3U};

    auto result = range.access_subspan(values);

    expect(result.size() == 2U);
    expect(result[0] == 20);
    expect(result[1] == 30);

    static_assert(std::same_as<
        decltype(result),
        std::span<const int>
    >);
};


"Subspan works in a constant-evaluated context"_test = [] {
    static constexpr std::array values {10, 20, 30, 40};

    constexpr estd::integral_range range {1U, 3U};

    constexpr auto result = range.access_subspan(values);

    static_assert(result.size() == 2U);
    static_assert(result[0] == 20);
    static_assert(result[1] == 30);

    expect(true);
};


// ============================================================================
// access_subspan: gsl::not_null pointers
// ============================================================================

"Pointer subspan returns the expected elements"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    {
        estd::integral_range range {0U, 5U};
        auto result = range.access_subspan(pointer);

        expect(result.size() == 5U);
        expect(result[0] == 10);
        expect(result[4] == 50);
    }
    {
        estd::integral_range range {1U, 4U};
        auto result = range.access_subspan(pointer);

        expect(result.size() == 3U);
        expect(result[0] == 20);
        expect(result[1] == 30);
        expect(result[2] == 40);
    }
};


"Pointer subspan handles empty and singleton ranges"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    {
        estd::integral_range range {2U, 2U};
        auto result = range.access_subspan(pointer);

        expect(result.empty());
        expect(result.data() == values + 2);
    }
    {
        estd::integral_range range {3U, 4U};
        auto result = range.access_subspan(pointer);

        expect(result.size() == 1U);
        expect(result[0] == 40);
    }
};


"Pointer subspan points to the correct offset"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    estd::integral_range range {2U, 4U};

    auto result = range.access_subspan(pointer);

    expect(result.data() == values + 2);
};


"Pointer subspan allows modification of underlying elements"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subspan(pointer);

    result[0] = 100;
    result[1] = 200;
    result[2] = 300;

    expect(values[0] == 10);
    expect(values[1] == 100);
    expect(values[2] == 200);
    expect(values[3] == 300);
    expect(values[4] == 50);
};


"Pointer subspan supports const elements"_test = [] {
    const int values[] {10, 20, 30, 40};

    gsl::not_null<const int*> pointer {values};

    estd::integral_range range {1U, 3U};

    auto result = range.access_subspan(pointer);

    expect(result.size() == 2U);
    expect(result[0] == 20);
    expect(result[1] == 30);

    static_assert(std::same_as<
        decltype(result),
        std::span<const int>
    >);
};


// ============================================================================
// access_subrange: contiguous ranges
// ============================================================================

"Full subrange returns all elements"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {0U, 5U};

    auto result = range.access_subrange(values);

    expect(std::ranges::distance(result) == 5);
    expect(*result.begin() == 10);
    expect(*(result.end() - 1) == 50);
};


"Middle subrange returns the selected elements"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subrange(values);

    expect(std::ranges::distance(result) == 3);
    expect(*result.begin() == 20);
    expect(*(result.begin() + 1) == 30);
    expect(*(result.begin() + 2) == 40);
};


"Empty subrange returns equal iterators"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    {
        estd::integral_range range {0U, 0U};
        auto result = range.access_subrange(values);

        expect(result.begin() == result.end());
    }
    {
        estd::integral_range range {2U, 2U};
        auto result = range.access_subrange(values);

        expect(result.begin() == result.end());
    }
    {
        estd::integral_range range {5U, 5U};
        auto result = range.access_subrange(values);

        expect(result.begin() == result.end());
    }
};


"Singleton subrange returns one element"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    {
        estd::integral_range range {0U, 1U};
        auto result = range.access_subrange(values);

        expect(std::ranges::distance(result) == 1);
        expect(*result.begin() == 10);
    }
    {
        estd::integral_range range {2U, 3U};
        auto result = range.access_subrange(values);

        expect(std::ranges::distance(result) == 1);
        expect(*result.begin() == 30);
    }
    {
        estd::integral_range range {4U, 5U};
        auto result = range.access_subrange(values);

        expect(std::ranges::distance(result) == 1);
        expect(*result.begin() == 50);
    }
};


"Subrange iterators have the expected positions"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subrange(values);

    expect(result.begin() == values.begin() + 1);
    expect(result.end() == values.begin() + 4);
};


"Subrange supports range-based iteration"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subrange(values);

    std::array expected {20, 30, 40};
    std::size_t index = 0;

    for (auto value : result) {
        expect(value == expected[index]);
        ++index;
    }

    expect(index == expected.size());
};


"Subrange allows modification of underlying elements"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subrange(values);

    *result.begin() = 100;
    *(result.begin() + 1) = 200;
    *(result.begin() + 2) = 300;

    expect(values[0] == 10);
    expect(values[1] == 100);
    expect(values[2] == 200);
    expect(values[3] == 300);
    expect(values[4] == 50);
};


"Subrange works with const containers"_test = [] {
    const std::array values {10, 20, 30, 40};

    estd::integral_range range {1U, 3U};

    auto result = range.access_subrange(values);

    expect(std::ranges::distance(result) == 2);
    expect(*result.begin() == 20);
    expect(*(result.begin() + 1) == 30);

    static_assert(std::same_as<
        std::ranges::range_reference_t<decltype(result)>,
        const int&
    >);
};


"Subrange works in a constant-evaluated context"_test = [] {
    static constexpr std::array values {10, 20, 30, 40};

    constexpr estd::integral_range range {1U, 3U};

    constexpr auto result = range.access_subrange(values);

    static_assert(std::ranges::distance(result) == 2);
    static_assert(*result.begin() == 20);
    static_assert(*(result.begin() + 1) == 30);

    expect(true);
};


// ============================================================================
// access_subrange: gsl::not_null pointers
// ============================================================================

"Pointer subrange returns the expected elements"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    {
        estd::integral_range range {0U, 5U};
        auto result = range.access_subrange(pointer);

        expect(std::ranges::distance(result) == 5);
        expect(*result.begin() == 10);
        expect(*(result.end() - 1) == 50);
    }
    {
        estd::integral_range range {1U, 4U};
        auto result = range.access_subrange(pointer);

        expect(std::ranges::distance(result) == 3);
        expect(*result.begin() == 20);
        expect(*(result.begin() + 1) == 30);
        expect(*(result.begin() + 2) == 40);
    }
};


"Pointer subrange returns equal iterators for an empty range"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    estd::integral_range range {2U, 2U};

    auto result = range.access_subrange(pointer);

    expect(result.begin() == result.end());
    expect(result.begin() == values + 2);
};


"Pointer subrange uses the correct pointer offsets"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    estd::integral_range range {2U, 4U};

    auto result = range.access_subrange(pointer);

    expect(result.begin() == values + 2);
    expect(result.end() == values + 4);
};


"Pointer subrange supports iteration"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subrange(pointer);

    std::array expected {20, 30, 40};
    std::size_t index = 0;

    for (auto value : result) {
        expect(value == expected[index]);
        ++index;
    }

    expect(index == expected.size());
};


"Pointer subrange allows modification of underlying elements"_test = [] {
    int values[] {10, 20, 30, 40, 50};

    gsl::not_null<int*> pointer {values};

    estd::integral_range range {1U, 4U};

    auto result = range.access_subrange(pointer);

    *result.begin() = 100;
    *(result.begin() + 1) = 200;
    *(result.begin() + 2) = 300;

    expect(values[0] == 10);
    expect(values[1] == 100);
    expect(values[2] == 200);
    expect(values[3] == 300);
    expect(values[4] == 50);
};


"Pointer subrange supports const elements"_test = [] {
    const int values[] {10, 20, 30, 40};

    gsl::not_null<const int*> pointer {values};

    estd::integral_range range {1U, 3U};

    auto result = range.access_subrange(pointer);

    expect(std::ranges::distance(result) == 2);
    expect(*result.begin() == 20);
    expect(*(result.begin() + 1) == 30);

    static_assert(std::same_as<
        std::ranges::range_reference_t<decltype(result)>,
        const int&
    >);
};


// ============================================================================
// Compile-time properties
// ============================================================================

"Default construction is constant evaluable"_test = [] {
    constexpr estd::integral_range<int> range {};

    static_assert(range.size() == 0U);
    static_assert(range.begin() == range.end());

    expect(true);
};


"Bound construction is constant evaluable"_test = [] {
    constexpr estd::integral_range range {-5, 5};

    static_assert(range.size() == 10U);
    static_assert(*range.begin() == -5);
    static_assert(*range.end() == 5);

    expect(true);
};


"Size and iteration are constant evaluable"_test = [] {
    constexpr estd::integral_range range {10, 15};

    constexpr auto begin = range.begin();

    static_assert(*begin == 10);
    static_assert(*(++estd::integral_range<int>::iterator {10}) == 11);
    static_assert(range.size() == 5U);

    expect(true);
};


"Size type is the corresponding unsigned integral type"_test = [] {
    static_assert(std::same_as<
        estd::integral_range<int>::unsigned_type,
        unsigned int
    >);

    static_assert(std::same_as<
        estd::integral_range<long>::unsigned_type,
        unsigned long
    >);

    static_assert(std::same_as<
        estd::integral_range<unsigned>::unsigned_type,
        unsigned
    >);

    expect(true);
};


"Access functions are restricted to unsigned range types"_test = [] {
    using signed_range = estd::integral_range<int>;
    using unsigned_range = estd::integral_range<unsigned>;

    using array_type = std::array<int, 5>;

    static_assert(requires (
        const unsigned_range& range,
        array_type& values
    ) {
        range.access_subspan(values);
        range.access_subrange(values);
    });

    static_assert(!HasAccessSubspan<signed_range, array_type>);
    static_assert(!HasAccessSubrange<signed_range, array_type>);
    
    expect(true);
};

}