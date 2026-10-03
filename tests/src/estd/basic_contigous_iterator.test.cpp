#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <tuple>
#include <utility>

#include <boost/ut.hpp>

#include "../../../src/estd/basic_contiguous_iterator.hpp"

using namespace boost::ut;


namespace {

using iterator = estd::basic_contiguous_iterator<int>;
using const_iterator = estd::basic_contiguous_iterator<const int>;

struct TestValue {
    int value;
};

}

int main () {

// ============================================================================
// Construction and initialization
// ============================================================================

"Default construction creates a null iterator"_test = [] {
    iterator it;

    expect(it.operator->() == nullptr);
};


"Default construction is constexpr"_test = [] {
    constexpr iterator it {};

    static_assert(it.operator->() == nullptr);

    expect(true);
};


"Construction at the beginning points to the first element"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(it.operator->() == values.data());
    expect(*it == 10);
};


"Construction at the end points one past the last element"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    expect(it.operator->() == values.data() + values.size());
};


"Construction supports an empty range"_test = [] {
    std::array<int, 1> values {};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data()
    );

    auto end = iterator::at_range_end(
        values.data(),
        values.data()
    );

    expect(begin == end);
    expect(begin.operator->() == values.data());
    expect(end.operator->() == values.data());
};


"Construction supports a singleton range"_test = [] {
    std::array values {42};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + 1
    );

    auto end = iterator::at_range_end(
        values.data(),
        values.data() + 1
    );

    expect(begin != end);
    expect(*begin == 42);
    expect(begin + 1 == end);
};


"Construction supports const pointers"_test = [] {
    const std::array values {10, 20, 30};

    auto begin = const_iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto end = const_iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    expect(*begin == 10);
    expect(end - begin == 3);
};


"Construction supports volatile pointers"_test = [] {
    volatile int values[] {10, 20, 30};

    using volatile_iterator =
        estd::basic_contiguous_iterator<volatile int>;

    auto begin = volatile_iterator::at_range_begin(
        values,
        values + 3
    );

    expect(*begin == 10);
    expect(begin.operator->() == values);
};


"Construction supports const volatile pointers"_test = [] {
    const volatile int values[] {10, 20, 30};

    using cv_iterator =
        estd::basic_contiguous_iterator<const volatile int>;

    auto begin = cv_iterator::at_range_begin(
        values,
        values + 3
    );

    expect(*begin == 10);
    expect(begin.operator->() == values);
};


// ============================================================================
// Dereferencing and pointer access
// ============================================================================

"Dereferencing returns the element at the current position"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(*it == 10);

    ++it;
    expect(*it == 20);

    ++it;
    expect(*it == 30);

    ++it;
    expect(*it == 40);

    ++it;
    expect(*it == 50);
};


"Dereferencing allows modification through a mutable iterator"_test = [] {
    std::array values {10, 20, 30};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    *it = 100;

    expect(values[0] == 100);

    ++it;
    *it = 200;

    expect(values[1] == 200);
};


"Dereferencing a const iterator does not allow modification"_test = [] {
    using reference = std::iter_reference_t<const_iterator>;

    static_assert(std::same_as<reference, const int&>);
    static_assert(!std::assignable_from<reference, int>);

    const std::array values {10, 20, 30};

    auto it = const_iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(*it == 10);
};


"Arrow operator returns the current pointer"_test = [] {
    std::array values {10, 20, 30};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(it.operator->() == values.data());

    ++it;

    expect(it.operator->() == values.data() + 1);

    ++it;

    expect(it.operator->() == values.data() + 2);
};


"Arrow operator accesses members of a struct"_test = [] {
    std::array values {
        TestValue {10},
        TestValue {20},
        TestValue {30}
    };

    using struct_iterator =
        estd::basic_contiguous_iterator<TestValue>;

    auto it = struct_iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(it->value == 10);

    ++it;

    expect(it->value == 20);
};


"Indexing accesses elements relative to the current position"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(it[0] == 10);
    expect(it[1] == 20);
    expect(it[2] == 30);
    expect(it[3] == 40);
    expect(it[4] == 50);
};


"Indexing supports negative offsets within the valid range"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    std::ignore = it += 3;

    expect(it[-1] == 30);
    expect(it[-2] == 20);
    expect(it[-3] == 10);
};


"Indexing allows modification of elements"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    it[1] = 200;
    it[3] = 400;

    expect(values[0] == 10);
    expect(values[1] == 200);
    expect(values[2] == 30);
    expect(values[3] == 400);
    expect(values[4] == 50);
};


// ============================================================================
// Increment and decrement
// ============================================================================

"Prefix increment advances the iterator"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto& result = ++it;

    expect(&result == &it);
    expect(*it == 20);

    ++it;
    expect(*it == 30);

    ++it;
    expect(*it == 40);
};


"Postfix increment returns the previous position"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto previous = it++;

    expect(*previous == 10);
    expect(*it == 20);

    previous = it++;

    expect(*previous == 20);
    expect(*it == 30);
};


"Repeated increment reaches the range end"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    ++it;
    ++it;
    ++it;
    ++it;
    ++it;

    expect(it == iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    ));
};


"Prefix decrement moves the iterator backward"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    auto& result = --it;

    expect(&result == &it);
    expect(*it == 50);

    --it;
    expect(*it == 40);

    --it;
    expect(*it == 30);
};


"Postfix decrement returns the previous position"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    auto previous = it--;

    expect(previous == iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    ));

    expect(*it == 50);

    previous = it--;

    expect(*previous == 50);
    expect(*it == 40);
};


"Repeated decrement reaches the range beginning"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    --it;
    --it;
    --it;
    --it;
    --it;

    expect(it == iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    ));
};


"Increment and decrement work with const iterators"_test = [] {
    const std::array values {10, 20, 30, 40};

    auto it = const_iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(*it == 10);

    ++it;
    expect(*it == 20);

    it++;
    expect(*it == 30);

    --it;
    expect(*it == 20);

    it--;
    expect(*it == 10);
};


"Increment and decrement preserve the original range boundaries"_test = [] {
    std::array values {10, 20, 30, 40};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto end = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    auto it = begin;

    std::ignore = it += 2;
    std::ignore = it -= 1;

    expect(it == begin + 1);
    expect(it + 3 == end);
};


// ============================================================================
// Iterator arithmetic
// ============================================================================

"Adding a positive offset returns the expected iterator"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(*(begin + 1) == 20);
    expect(*(begin + 2) == 30);
    expect(*(begin + 4) == 50);
};


"Adding zero returns an equivalent iterator"_test = [] {
    std::array values {10, 20, 30};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto result = begin + 0;

    expect(result == begin);
    expect(result.operator->() == begin.operator->());
};


"Adding a negative offset moves backward"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto it = begin + 4;

    expect(*(it + (-1)) == 40);
    expect(*(it + (-2)) == 30);
    expect(*(it + (-4)) == 10);
};


"Left-hand addition works"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(*(1 + begin) == 20);
    expect(*(2 + begin) == 30);
    expect(*(4 + begin) == 50);
};


"Compound addition modifies the iterator"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto& result = (it += 2);

    expect(&result == &it);
    expect(*it == 30);

    std::ignore = it += 2;

    expect(*it == 50);
};


"Subtracting a positive offset returns the expected iterator"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto end = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    expect(*(end - 1) == 50);
    expect(*(end - 2) == 40);
    expect(*(end - 5) == 10);
};


"Subtracting a negative offset moves forward"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(*(begin - (-1)) == 20);
    expect(*(begin - (-3)) == 40);
};


"Compound subtraction modifies the iterator"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto it = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    auto& result = (it -= 2);

    expect(&result == &it);
    expect(*it == 40);

    std::ignore = it -= 2;

    expect(*it == 20);
};


"Arithmetic supports different integral types"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(*(begin + int16_t{1}) == 20);
    expect(*(begin + 2U) == 30);
    expect(*(begin + 3L) == 40);
    expect(*(begin + int64_t{4}) == 50);
};


"Arithmetic supports const iterators"_test = [] {
    const std::array values {10, 20, 30, 40, 50};

    auto begin = const_iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(*(begin + 2) == 30);
    expect(*(begin + 4) == 50);
    expect(*(begin + 4 - 2) == 30);
};


// ============================================================================
// Comparison and distance
// ============================================================================

"Equal positions compare equal"_test = [] {
    std::array values {10, 20, 30};

    auto first = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto second = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(first == second);
    expect(!(first != second));
};


"Different positions compare unequal"_test = [] {
    std::array values {10, 20, 30};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto middle = begin + 1;

    expect(begin != middle);
    expect(!(begin == middle));
};


"Less-than comparison reflects pointer order"_test = [] {
    std::array values {10, 20, 30, 40};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto middle = begin + 2;

    expect(begin < middle);
    expect(!(middle < begin));
};


"Greater-than comparison reflects pointer order"_test = [] {
    std::array values {10, 20, 30, 40};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto middle = begin + 2;

    expect(middle > begin);
    expect(!(begin > middle));
};


"Less-than-or-equal comparison works"_test = [] {
    std::array values {10, 20, 30};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto middle = begin + 1;

    expect(begin <= middle);
    expect(begin <= begin);
    expect(!(middle <= begin));
};


"Greater-than-or-equal comparison works"_test = [] {
    std::array values {10, 20, 30};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto middle = begin + 1;

    expect(middle >= begin);
    expect(begin >= begin);
    expect(!(begin >= middle));
};


"Subtraction returns the distance between iterators"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto end = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    expect(end - begin == 5);
    expect((begin + 3) - begin == 3);
    expect((begin + 4) - (begin + 1) == 3);
};


"Subtraction supports negative distances"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(begin - (begin + 1) == -1);
    expect(begin - (begin + 3) == -3);
    expect((begin + 2) - (begin + 4) == -2);
};


"Comparisons work between mutable and const iterators"_test = [] {
    std::array values {10, 20, 30, 40};

    auto mutable_begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    const_iterator const_begin = mutable_begin;
    const_iterator const_middle = mutable_begin + 2;

    expect(mutable_begin == const_begin);
    expect(const_begin == mutable_begin);

    expect(mutable_begin < const_middle);
    expect(const_middle > mutable_begin);

    expect(mutable_begin <= const_begin);
    expect(const_begin >= mutable_begin);
};


"Distance works between mutable and const iterators"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto mutable_begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    const_iterator const_begin = mutable_begin;
    const_iterator const_end = mutable_begin + 5;

    expect(const_end - mutable_begin == 5);
    expect(mutable_begin - const_end == -5);

    expect(const_end - const_begin == 5);
    expect(mutable_begin - const_begin == 0);
};


// ============================================================================
// Conversion
// ============================================================================

"Mutable iterator converts to const iterator"_test = [] {
    std::array values {10, 20, 30};

    auto mutable_it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    const_iterator const_it = mutable_it;

    expect(const_it == mutable_it);
    expect(*const_it == 10);
};


"Conversion preserves the current position"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto mutable_it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    std::ignore = mutable_it += 3;

    const_iterator const_it = mutable_it;

    expect(const_it.operator->() == values.data() + 3);
    expect(*const_it == 40);
};


"Conversion preserves the valid range in debug builds"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto mutable_it = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    const_iterator const_it = mutable_it;

    ++const_it;
    ++const_it;

    expect(*const_it == 30);

    --const_it;

    expect(*const_it == 20);
};


"Const iterator cannot convert to mutable iterator"_test = [] {
    static_assert(!std::constructible_from<
        iterator,
        const_iterator
    >);

    static_assert(!std::convertible_to<
        const_iterator,
        iterator
    >);

    expect(true);
};


"Conversion works for iterators at the range end"_test = [] {
    std::array values {10, 20, 30};

    auto mutable_end = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    const_iterator const_end = mutable_end;

    expect(const_end == mutable_end);
    expect(const_end.operator->() == values.data() + 3);
};


"Conversion works for an empty range"_test = [] {
    std::array<int, 1> values {};

    auto mutable_begin = iterator::at_range_begin(
        values.data(),
        values.data()
    );

    const_iterator const_begin = mutable_begin;

    expect(const_begin == mutable_begin);
    expect(const_begin.operator->() == values.data());
};


// ============================================================================
// Standard iterator concepts
// ============================================================================

"Iterator satisfies the standard iterator concepts"_test = [] {
    static_assert(std::input_iterator<iterator>);
    static_assert(std::forward_iterator<iterator>);
    static_assert(std::bidirectional_iterator<iterator>);
    static_assert(std::random_access_iterator<iterator>);
    static_assert(std::contiguous_iterator<iterator>);

    static_assert(std::input_iterator<const_iterator>);
    static_assert(std::forward_iterator<const_iterator>);
    static_assert(std::bidirectional_iterator<const_iterator>);
    static_assert(std::random_access_iterator<const_iterator>);
    static_assert(std::contiguous_iterator<const_iterator>);

    expect(true);
};


"Iterator satisfies sized sentinel requirements"_test = [] {
    static_assert(std::sized_sentinel_for<iterator, iterator>);
    static_assert(std::sized_sentinel_for<const_iterator, const_iterator>);

    static_assert(std::sized_sentinel_for<
        const_iterator,
        iterator
    >);

    static_assert(std::sized_sentinel_for<
        iterator,
        const_iterator
    >);

    expect(true);
};


"Iterator exposes the expected associated types"_test = [] {
    static_assert(std::same_as<
        std::iter_value_t<iterator>,
        int
    >);

    static_assert(std::same_as<
        std::iter_reference_t<iterator>,
        int&
    >);

    static_assert(std::same_as<
        std::iter_value_t<const_iterator>,
        int
    >);

    static_assert(std::same_as<
        std::iter_reference_t<const_iterator>,
        const int&
    >);

    static_assert(std::same_as<
        std::iter_difference_t<iterator>,
        std::ptrdiff_t
    >);

    static_assert(std::same_as<
        std::iter_rvalue_reference_t<iterator>,
        int&&
    >);

    expect(true);
};


"std::to_address returns the underlying pointer"_test = [] {
    std::array values {10, 20, 30, 40};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    expect(std::to_address(begin) == values.data());

    auto middle = begin + 2;

    expect(std::to_address(middle) == values.data() + 2);
};


"Iterator works with standard algorithms"_test = [] {
    std::array values {10, 20, 30, 40, 50};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto end = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    expect(std::ranges::distance(begin, end) == 5);

    expect(std::ranges::all_of(
        std::ranges::subrange {begin, end},
        [](int value) { return value > 0; }
    ));

    expect(std::ranges::find(
        std::ranges::subrange {begin, end},
        30
    ) == begin + 2);
};


"Iterator supports sorting through its random access interface"_test = [] {
    std::array values {50, 10, 40, 20, 30};

    auto begin = iterator::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    auto end = iterator::at_range_end(
        values.data(),
        values.data() + values.size()
    );

    std::ranges::sort(begin, end);

    expect(values[0] == 10);
    expect(values[1] == 20);
    expect(values[2] == 30);
    expect(values[3] == 40);
    expect(values[4] == 50);
};


// ============================================================================
// Compile-time properties
// ============================================================================

"Iterator conversion properties are correct"_test = [] {
    static_assert(std::is_convertible_v<
        iterator,
        const_iterator
    >);

    static_assert(!std::is_convertible_v<
        const_iterator,
        iterator
    >);

    static_assert(std::constructible_from<
        const_iterator,
        const iterator&
    >);

    static_assert(std::copy_constructible<iterator>);
    static_assert(std::copy_constructible<const_iterator>);

    expect(true);
};


"Iterator supports constexpr construction and arithmetic"_test = [] {
    static constexpr std::array values {10, 20, 30, 40};

    constexpr auto begin = estd::basic_contiguous_iterator<const int>::at_range_begin(
        values.data(),
        values.data() + values.size()
    );

    constexpr auto second = begin + 1;
    constexpr auto fourth = begin + 3;

    static_assert(*begin == 10);
    static_assert(*second == 20);
    static_assert(*fourth == 40);

    static_assert(fourth - begin == 3);
    static_assert(begin < fourth);

    expect(true);
};

}