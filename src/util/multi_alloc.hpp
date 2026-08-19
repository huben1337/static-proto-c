#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../estd/utility.hpp"
#include "../estd/empty.hpp"
#include "../estd/array.hpp"

namespace detail {

template<typename T, size_t _extent, bool fill>
struct alloc_info;

template<typename T, size_t _extent>
struct alloc_info<T, _extent, false> {
    using type = T;
    static constexpr size_t extent = _extent;
    static constexpr bool fill = false;

    [[nodiscard]] constexpr size_t size() const { return _extent; }
};

template<typename T>
struct alloc_info<T, std::dynamic_extent, false> {
    using type = T;
    static constexpr size_t extent = std::dynamic_extent;
    static constexpr bool fill = false;
    
private:
    size_t _size;

public:
    [[nodiscard]] constexpr size_t size() const { return _size; }

    constexpr explicit alloc_info(size_t size) : _size(size) {}
};

template<typename T, size_t _extent>
struct alloc_info<T, _extent, true> {
    using type = T;
    static constexpr size_t extent = _extent;
    static constexpr bool fill = true;

private:
    T _v;

public:
    constexpr explicit alloc_info(const T& v) : 
        _v(v)
    {}

    [[nodiscard]] constexpr size_t size() const { return _extent; }
    [[nodiscard]] constexpr const T& value() const { return _v; }
};

template<typename T>
struct alloc_info<T, std::dynamic_extent, true> {
    using type = T;
    static constexpr size_t extent = std::dynamic_extent;
    static constexpr bool fill = true;
    
private:
    size_t _size;
    T _v;

public:

    constexpr explicit alloc_info(size_t size, const T& v) : 
        _size(size),
        _v(v)
    {}

    [[nodiscard]] constexpr size_t size() const { return _size; }
    [[nodiscard]] constexpr const T& value() const { return _v; }
};

}

// template<typename T, size_t extents>
// alloc_req_info(const T&) -> alloc_req_info<T, extents, true>;


template<typename... Allocs>
struct multi_alloc {
private:
    static constexpr size_t allocs_count = sizeof...(Allocs);
    static constexpr auto indecies = std::make_index_sequence<allocs_count>{};
    static constexpr size_t alignment = std::max({alignof(typename Allocs::type)...});

    static constexpr bool fill_requires_guard = ((
        Allocs::fill &&
        (!std::is_trivially_constructible_v<
            typename Allocs::type,
            const typename Allocs::type&>
        || !std::is_trivially_assignable_v<
            typename Allocs::type&,
            const typename Allocs::type&>)
    ) || ...);

    static constexpr bool default_construct_requires_guard = ((
        !Allocs::fill &&
        !std::is_trivially_destructible_v<typename Allocs::type>
    ) || ...);

    static constexpr bool requires_guard = fill_requires_guard || default_construct_requires_guard;

    // static_assert(!requires_guard);

    struct alignas(alignment) Allocated {
        
    };

    static_assert(
        std::is_trivially_default_constructible_v<Allocated> &&
        std::is_trivially_copyable_v<Allocated>
    );

    static_assert(((alignof(Allocated) >= alignof(typename Allocs::type)) && ...));
    static_assert(sizeof(Allocated) == alignof(Allocated));

    using AllocatedTuple = std::tuple<std::span<typename Allocs::type, Allocs::extent>...>;

    estd::array<Allocated> _data;
    AllocatedTuple _allocated;

    template<typename T, size_t extent, bool fill>
    static constexpr void construct_allocation(
        std::span<T, extent> allocation,
        [[maybe_unused]] const detail::alloc_info<T, extent, fill>& alloc
    ) {
        if constexpr (fill) {
            std::ranges::uninitialized_fill(allocation, alloc.value());
        } else {
            if constexpr (!std::is_trivially_default_constructible_v<T>
                || !std::is_trivially_copyable_v<T>) {
                std::ranges::uninitialized_default_construct(allocation);
            }
        }
    }

    template<size_t... Is>
    static constexpr void construct_allocations(
        AllocatedTuple& allocated,
        const Allocs&... allocs,
        std::index_sequence<Is...> /*unused*/
    ) {
        if constexpr (requires_guard) {
            destruction_guard guard {allocated};
            ((construct_allocation(std::get<Is>(allocated), allocs), guard.next()), ...);
        } else {
            (construct_allocation(std::get<Is>(allocated), allocs), ...);
        }
    }

    template<typename T, size_t extent>
    static constexpr void destruct_allocation(std::span<T, extent> allocation) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            std::ranges::destroy(allocation);
        }
    }

    template<size_t... Is>
    static constexpr void destroy_allocations(
        const AllocatedTuple& allocated,
        std::index_sequence<Is...> /*unused*/
    ) {
        constexpr size_t last = sizeof...(Is) - 1;
        (destruct_allocation(std::get<last - Is>(allocated)), ...);
    }

    struct destruction_guard {
    private:
        size_t _constructed = 0;
        AllocatedTuple& _allocated;

        template<size_t... Is>
        constexpr void destroy_constructed_allocations(std::index_sequence<Is...> /*unused*/) const {
            const bool matched_count = (
                ... || (Is == _constructed
                    ? (destroy_allocations(_allocated, std::make_index_sequence<Is>{}), true)
                    : false)
            );
            assert(matched_count);
        }

    public:
        constexpr explicit destruction_guard(AllocatedTuple& allocated) :
            _allocated(allocated)
        {}

        destruction_guard(const destruction_guard&) = delete;
        destruction_guard(destruction_guard&&) = delete;

        destruction_guard& operator=(const destruction_guard&) = delete;
        destruction_guard& operator=(destruction_guard&&) = delete;

        ~destruction_guard() {
            if (_constructed == allocs_count) return;
            destroy_constructed_allocations(std::make_index_sequence<allocs_count - 1>{});
        }

        constexpr void next() { _constructed++; }
    };

    template<typename First, typename... Rest>
    [[nodiscard]] static constexpr AllocatedTuple make_allocated_tuple(
        Allocated* ptr,
        std::pair<const First&, size_t> first_allc,
        std::pair<const Rest&, size_t>... rest_allocs
    ) {
        return AllocatedTuple{
            std::span<typename First::type, First::extent>{
                std::assume_aligned<alignof(typename First::type)>(estd::ptr_cast<typename First::type>(ptr)),
                (ptr += first_allc.second, first_allc.first.size())
            },
            (
                std::span<typename Rest::type, Rest::extent>{
                    std::assume_aligned<alignof(typename Rest::type)>(estd::ptr_cast<typename Rest::type>(ptr)),
                    (ptr += rest_allocs.second, rest_allocs.first.size())
                }
            )...
        };
    }

    constexpr explicit multi_alloc(std::pair<const Allocs&, size_t>... allocs, estd::empty /*unused*/) :
        _data(estd::array<Allocated>{(allocs.second + ...)}),
        _allocated(make_allocated_tuple(_data.data(), allocs...))
    {
        construct_allocations(_allocated, allocs.first..., indecies);
    }

public:
    constexpr explicit multi_alloc(
        const detail::alloc_info<
            typename Allocs::type,
            Allocs::extent,
            Allocs::fill
        >&... allocs
    ) :
        multi_alloc{std::pair<const Allocs&, size_t>{
            allocs,
            ((allocs.size() * sizeof(typename Allocs::type)) + sizeof(Allocated) - 1) / sizeof(Allocated)
        } ..., estd::empty{}}
    {}
    
    constexpr multi_alloc(const multi_alloc&) = delete;

    constexpr multi_alloc(multi_alloc&&) = default;

    constexpr multi_alloc& operator=(const multi_alloc&) = delete;

    constexpr multi_alloc& operator=(multi_alloc&& other) {
        if (this == &other) return *this;

        if (_data.data() != nullptr) {
            destroy_allocations(_allocated, indecies);
        }

        _data = std::move(other._data);
        _allocated = other._allocated;

        return *this;
    }

    constexpr ~multi_alloc() {
        if (_data.data() != nullptr) {
            destroy_allocations(_allocated, indecies);
        }
    }

    [[nodiscard]] constexpr const AllocatedTuple& allocated() const { return _allocated; }
};

template<>
struct multi_alloc<> {
private:
    std::tuple<> _allocated;
public:
    [[nodiscard]] const std::tuple<>& allocated() const { return _allocated; }
};

template<typename... Ts, size_t... extents, bool... fills>
multi_alloc(detail::alloc_info<Ts, extents, fills>...) -> multi_alloc<detail::alloc_info<Ts, extents, fills>...>;

template<typename T>
[[nodiscard]] constexpr auto alloc(size_t size) {
    return detail::alloc_info<T, std::dynamic_extent, false>{size};
}

template<typename T, size_t extnet>
[[nodiscard]] constexpr auto alloc() {
    return detail::alloc_info<T, extnet, false>{};
}
template<typename T>
[[nodiscard]] constexpr auto alloc(size_t size, const T& v) {
    return detail::alloc_info<T, std::dynamic_extent, true>{size, v};
}

template<typename T, size_t extnet>
[[nodiscard]] constexpr auto alloc(const T& v) {
    return detail::alloc_info<T, extnet, true>{v};
}
