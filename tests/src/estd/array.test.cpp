#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <boost/ut.hpp>
#include <utility>
#include "../../../src/estd/array.hpp"
#include "../../helpers.hpp"
#include "../../utility.hpp"

using namespace test;
using namespace boost::ut;



int main () {

"Default construction creates empty array"_test = [] {
    {
        estd::array<TrivialValue> arr;
        expect(arr.empty());
        expect(arr.size() == 0);
        expect(arr.begin() == arr.end());
        expect(arr.data() == nullptr);
    }
    {
        estd::array<TrivialValue> arr {};
        expect(arr.empty());
        expect(arr.size() == 0);
        expect(arr.begin() == arr.end());
        expect(arr.data() == nullptr);
    }
};

"Array constructed with zero size is empty"_test = [] {
    estd::array<TrivialValue> arr {0};
    expect(arr.empty());
    expect(arr.size() == 0);
    expect(arr.begin() == arr.end());
    expect(arr.data() == nullptr);
};

"Array constructed with zero size and fill value is empty"_test = [] {
    estd::array<TrivialValue> arr {0, TrivialValue{63}};
    expect(arr.empty());
    expect(arr.size() == 0);
    expect(arr.begin() == arr.end());
    expect(arr.data() == nullptr);
};

"Array constructed with size N has size N"_test = [] {
    for (auto request_size : std::views::concat(
        std::views::iota(size_t{1}, size_t{20}),
        std::to_array<size_t>({101, 1001, 100001})
    )) {
        estd::array<TrivialValue> arr {request_size};
        expect(!arr.empty());
        expect(arr.size() == request_size);
        expect(arr.begin() + request_size == arr.end());
        expect(arr.data() != nullptr);
    }
};

"Array constructed with size N and fill value has size N"_test = [] {
    for (auto request_size : std::views::concat(
        std::views::iota(size_t{1}, size_t{20}),
        std::to_array<size_t>({101, 1001, 100001})
    )) {
        estd::array<TrivialValue> arr {request_size, TrivialValue{63}};
        expect(!arr.empty());
        expect(arr.size() == request_size);
        expect(arr.begin() + request_size == arr.end());
        expect(arr.data() != nullptr);
    }
};

"Array constructed with size N and trivial fill value is filled"_test = [] {
    for (auto request_size : std::views::concat(
        std::views::iota(size_t{0}, size_t{20}),
        std::to_array<size_t>({101, 1001, 100001})
    )) {
        TrivialValue fill_value {63};
        estd::array<TrivialValue> arr {request_size, fill_value};
        expect(std::ranges::all_of(
            arr,
            [&](auto&& x) { return x == fill_value; }
        ));
    }
};

"Size constructor default constructs every element"_test = [] {
    Tracked::reset();

    {
        const int n = 5;
        estd::array<Tracked> arr{n};

        expect(arr.size() == n);

        expect_stats(
            n,  // default_ctor
            0,  // value_ctor
            0,  // copy_ctor
            0,  // move_ctor
            0,  // copy_assign
            0,  // move_assign
            0,  // dtor
            n   // alive,
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

"Fill constructor copies fill value"_test = [] {
    Tracked::reset();

    {
        Tracked fill{42};

        expect_stats(
            0,  // default_ctor
            1,  // value_ctor
            0,  // copy_ctor
            0,  // move_ctor
            0,  // copy_assign
            0,  // move_assign
            0,  // dtor
            1   // alive
        );

        const int n = 4;
        estd::array<Tracked> arr{n, fill};

        expect_stats(
            0,  // default_ctor
            1,  // value_ctor
            n,  // copy_ctor
            0,  // move_ctor
            0,  // copy_assign
            0,  // move_assign
            0,  // dtor
            n + 1
        );
    }

    expect_stats(
        0,
        1,
        4,
        0,
        0,
        0,
        5,
        0
    );
};

"Every constructed element is destroyed exactly once"_test = [] {
    Tracked::reset();

    {
        const int n = 17;
        estd::array<Tracked> arr{n};

        expect_stats(
            n, 0, 0, 0,
            0, 0, 0, n
        );
    }

    expect_stats(
        17, 0, 0, 0,
        0, 0, 17, 0
    );
};

"Copy constructing array copies every element"_test = [] {
    const int n = 13;

    Tracked::reset();

    {
        Tracked fill_value{9};

        estd::array<Tracked> original{n, fill_value};

        expect_stats(
            0,       // default_ctor
            1,       // value_ctor
            n,       // copy_ctor
            0,       // move_ctor
            0,       // copy_assign
            0,       // move_assign
            0,       // dtor
            n + 1    // alive
        );

        estd::array<Tracked> copy{original};

        expect_stats(
            0,
            1,
            2 * n,
            0,
            0,
            0,
            0,
            (2 * n) + 1
        );

        expect(copy.size() == original.size());
        expect(std::ranges::equal(original, copy));
    }

    expect_stats(
        0,
        1,
        2 * n,
        0,
        0,
        0,
        (2 * n) + 1,
        0
    );
};

"Copy assignment copies every element"_test = [] {
    Tracked::reset();

    const int src_n = 6;
    const int dst_n = 7;

    {
        const Tracked fill_value{7};

        estd::array<Tracked> src{src_n, fill_value};
        estd::array<Tracked> dst{dst_n};

        expect_stats(
            dst_n,   // default_ctor
            1,       // value_ctor
            src_n,   // copy_ctor
            0,       // move_ctor
            0,       // copy_assign
            0,       // move_assign
            0,       // dtor
            src_n + dst_n + 1
        );

        dst = src;

        expect_stats(
            dst_n,
            1,
            2 * src_n,
            0,
            0,
            0,
            dst_n,
            (2 * src_n) + 1
        );

        expect(dst.size() == src_n);
        expect(std::ranges::equal(src, dst));
    }

    expect_stats(
        dst_n,
        1,
        2 * src_n,
        0,
        0,
        0,
        (2 * src_n) + dst_n + 1,
        0
    );
};

"Self copy assignment is safe"_test = [] {
    const int n = 3;

    Tracked::reset();

    {
        estd::array<Tracked> arr{n};

        arr[0].value = 1;
        arr[1].value = 2;
        arr[2].value = 3;

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

        NOWARN(-Wself-assign-overloaded,
            arr = arr;
        )

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

        expect(arr[0].value == 1);
        expect(arr[1].value == 2);
        expect(arr[2].value == 3);
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

"Move constructing array moves only the array storage"_test = [] {
    Tracked::reset();
    
    const int n = 4;

    {
        const Tracked fill_value{11};
        estd::array<Tracked> src{n, fill_value};

        expect_stats(
            0,
            1,
            n,
            0,
            0,
            0,
            0,
            n + 1
        );

        estd::array<Tracked> moved{std::move(src)};

        // Moving the array does not move-construct individual elements.
        // Ownership of the existing elements is transferred.
        expect_stats(
            0,
            1,
            n,
            0,
            0,
            0,
            0,
            n + 1
        );

        expect(moved.size() == n);
        expect(std::ranges::all_of(
            moved,
            [&](auto&& x) { return x == fill_value; }
        ));
    }

    expect_stats(
        0,
        1,
        n,
        0,
        0,
        0,
        n + 1,
        0
    );
};

"Move assignment moves only the array storage"_test = [] {
    Tracked::reset();

    const int src_n = 6;
    const int dst_n = 5;

    {
        const Tracked fill_value{22};
        estd::array<Tracked> src{src_n, fill_value};
        estd::array<Tracked> dst{dst_n};

        expect_stats(
            dst_n,
            1,
            src_n,
            0,
            0,
            0,
            0,
            src_n + dst_n + 1
        );

        dst = std::move(src);

        expect_stats(
            dst_n,
            1,
            src_n,
            0,
            0,
            0,
            dst_n,
            src_n + 1
        );

        expect(dst.size() == src_n);
        expect(std::ranges::all_of(
            dst,
            [&](auto&& x) { return x == fill_value; }
        ));
    }

    expect_stats(
        dst_n,
        1,
        src_n,
        0,
        0,
        0,
        src_n + dst_n + 1,
        0
    );
};

"Self move assignment is safe"_test = [] {
    const int n = 2;

    Tracked::reset();

    {
        estd::array<Tracked> arr{n};

        arr[0].value = 9;
        arr[1].value = 8;

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

        NOWARN(-Wself-move,
            arr = std::move(arr);
        )

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

        expect(arr.size() == n);
        expect(arr[0].value == 9);
        expect(arr[1].value == 8);
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

"Mutable iterators access every element"_test = [] {
    estd::array<Tracked> arr{5};

    int value = 1;
    for (auto & it : arr) {
        it.value = value++;
    }

    value = 1;
    for (auto & it : arr) {
        expect(it.value == value++);
    }
};

"Const iterators read every element"_test = [] {
    estd::array<Tracked> arr{3};

    arr[0].value = 4;
    arr[1].value = 5;
    arr[2].value = 6;

    const auto& c = arr;

    expect(c.begin()->value == 4);
    expect((c.begin()+1)->value == 5);
    expect((c.end()-1)->value == 6);
};

"Explicit const iterators read every element"_test = [] {
    estd::array<Tracked> arr{3};

    arr[0].value = 4;
    arr[1].value = 5;
    arr[2].value = 6;

    expect(arr.cbegin()->value == 4);
    expect((arr.cbegin() + 1)->value == 5);
    expect((arr.cend() -1 )->value == 6);
};

"Mutable at access method modifies elements"_test = [] {
    estd::array<Tracked> arr{2};

    arr.at(0).value = 15;
    arr.at(1).value = 25;

    expect(arr.at(0).value == 15);
    expect(arr.at(1).value == 25);
};

"Const at access method reads elements"_test = [] {
    estd::array<Tracked> arr{2};

    arr.at(0).value = 7;
    arr.at(1).value = 9;

    const auto& c = arr;

    expect(c.at(0).value == 7);
    expect(c.at(1).value == 9);
};


"Mutable subscript modifies elements"_test = [] {
    estd::array<Tracked> arr{2};

    arr[0].value = 15;
    arr[1].value = 25;

    expect(arr[0].value == 15);
    expect(arr[1].value == 25);
};

"Const subscript reads elements"_test = [] {
    estd::array<Tracked> arr{2};

    arr[0].value = 7;
    arr[1].value = 9;

    const auto& c = arr;

    expect(c[0].value == 7);
    expect(c[1].value == 9);
};

"Begin and end reference the correct objects"_test = [] {
    estd::array<Tracked> arr{3};

    expect(&*arr.begin() == &arr[0]);
    expect(&*(arr.end() - 1) == &arr[2]);
};

"Const begin and end reference the correct objects"_test = [] {
    const estd::array<Tracked> arr{3};

    expect(&*arr.begin() == &arr[0]);
    expect(&*(arr.end() - 1) == &arr[2]);
};

"CBegin and cend reference the correct objects"_test = [] {
    estd::array<Tracked> arr{3};

    expect(&*arr.cbegin() == &arr[0]);
    expect(&*(arr.cend() - 1) == &arr[2]);
};

"Front and back always return the same object refrence"_test = [] {
    estd::array<Tracked> arr{3};

    expect(&arr.front() == &arr.front());
    expect(&arr.back() == &arr.back());
};

"Const dront and back always return the same object refrence"_test = [] {
    estd::array<Tracked> arr{3};
    const auto& c = arr;

    expect(&c.front() == &c.front());
    expect(&c.back() == &c.back());
};


"Subscript always returns the same object reference"_test = [] {
    estd::array<Tracked> arr{3};

    expect(&arr[0] == &arr[0]);
    expect(&arr[1] == &arr[1]);
    expect(&arr[2] == &arr[2]);
};

"Const subscript always returns the same object reference"_test = [] {
    const estd::array<Tracked> arr{3};

    expect(&arr[0] == &arr[0]);
    expect(&arr[1] == &arr[1]);
    expect(&arr[2] == &arr[2]);
};

"At always returns the same object reference"_test = [] {
    estd::array<Tracked> arr{3};

    expect(&arr.at(0) == &arr.at(0));
    expect(&arr.at(1) == &arr.at(1));
    expect(&arr.at(2) == &arr.at(2));
};

"Const at always returns the same object reference"_test = [] {
    const estd::array<Tracked> arr{3};

    expect(&arr.at(0) == &arr.at(0));
    expect(&arr.at(1) == &arr.at(1));
    expect(&arr.at(2) == &arr.at(2));
};

"Front and back always return the same object reference"_test = [] {
    estd::array<Tracked> arr{3};

    expect(&arr.front() == &arr.front());
    expect(&arr.back() == &arr.back());
};

"Const front and back always return the same object reference"_test = [] {
    const estd::array<Tracked> arr{3};

    expect(&arr.front() == &arr.front());
    expect(&arr.back() == &arr.back());
};

"All element accessors reference the same objects"_test = [] {
    estd::array<Tracked> arr{3};

    expect(&arr[0] == &arr.at(0));
    expect(&arr[0] == &arr.front());
    expect(&arr[0] == arr.data());
    expect(&arr[0] == &*arr.begin());

    expect(&arr[2] == &arr.at(2));
    expect(&arr[2] == &arr.back());
    expect(&arr[2] == &*(arr.end() - 1));
};

"All const element accessors reference the same objects"_test = [] {
    const estd::array<Tracked> arr{3};

    expect(&arr[0] == &arr.at(0));
    expect(&arr[0] == &arr.front());
    expect(&arr[0] == arr.data());
    expect(&arr[0] == &*arr.begin());
    expect(&arr[0] == &*arr.cbegin());

    expect(&arr[2] == &arr.at(2));
    expect(&arr[2] == &arr.back());
    expect(&arr[2] == &*(arr.end() - 1));
    expect(&arr[2] == &*(arr.cend() - 1));
};

"Front and back reference the first and last elements"_test = [] {
    estd::array<Tracked> arr{3};

    expect(&arr.front() == &arr[0]);
    expect(&arr.back() == &arr[2]);
};

"Const front and back reference the first and last elements"_test = [] {
    const estd::array<Tracked> arr{3};

    expect(&arr.front() == &arr[0]);
    expect(&arr.back() == &arr[2]);
};

"Data returns the address of the first element"_test = [] {
    estd::array<Tracked> arr{3};

    expect(arr.data() == &arr[0]);
};

"Const data returns the address of the first element"_test = [] {
    const estd::array<Tracked> arr{3};

    expect(arr.data() == &arr[0]);
};

"Front and back modify elements"_test = [] {
    estd::array<Tracked> arr{3};

    arr.front().value = 100;
    arr.back().value = 300;

    expect(arr[0].value == 100);
    expect(arr[2].value == 300);
};

"Const front and back read elements"_test = [] {
    estd::array<Tracked> arr{3};

    arr.front().value = 5;
    arr.back().value = 8;

    const auto& c = arr;

    expect(c.front().value == 5);
    expect(c.back().value == 8);
    expect(&c.front() == &c.front());
    expect(&c.back() == &c.back());
};

"Data pointer references first element"_test = [] {
    estd::array<Tracked> arr{4};

    arr[0].value = 123;

    expect(arr.data() == &arr[0]);
    expect(arr.data()->value == 123);
};

"Const data pointer references first element"_test = [] {
    estd::array<Tracked> arr{2};

    arr[0].value = 77;

    const auto& c = arr;

    expect(c.data() == &c[0]);
    expect(c.data()->value == 77);
};

"Array supports move-only types"_test = [] {
    estd::array<NonCopyable> arr{3};
    estd::array<NonCopyable> moved{std::move(arr)};
    expect(moved.size() == 3);
};

"Array supports copy-only types"_test = [] {
    estd::array<NonMovable> arr{3};
    estd::array<NonMovable> copy{arr};
    expect(copy.size() == 3);
};

}