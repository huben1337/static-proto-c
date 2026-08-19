#pragma once

#include <cassert>
#include <cstddef>
#include <gsl/pointers>
#include <iterator>
#include <concepts>
#include <type_traits>

namespace estd {

template<typename T>
struct basic_contiguous_iterator {
    using difference_type = std::ptrdiff_t;
    using size_type = size_t;
    using value_type = std::remove_cv_t<T>;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::contiguous_iterator_tag;
    using iterator_concept = std::contiguous_iterator_tag;

    template<typename>
    friend struct basic_contiguous_iterator;

private:
    pointer _ptr;
    #ifndef NDEBUG
    pointer _begin;
    pointer _end;
    #endif

    #ifndef NDEBUG

    constexpr basic_contiguous_iterator(
        pointer current,
        pointer begin,
        pointer end
    ) :
        _ptr(current),
        _begin(begin),
        _end(end)
    {}

    #endif

public:
    constexpr basic_contiguous_iterator() :
        _ptr(nullptr)
        #ifndef NDEBUG
        ,
        _begin(nullptr),
        _end(nullptr)
        #endif
    {}

    template<typename U>
    requires (!std::same_as<U, T> && std::convertible_to<U*, pointer>)
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr basic_contiguous_iterator(const basic_contiguous_iterator<U>& other) :
        _ptr(other._ptr)
        #ifndef NDEBUG
        , _begin(other._begin)
        , _end(other._end)
        #endif
    {}

    #ifdef NDEBUG
    constexpr explicit basic_contiguous_iterator(pointer current) :
        _ptr(current)
    {}

    #endif


    [[nodiscard]]
    constexpr static basic_contiguous_iterator at_range_end(
        #ifdef NDEBUG
        [[maybe_unused]]
        #endif
        pointer begin,
        pointer end
    ) {
        #ifdef NDEBUG
        return basic_contiguous_iterator{end};
        #else
        assert(begin <= end);
        return {end, begin, end};
        #endif
    }

    [[nodiscard]]
    constexpr static basic_contiguous_iterator at_range_begin(
        pointer begin,
        #ifdef NDEBUG
        [[maybe_unused]]
        #endif
        pointer end
    ) {
        #ifdef NDEBUG
        return basic_contiguous_iterator{begin};
        #else
        assert(begin <= end);
        return {begin, begin, end};
        #endif
    }

    [[nodiscard]]
    constexpr reference operator*() const { return *_ptr; }

    constexpr basic_contiguous_iterator& operator++() {
        assert(_ptr < _end);
        ++_ptr;
        return *this;
    }

    constexpr basic_contiguous_iterator operator++(int) {
        assert(_ptr < _end);
        return basic_contiguous_iterator{
            _ptr++
            #ifndef NDEBUG
            ,
            _begin,
            _end
            #endif
        };
    }

    constexpr basic_contiguous_iterator& operator--() {
        assert(_ptr > _begin);
        --_ptr;
        return *this;
    }

    constexpr basic_contiguous_iterator operator--(int) {
        assert(_ptr > _begin);
        return basic_contiguous_iterator{
            _ptr--
            #ifndef NDEBUG
            ,
            _begin,
            _end
            #endif
        };
    }

    template<typename U>
    requires (std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<U>>)
    [[nodiscard]]
    constexpr auto operator<=>(const basic_contiguous_iterator<U>& other) const {
        return _ptr <=> other._ptr;
    }

    template<typename U>
    requires (std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<U>>)
    [[nodiscard]]
    constexpr bool operator==(const basic_contiguous_iterator<U>& other) const {
        return _ptr == other._ptr;
    }

    template<typename U>
    requires (std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<U>>)
    [[nodiscard]]
    constexpr difference_type operator-(const basic_contiguous_iterator<U>& other) const {
        return _ptr - other._ptr;
    }

private:
    template<typename Num>
    [[nodiscard]]
    constexpr basic_contiguous_iterator subtract(const Num n) const {
        pointer new_ptr = _ptr - n;
        assert(new_ptr >= _begin);
        if constexpr (std::is_signed_v<Num>) {
            assert(new_ptr <= _end);
        }
        return basic_contiguous_iterator{
            new_ptr
            #ifndef NDEBUG
            ,
            _begin,
            _end
            #endif
        };
    }

    template<typename Num>
    [[nodiscard]]
    constexpr basic_contiguous_iterator& decrement(const Num n) {
        _ptr -= n;
        assert(_ptr >= _begin);
        if constexpr (std::is_signed_v<Num>) {
            assert(_ptr <= _end);
        }
        return *this;
    }

public:
    template<std::integral Num>
    [[nodiscard]]
    constexpr basic_contiguous_iterator operator-(const Num n) const {
        return subtract(n);
    }

    template<std::integral Num>
    [[nodiscard]]
    constexpr basic_contiguous_iterator& operator-=(const Num n) {
        return decrement(n);
    }

private:
    template<typename Num>
    [[nodiscard]]
    constexpr basic_contiguous_iterator add(const Num n) const {
        pointer new_ptr = _ptr + n;
        assert(new_ptr <= _end);
        if constexpr (std::is_signed_v<Num>) {
            assert(new_ptr >= _begin);
        }
        return basic_contiguous_iterator{
            new_ptr
            #ifndef NDEBUG
            ,
            _begin,
            _end
            #endif
        };
    }

    template<typename Num>
    [[nodiscard]]
    constexpr basic_contiguous_iterator& increment(const Num n) {
        _ptr += n;
        assert(_ptr <= _end);
        if constexpr (std::is_signed_v<Num>) {
            assert(_ptr >= _begin);
        }
        return *this;
    }

public:
    template<std::integral Num>
    [[nodiscard]]
    constexpr basic_contiguous_iterator operator+(const Num n) const {
        return add(n);
    }

    template<std::integral Num>
    [[nodiscard]]
    friend constexpr basic_contiguous_iterator operator+(const Num n, const basic_contiguous_iterator& i) {
        return i.add(n);
    }

    template<std::integral Num>
    [[nodiscard]]
    constexpr basic_contiguous_iterator& operator+=(const Num n) {
        return increment(n);
    }
private:
    template<typename Num>
    [[nodiscard]]
    constexpr reference at(Num i) const {
        #ifdef NDEBUG
        return _ptr[i];
        #else
        pointer accessed = _ptr + i;
        assert(accessed < _end);
        if constexpr (std::is_signed_v<Num>) {
            assert(accessed >= _begin);
        }
        return *accessed;
        #endif
    }

public:
    template<std::integral Num>
    [[nodiscard]]
    constexpr reference operator[](const Num i) const {
        return at(i);
    }

    [[nodiscard]]
    constexpr pointer operator->() const {
        return _ptr;
    }

    // [[nodiscard]]
    // constexpr operator basic_contiguous_iterator<const T>()
    //     const requires(!std::is_const_v<T>)
    // {
    //     return basic_contiguous_iterator<const T>{
    //         _ptr
    //         #ifndef NDEBUG
    //         ,
    //         _begin,
    //         _end
    //         #endif
    //     };
    // }
};


static_assert(std::contiguous_iterator<basic_contiguous_iterator<int>>);
static_assert(std::contiguous_iterator<basic_contiguous_iterator<const int>>);
static_assert(std::contiguous_iterator<basic_contiguous_iterator<volatile int>>);
static_assert(std::contiguous_iterator<basic_contiguous_iterator<const volatile int>>);

static_assert(std::is_convertible_v<basic_contiguous_iterator<int>, basic_contiguous_iterator<int>>);
static_assert(std::is_convertible_v<basic_contiguous_iterator<int>, basic_contiguous_iterator<const int>>);

using test_iterator = estd::basic_contiguous_iterator<int>;
using test_const_iterator = estd::basic_contiguous_iterator<const int>;

static_assert(std::random_access_iterator<test_iterator>);
static_assert(std::bidirectional_iterator<test_iterator>);
static_assert(std::forward_iterator<test_iterator>);

static_assert(std::contiguous_iterator<test_iterator>);
static_assert(std::contiguous_iterator<test_const_iterator>);

static_assert(std::constructible_from<
    test_const_iterator,
    const test_iterator&
>);

static_assert(std::copy_constructible<test_const_iterator>);

static_assert(!std::constructible_from<
    test_iterator,
    const test_const_iterator&
>);

static_assert(std::convertible_to<
    test_iterator,
    test_const_iterator
>);

static_assert(!std::convertible_to<
    test_const_iterator,
    test_iterator
>);

static_assert(requires(test_iterator i, test_const_iterator ci) {
    i == ci;
    ci == i;
    i <=> ci;
    ci <=> i;
    i - ci;
    ci - i;
});

} // namespace estd