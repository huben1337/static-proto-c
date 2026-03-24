#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <gsl/util>
#include <limits>
#include <memory>
#include <gsl/pointers>
#include <ranges>

#include "../util/logger.hpp"
#include "../estd/type_traits.hpp"

template <std::unsigned_integral U>
struct MemoryTypesBase {
    using index_t = U;

    template <typename T>
    struct Index {
        constexpr Index () = default;
        constexpr explicit Index (U value) : value(value) {}

        U value;

        [[nodiscard]] constexpr Index add (U offset) const {
            return Index{value + offset};
        }

        [[nodiscard]] constexpr Index sub (U offset) const {
            return Index{value - offset};
        }

        // NOLINTNEXTLINE(google-explicit-constructor)
        [[nodiscard]] constexpr operator Index<const T>() const {
            return Index<const T>{value};
        }
    };

    template <typename T>
    struct Span {
        constexpr Span () = default;
        constexpr Span (Index<T> start_idx, Index<T> end_idx) : start_idx(start_idx), end_idx(end_idx) {}
        constexpr Span (Index<T> start_idx, U length) : start_idx(start_idx), end_idx(start_idx.add(length)) {}

        Index<T> start_idx;
        Index<T> end_idx;

        [[nodiscard]] constexpr U size () const {
            return end_idx.value - start_idx.value;
        }

        [[nodiscard]] constexpr bool empty () const {
            return start_idx.value == end_idx.value;
        }

        // NOLINTNEXTLINE(google-explicit-constructor)
        [[nodiscard]] constexpr operator Span<const T>() const {
            return Span<const T>{start_idx, end_idx};
        }
    };

    template <typename T>
    struct View {
        using length_t = estd::fitting_uint_t<std::numeric_limits<U>::max() / sizeof(T)>;

        constexpr View () = default;
        constexpr View (Index<T> start_idx, length_t length) : start_idx(start_idx), length(length) {}
        constexpr View (Index<T> start_idx, Index<T> end_idx) : start_idx(start_idx), length((end_idx.value - start_idx.value) / sizeof(T)) {}
        Index<T> start_idx;
        length_t length;

        [[nodiscard]] constexpr U size () const {
            return length * sizeof(T);
        }

        [[nodiscard]] constexpr bool empty () const {
            return length == 0;
        }

        // NOLINTNEXTLINE(google-explicit-constructor)
        [[nodiscard]] constexpr operator View<const T>() const {
            return View<const T>{start_idx, length};
        }
    };
};

template <typename Derived, std::unsigned_integral U>
struct MemoryBase : MemoryTypesBase<U> {
    using Base = MemoryTypesBase<U>;

    friend Derived;
    friend Base;

    template <typename T>
    using Index = Base::template Index<T>;

    template <typename T>
    using Span = Base::template Span<T>;

    template <typename T>
    using View = Base::template View<T>;

private:
    template <typename T>
    [[nodiscard]] constexpr T* _get_unaligned (this const Derived& self, const Index<T> index) {
        return reinterpret_cast<T*>(self.data() + index.value);
    }

    template <typename T>
    [[nodiscard]] constexpr T* _get (this const Derived& self, const Index<T> index) {
        return std::assume_aligned<alignof(T), T>(self.template _get_unaligned<T>(index));
    }

public:
    template <typename T>
    [[nodiscard]] constexpr T& get_unaligned (this const Derived& self, const Index<T> index) {
        return *self.template _get_unaligned<T>(index);
    }

    template <typename T>
    [[nodiscard]] constexpr T& get (this const Derived& self, const Index<T> index) {
        return *self.template _get<T>(index);
    }

    template <typename T>
    [[nodiscard]] constexpr std::ranges::subrange<T*> get_unaligned (this const Derived& self, const Span<T> span) {
        return {self.template _get_unaligned<T>(span.start_idx), self.template _get_unaligned<T>(span.end_idx)};
    }

    template <typename T>
    [[nodiscard]] constexpr std::ranges::subrange<T*> get (this const Derived& self, const Span<T> span) {
        return {self.template _get<T>(span.start_idx), self.template _get<T>(span.end_idx)};
    }

    template <typename T>
    [[nodiscard]] constexpr std::span<T> get_unaligned (this const Derived& self, const View<T> view) {
        return {self.template _get_unaligned<T>(view.start_idx), view.length};
    }

    template <typename T>
    [[nodiscard]] constexpr std::span<T> get (this const Derived& self, const View<T> view) {
        return {self.template _get<T>(view.start_idx), view.length};
    }
};

template <std::unsigned_integral U>
struct ReadOnlyMemory;

template <std::unsigned_integral U, typename AllocatedT>
struct Memory : MemoryBase<Memory<U, AllocatedT>, U> {
    using Base = MemoryBase<Memory<U, AllocatedT>, U>;

protected:
    static constexpr U max_position = std::numeric_limits<U>::max();
    static constexpr uint8_t grow_factor = 2;
    static constexpr size_t alignment = std::max(alignof(max_align_t), alignof(AllocatedT));

    U _capacity;
    U _position = 0;
    gsl::owner<AllocatedT*> _data;
    bool _in_heap;

public:
    template <typename T>
    using Index = Base::template Index<T>;

    template <typename T>
    using Span = Base::template Span<T>;

    template <typename T>
    using View = Base::template View<T>;

private:
    [[gnu::always_inline]] static constexpr gsl::owner<AllocatedT*> alloc_handled (U capacity) {
        gsl::owner<AllocatedT*> allocated;
        if constexpr (alignment > alignof(max_align_t)) {
            allocated = static_cast<gsl::owner<AllocatedT*>>(std::aligned_alloc(alignment, capacity));
        } else {
            allocated = static_cast<gsl::owner<AllocatedT*>>(std::malloc(capacity));
        }

        BSSERT(allocated != nullptr, "[Memory::create_data] Memory allocation failed.")

        return allocated;
    }

public:
    constexpr explicit Memory (U capacity) : _capacity(capacity), _data(alloc_handled(capacity)), _in_heap(true) {}

    template <U N>
    constexpr explicit Memory (AllocatedT (&data)[N]) : _capacity(sizeof(AllocatedT) * N), _data(data), _in_heap(false) {
        static_assert(sizeof(AllocatedT) * N <= max_position, "capacity overflow");
    }

private:
    constexpr Memory (AllocatedT* data, U capacity) : _capacity(capacity), _data(data), _in_heap(false) {}

public:
    static constexpr Memory from_stack (AllocatedT* data, U capacity) {
        return Memory{data, capacity};
    }

    constexpr Memory (const Memory&) = delete;
    constexpr Memory& operator = (const Memory&) = delete;

    constexpr Memory (Memory&& other) noexcept
    : _capacity(other._capacity), _position(other._position), _data(other._data), _in_heap(other._in_heap) {
        other._in_heap = false;
    }
    constexpr Memory& operator = (Memory&& other) noexcept {
        if (_in_heap) {
            std::free(_data);
        }

        _in_heap = other._in_heap;
        _data = other._data;
        _capacity = other._capacity;
        _position = other._position;

        other.reset();
        return *this;
    };


    constexpr ~Memory () {
        if (_in_heap) {
            std::free(_data);
        }
        reset();
    }

private:
    constexpr void reset () {
        _in_heap = false;
        _data = nullptr;
        _capacity = 0;
        _position = 0;
    }
    
public:
    [[nodiscard]] constexpr uint8_t* const& data () const {
        return reinterpret_cast<uint8_t* const&>(_data);
    }

    [[nodiscard]] constexpr const U& current_position () const {
        return _position;
    }

    [[nodiscard]] constexpr const U& current_capacity () const {
        return _capacity;
    }

    constexpr void clear () {
        _position = 0;
    }

    constexpr void go_back (U amount) {
        _position -= amount;
    }

    constexpr void go_to (U position) {
        BSSERT(position <= _position, "[Memory::go_to] cant go forward.");
        _position = position;
    }

    constexpr void go_to_unsafe (U position) {
        _position = position;
    }

    template <typename T>
    [[nodiscard]] constexpr Index<T> position_idx () const {
        return Index<T>{_position};
    }

    template <typename T>
    constexpr T& get_next_unaligned () {
        return Base::get_unaligned(next_idx<T>());
    }

    template <typename T>
    constexpr T& get_next () {
        return Base::get(next_idx<T>());
    }

    template <typename T>
    requires (sizeof(T) == 1)
    constexpr Index<T> next_idx () {
        if (_position == _capacity) {
            grow();
        }
        return Index<T>{_position++};
    }

    template <typename T>
    requires (sizeof(T) > 1)
    constexpr Index<T> next_idx () {
        return next_multi_byte<T>(sizeof(T));
    }

    template <typename T>
    constexpr T* get_next_multi_byte (U size) {
        return Base::_get_unaligned(next_multi_byte<T>(size));
    }

    template <typename T>
    constexpr T* get_next_multi_byte_aligned (U size) {
        return Base::_get(next_multi_byte<T>(size));
    }

    template <typename T>
    constexpr Index<T> next_multi_byte (U size) {
        U next = _position;
        _position += size;
        if (_position < next) {
            console.error("[Memory::next_multi_byte] position overflow.");
            std::exit(1);
        }
        if (_position >= _capacity) {
            grow();
        }
        return Index<T>{next};
    }

    [[gnu::always_inline]] constexpr void grow () {
        U new_capacity;
        if (_position >= (max_position / grow_factor)) {
            console.warn("[Memory::grow] capped growth.");
            new_capacity = max_position;
        } else {
            new_capacity = _position * grow_factor;
        }
        if constexpr (alignment <= alignof(max_align_t)) {
            if (_in_heap) {
                _data = static_cast<gsl::owner<AllocatedT*>>(std::realloc(_data, new_capacity));
                BSSERT(_data != nullptr, "[Memory::grow] Reallocation failed.")
                goto done;
            }
        }
        {
            gsl::owner<AllocatedT*> allocated = alloc_handled(new_capacity);
            std::memcpy(allocated, _data, _capacity);
            _data = allocated;
            _in_heap = true;
        }
        done:
        _capacity = new_capacity;
    }

    friend struct ReadOnlyMemory<U>;
};

template <std::unsigned_integral U>
struct ReadOnlyMemory : MemoryBase<ReadOnlyMemory<U>, U> {
    using Base = MemoryBase<ReadOnlyMemory<U>, U>;

    template <typename T>
    using Index = Base::template Index<T>;

    template <typename T>
    using Span = Base::template Span<T>;

    template <typename T>
    using View = Base::template View<T>;

private:
    uint8_t* _data;

public:
    constexpr explicit ReadOnlyMemory (uint8_t* data) : _data(data) {}
    template <typename AllocatedT>
    constexpr explicit ReadOnlyMemory (const Memory<U, AllocatedT>& memory) : _data(reinterpret_cast<uint8_t*>(memory._data)) {}

    [[nodiscard]] constexpr uint8_t* const& data () const {
        return _data;
    }
};

using Buffer = Memory<uint32_t, max_align_t>;
using ReadOnlyBuffer = ReadOnlyMemory<uint32_t>;

template <size_t SIZE>
constexpr size_t MEMORY_INIT_STACK_MIN = SIZE < sizeof(max_align_t) ? sizeof(max_align_t) : SIZE;

// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define MEMORY_INIT_STACK_ARGS(ALLOC_TYPE, SIZE) static_cast<ALLOC_TYPE*>(__builtin_alloca_with_align(SIZE, alignof(ALLOC_TYPE) * 8)), SIZE
#define MEMORY_INIT_STACK(SIZE_TYPE, ALLOC_TYPE, SIZE) Memory<SIZE_TYPE, ALLOC_TYPE>::from_stack(MEMORY_INIT_STACK_ARGS(ALLOC_TYPE, SIZE))
#define BUFFER_INIT_STACK(SIZE) Buffer::from_stack(MEMORY_INIT_STACK_ARGS(max_align_t, SIZE))

template <typename ARRAY_TYPE, typename ELEMENT_TYPE, size_t LENGTH>
constexpr size_t MEMORY_INIT_ARRAY_SIZE = ((LENGTH * sizeof(ELEMENT_TYPE))+ sizeof(ARRAY_TYPE) - 1) / sizeof(ARRAY_TYPE);

template <typename ELEMENT_TYPE, size_t LENGTH>
constexpr size_t BUFFER_INIT_ARRAY_SIZE = MEMORY_INIT_ARRAY_SIZE<max_align_t, ELEMENT_TYPE, LENGTH>;
