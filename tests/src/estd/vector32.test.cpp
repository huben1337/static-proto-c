#include <array>
#include <cstddef>
#include <cstdint>
#include <gsl/util>
#include <ranges>
#include <utility>

#include <boost/ut.hpp>

#include "../../../src/estd/vector32.hpp"
#include "../../helpers.hpp"
#include "../../utility.hpp"

using namespace test;
using namespace boost::ut;


int main() {


// ============================================================================
// Construction / basic state
// ============================================================================

"Default construction creates empty vector"_test = [] {
    estd::vector32<TrivialValue> vec;

    expect(vec.empty());
    expect(vec.size() == 0);
    expect(vec.capacity() == 0);
    expect(vec.begin() == vec.end());
    expect(vec.data() == nullptr);
};


"Zero-size construction creates empty vector"_test = [] {
    estd::vector32<TrivialValue> vec{0};

    expect(vec.empty());
    expect(vec.size() == 0);
    expect(vec.begin() == vec.end());
    expect(vec.data() == nullptr);
    expect(vec.capacity() >= vec.size());
};


"Size construction creates N default-constructed elements"_test = [] {
    for (auto n : std::views::concat(
        std::views::iota(uint32_t{1}, uint32_t{20}),
        std::to_array<uint32_t>({101, 1001, 100001})
    )) {
        Tracked::reset();

        {
            estd::vector32<Tracked> vec{n};

            expect(vec.size() == n);
            expect(!vec.empty());
            expect(vec.data() != nullptr);
            expect(vec.begin() + n == vec.end());

            expect_stats(
                gsl::narrow_cast<int>(n),  // default_ctor
                0,  // value_ctor
                0,  // copy_ctor
                0,  // move_ctor
                0,  // copy_assign
                0,  // move_assign
                0,  // dtor
                gsl::narrow_cast<int>(n)   // alive
            );
        }

        expect_stats(
            gsl::narrow_cast<int>(n), 0, 0, 0,
            0, 0,
            gsl::narrow_cast<int>(n),
            0
        );
    }
};


"Size construction establishes valid capacity invariant"_test = [] {
    for (auto n : std::views::concat(
        std::views::iota(uint32_t{0}, uint32_t{20}),
        std::to_array<uint32_t>({101, 1001})
    )) {
        estd::vector32<TrivialValue> vec{n};

        expect(vec.capacity() >= vec.size());
        expect(vec.size() == n);
    }
};


// ============================================================================
// Move construction
// ============================================================================

"Move constructing vector transfers storage without moving elements"_test = [] {
    Tracked::reset();

    const uint32_t n = 5;

    {
        estd::vector32<Tracked> src{n};

        expect_stats(
            n, 0, 0, 0,
            0, 0, 0, n
        );

        auto* original_data = src.data();
        const auto original_capacity = src.capacity();

        estd::vector32<Tracked> dst{std::move(src)};

        expect_stats(
            n, 0, 0, 0,
            0, 0, 0, n
        );

        expect(dst.size() == n);
        expect(dst.capacity() == original_capacity);
        expect(dst.data() == original_data);

        expect(dst.begin() + n == dst.end());

        // The actual objects must be the original objects.
        expect(&dst[0] == original_data);
        expect(&dst[n - 1] == original_data + n - 1);
    }

    expect_stats(
        n, 0, 0, 0,
        0, 0,
        n,
        0
    );
};


"Move constructing empty vector is harmless"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> src;
        estd::vector32<Tracked> dst{std::move(src)};

        expect(dst.empty());
        expect(dst.size() == 0);
        expect(dst.begin() == dst.end());

        expect_stats(
            0, 0, 0, 0,
            0, 0, 0, 0
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0, 0, 0
    );
};


// ============================================================================
// Move assignment
// ============================================================================

"Move assignment into empty vector transfers storage"_test = [] {
    Tracked::reset();

    const uint32_t n = 6;

    {
        estd::vector32<Tracked> src{n};
        estd::vector32<Tracked> dst;

        auto* original_data = src.data();
        const auto original_capacity = src.capacity();

        expect_stats(
            n, 0, 0, 0,
            0, 0, 0, n
        );

        dst = std::move(src);

        expect_stats(
            n, 0, 0, 0,
            0, 0, 0, n
        );

        expect(dst.size() == n);
        expect(dst.capacity() == original_capacity);
        expect(dst.data() == original_data);
    }

    expect_stats(
        n, 0, 0, 0,
        0, 0,
        n,
        0
    );
};


"Move assignment destroys existing destination elements"_test = [] {
    Tracked::reset();

    const uint32_t src_n = 7;
    const uint32_t dst_n = 4;

    {
        estd::vector32<Tracked> src{src_n};
        estd::vector32<Tracked> dst{dst_n};

        expect_stats(
            src_n + dst_n,
            0, 0, 0,
            0, 0, 0,
            src_n + dst_n
        );

        auto* source_data = src.data();

        dst = std::move(src);

        expect_stats(
            src_n + dst_n,
            0, 0, 0,
            0, 0,
            dst_n,
            src_n
        );

        expect(dst.size() == src_n);
        expect(dst.data() == source_data);
    }

    expect_stats(
        src_n + dst_n,
        0, 0, 0,
        0, 0,
        src_n + dst_n,
        0
    );
};


"Move assignment from empty vector destroys destination elements"_test = [] {
    Tracked::reset();

    const uint32_t n = 5;

    {
        estd::vector32<Tracked> src;
        estd::vector32<Tracked> dst{n};

        dst = std::move(src);

        expect(dst.empty());

        expect_stats(
            n, 0, 0, 0,
            0, 0,
            n,
            0
        );
    }

    expect_stats(
        n, 0, 0, 0,
        0, 0,
        n,
        0
    );
};


"Self move assignment is safe"_test = [] {
    Tracked::reset();

    const uint32_t n = 4;

    {
        estd::vector32<Tracked> vec{n};

        vec[0].value = 10;
        vec[1].value = 20;
        vec[2].value = 30;
        vec[3].value = 40;

        expect_stats(
            n, 0, 0, 0,
            0, 0, 0, n
        );

        NOWARN(-Wself-move,
            vec = std::move(vec);
        )

        expect_stats(
            n, 0, 0, 0,
            0, 0, 0, n
        );

        expect(vec.size() == n);
        expect(vec[0].value == 10);
        expect(vec[1].value == 20);
        expect(vec[2].value == 30);
        expect(vec[3].value == 40);
    }

    expect_stats(
        n, 0, 0, 0,
        0, 0,
        n,
        0
    );
};


"Moved-from vector can be destroyed"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> src{8};

        {
            estd::vector32<Tracked> dst{std::move(src)};
            expect(dst.size() == 8);
        }

        // No access to the moved-from state is required.
        // It merely has to remain a valid object for destruction.
    }

    expect_stats(
        8, 0, 0, 0,
        0, 0,
        8,
        0
    );
};


"Moved-from vector can be assigned to"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> src{5};
        estd::vector32<Tracked> dst{std::move(src)};

        estd::vector32<Tracked> replacement{3};

        src = std::move(replacement);

        expect(src.size() == 3);
        expect(src.data() != nullptr);

        expect_stats(
            8, 0, 0, 0,
            0, 0,
            0,
            8
        );
    }

    expect_stats(
        8, 0, 0, 0,
        0, 0,
        8,
        0
    );
};


// ============================================================================
// Capacity
// ============================================================================

"Capacity is never smaller than size"_test = [] {
    for (auto n : std::views::concat(
        std::views::iota(uint32_t{0}, uint32_t{20}),
        std::to_array<uint32_t>({100, 1000})
    )) {
        estd::vector32<TrivialValue> vec{n};

        expect(vec.capacity() >= vec.size());
    }
};


"Reserve zero on empty vector is harmless"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.reserve(0);

        expect(vec.empty());
        expect(vec.capacity() == 0);
        expect(vec.data() == nullptr);

        expect_stats(
            0, 0, 0, 0,
            0, 0, 0, 0
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0, 0, 0
    );
};


"Reserve establishes requested capacity"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.reserve(100);

        expect(vec.empty());
        expect(vec.size() == 0);
        expect(vec.capacity() >= 100);

        expect_stats(
            0, 0, 0, 0,
            0, 0, 0, 0
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0, 0, 0
    );
};


"Reserve does not change size or values"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{4};

        vec[0].value = 11;
        vec[1].value = 22;
        vec[2].value = 33;
        vec[3].value = 44;

        const auto old_size = vec.size();

        vec.reserve(100);

        expect(vec.size() == old_size);
        expect(vec[0].value == 11);
        expect(vec[1].value == 22);
        expect(vec[2].value == 33);
        expect(vec[3].value == 44);
        expect(vec.capacity() >= 100);

        expect_stats(
            4, 0, 0, 4,
            0, 0,
            4,
            4
        );
    }

    expect_stats(
        4, 0, 0, 4,
        0, 0,
        8,
        0
    );
};


"Reserve within existing capacity does not move elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{4};

        vec.reserve(100);

        // Account for the initial relocation-free construction and
        // reserve from the empty vector.
        const auto before = vec.capacity();

        Tracked::reset_ops();

        vec.reserve(before);

        expect(vec.size() == 4);
        expect(vec.capacity() == before);

        expect_stats(
            0, 0, 0, 0,
            0, 0, 0, 4
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0,
        4,
        0
    );
};


"Reserve beyond capacity reallocates and preserves objects"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{5};

        vec[0].value = 1;
        vec[1].value = 2;
        vec[2].value = 3;
        vec[3].value = 4;
        vec[4].value = 5;

        const auto old_capacity = vec.capacity();
        const auto requested_capacity = old_capacity + 1;

        Tracked::reset_ops();

        vec.reserve(requested_capacity);

        expect(vec.capacity() >= requested_capacity);
        expect(vec.size() == 5);

        expect(vec[0].value == 1);
        expect(vec[1].value == 2);
        expect(vec[2].value == 3);
        expect(vec[3].value == 4);
        expect(vec[4].value == 5);

        expect_stats(
            0, 0, 0, 5,
            0, 0,
            5,
            5
        );
    }

    expect_stats(
        0, 0, 0, 5,
        0, 0,
        10,
        0
    );
};


"Shrink to fit on empty vector is harmless"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.shrink_to_fit();

        expect(vec.empty());
        expect(vec.capacity() == 0);
        expect(vec.data() == nullptr);

        expect_stats(
            0, 0, 0, 0,
            0, 0, 0, 0
        );
    }
};


"Shrink to fit preserves size and values"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.reserve(20);
        vec.resize(5);

        vec[0].value = 10;
        vec[1].value = 20;
        vec[2].value = 30;
        vec[3].value = 40;
        vec[4].value = 50;

        Tracked::reset_ops();

        vec.shrink_to_fit();

        expect(vec.size() == 5);
        expect(vec.capacity() >= vec.size());

        expect(vec[0].value == 10);
        expect(vec[1].value == 20);
        expect(vec[2].value == 30);
        expect(vec[3].value == 40);
        expect(vec[4].value == 50);

        // Five existing objects are relocated and five old objects
        // are destroyed if shrink_to_fit actually shrinks.
        //
        // If your implementation intentionally doesn't shrink in this
        // situation, this exact lifetime assertion should be relaxed.
        expect_stats(
            0, 0, 0, 5,
            0, 0,
            5,
            5
        );
    }

    expect_stats(
        0, 0, 0, 5,
        0, 0,
        10,
        0
    );
};


// ============================================================================
// Iterators / data
// ============================================================================

"Empty vector has equal begin and end"_test = [] {
    estd::vector32<Tracked> vec;

    expect(vec.begin() == vec.end());
    expect(vec.cbegin() == vec.cend());
};


"Data points to first element"_test = [] {
    estd::vector32<Tracked> vec{4};

    expect(vec.data() == &vec[0]);
    expect(vec.begin() == vec.data());
};


"Const data points to first element"_test = [] {
    estd::vector32<Tracked> vec{4};

    const auto& c = vec;

    expect(c.data() == &c[0]);
    expect(c.begin() == c.data());
    expect(c.cbegin() == c.data());
};


"Iterator distance equals size"_test = [] {
    for (uint32_t n = 0; n < 20; ++n) {
        estd::vector32<Tracked> vec{n};

        expect(vec.end() - vec.begin() ==
               static_cast<std::ptrdiff_t>(n));

        expect(vec.cend() - vec.cbegin() ==
               static_cast<std::ptrdiff_t>(n));
    }
};


"Mutable iterators access every element"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{5};

        int value = 1;

        for (auto& x : vec) {
            x.value = value++;
        }

        value = 1;

        for (auto& x : vec) {
            expect(x.value == value++);
        }

        expect_stats(
            5, 0, 0, 0,
            0, 0, 0, 5
        );
    }

    expect_stats(
        5, 0, 0, 0,
        0, 0,
        5,
        0
    );
};


"Const iterators read every element"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{3};

        vec[0].value = 4;
        vec[1].value = 5;
        vec[2].value = 6;

        const auto& c = vec;

        expect(c.begin()->value == 4);
        expect((c.begin() + 1)->value == 5);
        expect((c.end() - 1)->value == 6);

        expect(c.cbegin()->value == 4);
        expect((c.cbegin() + 1)->value == 5);
        expect((c.cend() - 1)->value == 6);

        expect_stats(
            3, 0, 0, 0,
            0, 0, 0, 3
        );
    }

    expect_stats(
        3, 0, 0, 0,
        0, 0,
        3,
        0
    );
};


// ============================================================================
// Element access
// ============================================================================

"Mutable subscript modifies elements"_test = [] {
    estd::vector32<Tracked> vec{3};

    vec[0].value = 10;
    vec[1].value = 20;
    vec[2].value = 30;

    expect(vec[0].value == 10);
    expect(vec[1].value == 20);
    expect(vec[2].value == 30);
};


"Const subscript reads elements"_test = [] {
    estd::vector32<Tracked> vec{3};

    vec[0].value = 10;
    vec[1].value = 20;
    vec[2].value = 30;

    const auto& c = vec;

    expect(c[0].value == 10);
    expect(c[1].value == 20);
    expect(c[2].value == 30);
};


"Mutable at modifies elements"_test = [] {
    estd::vector32<Tracked> vec{2};

    vec.at(0).value = 15;
    vec.at(1).value = 25;

    expect(vec.at(0).value == 15);
    expect(vec.at(1).value == 25);
};


"Const at reads elements"_test = [] {
    estd::vector32<Tracked> vec{2};

    vec[0].value = 7;
    vec[1].value = 9;

    const auto& c = vec;

    expect(c.at(0).value == 7);
    expect(c.at(1).value == 9);
};


"Front and back reference first and last elements"_test = [] {
    estd::vector32<Tracked> vec{3};

    expect(&vec.front() == &vec[0]);
    expect(&vec.back() == &vec[2]);

    expect(&vec.front() == vec.data());
    expect(&vec.back() == vec.data() + 2);
};


"Const front and back reference first and last elements"_test = [] {
    const estd::vector32<Tracked> vec{3};

    expect(&vec.front() == &vec[0]);
    expect(&vec.back() == &vec[2]);
};


"All mutable accessors reference the same objects"_test = [] {
    estd::vector32<Tracked> vec{3};

    expect(&vec[0] == &vec.at(0));
    expect(&vec[0] == &vec.front());
    expect(&vec[0] == vec.data());
    expect(&vec[0] == &*vec.begin());

    expect(&vec[2] == &vec.at(2));
    expect(&vec[2] == &vec.back());
    expect(&vec[2] == &*(vec.end() - 1));
};


"All const accessors reference the same objects"_test = [] {
    const estd::vector32<Tracked> vec{3};

    expect(&vec[0] == &vec.at(0));
    expect(&vec[0] == &vec.front());
    expect(&vec[0] == vec.data());
    expect(&vec[0] == &*vec.begin());
    expect(&vec[0] == &*vec.cbegin());

    expect(&vec[2] == &vec.at(2));
    expect(&vec[2] == &vec.back());
    expect(&vec[2] == &*(vec.end() - 1));
    expect(&vec[2] == &*(vec.cend() - 1));
};


"Repeated access returns the same object reference"_test = [] {
    estd::vector32<Tracked> vec{3};

    expect(&vec[0] == &vec[0]);
    expect(&vec[1] == &vec[1]);
    expect(&vec[2] == &vec[2]);

    expect(&vec.at(0) == &vec.at(0));
    expect(&vec.at(1) == &vec.at(1));
    expect(&vec.at(2) == &vec.at(2));

    expect(&vec.front() == &vec.front());
    expect(&vec.back() == &vec.back());
};


// "At throws when index equals size"_test = [] {
//     estd::vector32<Tracked> vec{3};
// 
//     expect(throws<std::out_of_range>([&] {
//         std::ignore = vec.at(3);
//     }));
// };


// "At throws for empty vector"_test = [] {
//     estd::vector32<Tracked> vec;
// 
//     expect(throws<std::out_of_range>([&] {
//         std::ignore = vec.at(0);
//     }));
// };


"At accepts every valid index"_test = [] {
    estd::vector32<Tracked> vec{10};

    for (uint32_t i = 0; i < vec.size(); ++i) {
        expect(vec.at(i) == Tracked{});
    }
};


// ============================================================================
// clear
// ============================================================================

"Clear on empty vector is harmless"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.clear();

        expect(vec.empty());
        expect(vec.size() == 0);
        expect(vec.begin() == vec.end());

        expect_stats(
            0, 0, 0, 0,
            0, 0, 0, 0
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0, 0, 0
    );
};


"Clear destroys every element"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{7};

        const auto old_capacity = vec.capacity();

        vec.clear();

        expect(vec.empty());
        expect(vec.size() == 0);
        expect(vec.capacity() == old_capacity);
        expect(vec.begin() == vec.end());

        expect_stats(
            7, 0, 0, 0,
            0, 0,
            7,
            0
        );
    }

    expect_stats(
        7, 0, 0, 0,
        0, 0,
        7,
        0
    );
};


"Vector can be reused after clear"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{5};

        vec.clear();

        expect_stats(
            5, 0, 0, 0,
            0, 0,
            5,
            0
        );

        vec.emplace_back();

        expect(vec.size() == 1);
        expect(vec.data() != nullptr);

        expect_stats(
            6, 0, 0, 0,
            0, 0,
            5,
            1
        );
    }

    expect_stats(
        6, 0, 0, 0,
        0, 0,
        6,
        0
    );
};


// ============================================================================
// emplace_back
// ============================================================================

"Emplace back default constructs an element"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        Tracked& element = vec.emplace_back();

        expect(vec.size() == 1);
        expect(&element == &vec.back());

        expect_stats(
            1, 0, 0, 0,
            0, 0, 0, 1
        );
    }

    expect_stats(
        1, 0, 0, 0,
        0, 0,
        1,
        0
    );
};


"Emplace back constructs element with arguments"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        Tracked& element = vec.emplace_back(42);

        expect(vec.size() == 1);
        expect(&element == &vec.back());
        expect(element.value == 42);

        expect_stats(
            0, 1, 0, 0,
            0, 0, 0, 1
        );
    }

    expect_stats(
        0, 1, 0, 0,
        0, 0,
        1,
        0
    );
};


"Repeated emplace back constructs every element"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        for (uint32_t i = 0; i < 10; ++i) {
            vec.emplace_back(i);
        }

        expect(vec.size() == 10);

        for (uint32_t i = 0; i < 10; ++i) {
            expect(vec[i].value == gsl::narrow_cast<int>(i));
        }

        // The exact number of reallocations is intentionally not tested.
        // We only require that all ten final objects are alive.
        expect(Tracked::stats.alive == 10);
        expect(Tracked::stats.value_ctor == 10);
    }

    expect(Tracked::stats.alive == 0);
};


// ============================================================================
// push_back
// ============================================================================

"Push back from const reference copies element"_test = [] {
    Tracked::reset();

    {
        Tracked value{42};

        estd::vector32<Tracked> vec;

        vec.push_back(value);

        expect(vec.size() == 1);
        expect(vec[0].value == 42);

        expect_stats(
            0, 1, 1, 0,
            0, 0, 0, 2
        );
    }

    expect_stats(
        0, 1, 1, 0,
        0, 0,
        2,
        0
    );
};


"Push back from rvalue move constructs element"_test = [] {
    Tracked::reset();

    {
        Tracked value{42};

        estd::vector32<Tracked> vec;

        vec.push_back(std::move(value));

        expect(vec.size() == 1);
        expect(vec[0].value == 42);

        expect_stats(
            0, 1, 0, 1,
            0, 0, 0, 2
        );
    }

    expect_stats(
        0, 1, 0, 1,
        0, 0,
        2,
        0
    );
};


"Push back preserves all existing elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.emplace_back(1);
        vec.emplace_back(2);
        vec.emplace_back(3);

        expect(vec.size() == 3);
        expect(vec[0].value == 1);
        expect(vec[1].value == 2);
        expect(vec[2].value == 3);

        expect(Tracked::stats.alive == 3);
    }

    expect(Tracked::stats.alive == 0);
};


// ============================================================================
// pop_back
// ============================================================================

"Pop back destroys exactly one element"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{3};

        expect_stats(
            3, 0, 0, 0,
            0, 0, 0, 3
        );

        vec.pop_back();

        expect(vec.size() == 2);

        expect_stats(
            3, 0, 0, 0,
            0, 0,
            1,
            2
        );
    }

    expect_stats(
        3, 0, 0, 0,
        0, 0,
        3,
        0
    );
};


"Repeated pop back destroys elements in reverse order of removal"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{5};

        while (!vec.empty()) {
            vec.pop_back();

            expect(Tracked::stats.alive ==
                   static_cast<int>(vec.size()));
        }

        expect(vec.empty());

        expect_stats(
            5, 0, 0, 0,
            0, 0,
            5,
            0
        );
    }

    expect_stats(
        5, 0, 0, 0,
        0, 0,
        5,
        0
    );
};


// ============================================================================
// resize
// ============================================================================

"Resize from empty default constructs requested number of elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.resize(5);

        expect(vec.size() == 5);
        expect(!vec.empty());

        expect_stats(
            5, 0, 0, 0,
            0, 0, 0, 5
        );
    }

    expect_stats(
        5, 0, 0, 0,
        0, 0,
        5,
        0
    );
};


"Resize upward default constructs only new elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{3};

        vec[0].value = 10;
        vec[1].value = 20;
        vec[2].value = 30;

        Tracked::reset_ops();

        vec.resize(7);

        expect(vec.size() == 7);

        expect(vec[0].value == 10);
        expect(vec[1].value == 20);
        expect(vec[2].value == 30);

        expect_stats(
            4, 0, 0, 0,
            0, 0, 0, 7
        );
    }

    expect_stats(
        4, 0, 0, 0,
        0, 0,
        7,
        0
    );
};


"Resize downward destroys removed elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{7};

        vec[0].value = 10;
        vec[1].value = 20;
        vec[2].value = 30;

        Tracked::reset_ops();

        vec.resize(3);

        expect(vec.size() == 3);
        expect(vec[0].value == 10);
        expect(vec[1].value == 20);
        expect(vec[2].value == 30);

        expect_stats(
            0, 0, 0, 0,
            0, 0, 4, 3
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0,
        7,
        0
    );
};


"Resize to same size does nothing"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{5};

        Tracked::reset_ops();

        vec.resize(5);

        expect(vec.size() == 5);

        expect_stats(
            0, 0, 0, 0,
            0, 0, 0, 5
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0,
        5,
        0
    );
};


"Resize to zero destroys every element"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{6};

        vec.resize(0);

        expect(vec.empty());
        expect(vec.size() == 0);
        expect(vec.begin() == vec.end());

        expect_stats(
            6, 0, 0, 0,
            0, 0,
            6,
            0
        );
    }

    expect_stats(
        6, 0, 0, 0,
        0, 0,
        6,
        0
    );
};


"Vector can be reused after resize to zero"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{4};

        vec.resize(0);

        expect(vec.empty());
        expect(Tracked::stats.alive == 0);

        vec.emplace_back(123);

        expect(vec.size() == 1);
        expect(vec[0].value == 123);
        expect(Tracked::stats.alive == 1);
    }

    expect(Tracked::stats.alive == 0);
};


// ============================================================================
// uninitialized_resize
// ============================================================================

"Uninitialized resize from empty creates requested objects"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.uninitialized_resize(5);

        expect(vec.size() == 5);
        expect(!vec.empty());

        // For a non-trivial type, default initialization still invokes
        // its default constructor.
        expect_stats(
            5, 0, 0, 0,
            0, 0, 0, 5
        );
    }

    expect_stats(
        5, 0, 0, 0,
        0, 0,
        5,
        0
    );
};


"Uninitialized resize upward constructs only new elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{3};

        vec[0].value = 10;
        vec[1].value = 20;
        vec[2].value = 30;

        Tracked::reset_ops();

        vec.uninitialized_resize(7);

        expect(vec.size() == 7);

        expect(vec[0].value == 10);
        expect(vec[1].value == 20);
        expect(vec[2].value == 30);

        expect_stats(
            4, 0, 0, 0,
            0, 0, 0, 7
        );
    }

    expect_stats(
        4, 0, 0, 0,
        0, 0,
        7,
        0
    );
};


"Uninitialized resize downward destroys removed elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{7};

        Tracked::reset_ops();

        vec.uninitialized_resize(2);

        expect(vec.size() == 2);

        expect_stats(
            0, 0, 0, 0,
            0, 0,
            5,
            2
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0,
        7,
        0
    );
};


"Uninitialized resize to same size does nothing"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{4};

        Tracked::reset_ops();

        vec.uninitialized_resize(4);

        expect(vec.size() == 4);

        expect_stats(
            0, 0, 0, 0,
            0, 0, 0, 4
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0,
        4,
        0
    );
};


"Uninitialized resize to zero destroys every element"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{4};

        vec.uninitialized_resize(0);

        expect(vec.empty());

        expect_stats(
            4, 0, 0, 0,
            0, 0,
            4,
            0
        );
    }

    expect_stats(
        4, 0, 0, 0,
        0, 0,
        4,
        0
    );
};


// ============================================================================
// Interactions between operations
// ============================================================================

"Reserve followed by emplace does not relocate existing elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.reserve(10);

        Tracked::reset();

        vec.emplace_back(1);
        vec.emplace_back(2);
        vec.emplace_back(3);

        expect(vec.size() == 3);
        expect(vec[0].value == 1);
        expect(vec[1].value == 2);
        expect(vec[2].value == 3);

        expect_stats(
            0, 3, 0, 0,
            0, 0, 0, 3
        );
    }

    expect_stats(
        0, 3, 0, 0,
        0, 0,
        3,
        0
    );
};


"Clear preserves reserved capacity and subsequent insertion reuses vector"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec;

        vec.reserve(10);

        const auto reserved_capacity = vec.capacity();

        vec.emplace_back(1);
        vec.emplace_back(2);
        vec.emplace_back(3);

        Tracked::reset_ops();

        vec.clear();

        expect(vec.empty());
        expect(vec.capacity() == reserved_capacity);

        expect_stats(
            0, 0, 0, 0,
            0, 0,
            3,
            0
        );

        vec.emplace_back(42);

        expect(vec.size() == 1);
        expect(vec[0].value == 42);
        expect(vec.capacity() == reserved_capacity);

        expect_stats(
            0, 1, 0, 0,
            0, 0,
            3,
            1
        );
    }

    expect_stats(
        0, 1, 0, 0,
        0, 0,
        4,
        0
    );
};


"Resize down then up creates only the newly requested elements"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> vec{6};

        vec.resize(2);

        expect_stats(
            6, 0, 0, 0,
            0, 0,
            4,
            2
        );

        Tracked::reset_ops();

        vec.resize(5);

        expect(vec.size() == 5);

        expect_stats(
            3, 0, 0, 0,
            0, 0,
            0,
            5
        );
    }

    expect_stats(
        3, 0, 0, 0,
        0, 0,
        5,
        0
    );
};


"Move assignment after clear transfers new storage correctly"_test = [] {
    Tracked::reset();

    {
        estd::vector32<Tracked> src{5};
        estd::vector32<Tracked> dst{3};

        dst.clear();

        expect(dst.empty());

        Tracked::reset_ops();

        dst = std::move(src);

        expect(dst.size() == 5);
        expect(dst.data() != nullptr);

        expect_stats(
            0, 0, 0, 0,
            0, 0,
            0,
            5
        );
    }

    expect_stats(
        0, 0, 0, 0,
        0, 0,
        5,
        0
    );
};


// ============================================================================
// Move-only / non-movable element types
// ============================================================================

"Vector supports move-only elements"_test = [] {
    estd::vector32<NonCopyable> vec;

    vec.resize(3);

    expect(vec.size() == 3);

    estd::vector32<NonCopyable> moved{std::move(vec)};

    expect(moved.size() == 3);
};


"Vector supports move-only elements with emplace"_test = [] {
    estd::vector32<NonCopyable> vec;

    vec.emplace_back();
    vec.emplace_back();
    vec.emplace_back();

    expect(vec.size() == 3);
};


"Vector supports non-movable elements when no relocation is required"_test = [] {
    estd::vector32<NonMovable> vec {3};

    expect(vec.size() == 3);
};


}
