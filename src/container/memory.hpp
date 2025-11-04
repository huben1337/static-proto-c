#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <limits>
#include <memory>
#include <gsl/pointers>

#include "../util/logger.hpp"
#include "../estd/type_traits.hpp"


template <std::unsigned_integral U>
struct MemoryBase {
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
        template <typename MemoryT>
        [[nodiscard]] constexpr T* begin (const MemoryT& mem) const {
            return mem.get(start_idx);
        }
        template <typename MemoryT>
        [[nodiscard]] constexpr T* end (const MemoryT& mem) const {
            return mem.get(end_idx);
        }

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

        template <typename MemoryT>
        [[nodiscard]] constexpr T* begin (const MemoryT& mem) const {
            return mem.get(start_idx);
        }
        template <typename MemoryT>
        [[nodiscard]] constexpr T* end (const MemoryT& mem) const {
            return mem.get(start_idx.add(length));
        }

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

    using index_t = U;
};

template <std::unsigned_integral U>
struct ReadOnlyMemory;

template <std::unsigned_integral U, typename AllocatedT>
struct Memory : MemoryBase<U> {
    protected:
    static constexpr U max_position = std::numeric_limits<U>::max();
    static constexpr uint8_t grow_factor = 2;
    static constexpr size_t alignment = std::max(alignof(max_align_t), alignof(AllocatedT));

    U capacity;
    U position = 0;
    gsl::owner<AllocatedT*> _data;
    bool in_heap;

    public:
    template <typename T>
    using Index = MemoryBase<U>::template Index<T>;

    template <typename T>
    using Span = MemoryBase<U>::template Span<T>;

    template <typename T>
    using View = MemoryBase<U>::template View<T>;

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
    constexpr explicit Memory (U capacity) : capacity(capacity), _data(alloc_handled(capacity)), in_heap(true) {}

    template <U N>
    constexpr explicit Memory (AllocatedT (&data)[N]) : capacity(sizeof(AllocatedT) * N), _data(data), in_heap(false) {
        static_assert(sizeof(AllocatedT) * N <= max_position, "capacity overflow");
    }

    private:
    constexpr Memory (AllocatedT* data, U capacity) : capacity(capacity), _data(data), in_heap(false) {}

    public:
    static constexpr Memory from_stack (AllocatedT* data, U capacity) {
        return Memory{data, capacity};
    }

    constexpr Memory (const Memory&) = delete;
    constexpr Memory& operator = (const Memory&) = delete;

    constexpr Memory (Memory&& other) noexcept
    : capacity(other.capacity), position(other.position), _data(other._data), in_heap(other.in_heap) {
        other.in_heap = false;
    }
    constexpr Memory& operator = (Memory&& other) noexcept {
        capacity = other.capacity;
        position = other.position;
        _data = other._data;
        in_heap = other.in_heap;
        other.in_heap = false;
        return *this;
    };


    constexpr ~Memory () {
        if (in_heap) {
            std::free(_data);
            in_heap = false;
        }
        _data = nullptr;
        capacity = 0;
        position = 0;
    }

    [[nodiscard]] constexpr const uint8_t* const& data () const {
        return reinterpret_cast<uint8_t* const&>(_data);
    }

    [[nodiscard]] constexpr const U& current_position () const {
        return position;
    }

    [[nodiscard]] constexpr const U& current_capacity () const {
        return capacity;
    }

    constexpr void clear () {
        position = 0;
    }

    constexpr void go_back (U amount) {
        position -= amount;
    }

    constexpr void go_to (U position) {
        if (position > this->position) {
            logger::error("[Memory::go_to] cant go forward.");
            exit(1);
        }
        this->position = position;
    }

    constexpr void go_to_unsafe (U position) {
        this->position = position;
    }

    template <typename T>
    [[nodiscard]] constexpr Index<T> position_idx () const {
        return Index<T>{position};
    }

    template <typename T>
    [[nodiscard]] constexpr T* get (Index<T> index) const {
        return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(_data) + index.value);
    }

    template <typename T>
    [[nodiscard]] constexpr T* get_aligned (Index<T> index) const {
        return std::assume_aligned<alignof(T), T>(reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(_data) + index.value));
    }

    template <typename T>
    constexpr T* get_next () {
        return this->get(next_idx<T>());
    }

    template <typename T>
    constexpr T* get_next_aligned () {
        return this->get_aligned(next_idx<T>());
    }

    template <typename T>
    constexpr Index<T> next_idx () {
        if  constexpr (sizeof(T) == 1) {
            return next_single_byte<T>();
        } else {
            return next_multi_byte<T>(sizeof(T));
        }
    }

    template <typename T>
    constexpr T* get_next_single_byte () {
        return get(next_single_byte<T>());
    }

    template <typename T>
    constexpr T* get_next_single_byte_aligned () {
        return get_aligned(next_single_byte<T>());
    }

    template <typename T>
    constexpr Index<T> next_single_byte () {
        if (position == capacity) {
            grow();
        }
        return Index<T>{position++};
    }

    template <typename T>
    constexpr T* get_next_multi_byte (U size) {
        return get(next_multi_byte<T>(size));
    }

    template <typename T>
    constexpr T* get_next_multi_byte_aligned (U size) {
        return get_aligned(next_multi_byte<T>(size));
    }

    template <typename T>
    constexpr Index<T> next_multi_byte (U size) {
        U next = position;
        position += size;
        if (position < next) {
            logger::error("[Memory::next_multi_byte] position overflow.");
            exit(1);
        }
        if (position >= capacity) {
            grow();
        }
        return Index<T>{next};
    }

    [[gnu::always_inline]] constexpr void grow () {
        U new_capacity;
        if (position >= (max_position / grow_factor)) {
            logger::warn("[Memory::grow] capped growth.");
            new_capacity = max_position;
        } else {
            new_capacity = position * grow_factor;
        }
        if constexpr (alignment <= alignof(max_align_t)) {
            if (in_heap) {
                _data = static_cast<gsl::owner<AllocatedT*>>(std::realloc(_data, new_capacity));
                BSSERT(_data != nullptr, "[Memory::grow] Reallocation failed.")
                goto done;
            }
        }
        {
            gsl::owner<AllocatedT*> allocated = alloc_handled(new_capacity);
            std::memcpy(allocated, _data, capacity);
            _data = allocated;
            in_heap = true;
        }
        done:
        capacity = new_capacity;
    }

    friend struct ReadOnlyMemory<U>;
};

template <std::unsigned_integral U>
struct ReadOnlyMemory : MemoryBase<U> {
    private:
    uint8_t* _data;

    public:
    template <typename T>
    using Index = MemoryBase<U>::template Index<T>;

    template <typename T>
    using Span = MemoryBase<U>::template Span<T>;

    template <typename T>
    using View = MemoryBase<U>::template View<T>;


    constexpr explicit ReadOnlyMemory (uint8_t* data) : _data(data) {}
    template <typename AllocatedT>
    constexpr explicit ReadOnlyMemory (const Memory<U, AllocatedT>& memory) : _data(reinterpret_cast<uint8_t*>(memory._data)) {}

    [[nodiscard]] constexpr uint8_t* data () const {
        return _data;
    }

    template <typename T>
    [[nodiscard]] constexpr T* get (Index<T> index) const {
        return reinterpret_cast<T*>(_data + index.value);
    }

    template <typename T>
    [[nodiscard]] constexpr T* get_aligned (Index<T> index) const {
        return std::assume_aligned<alignof(T), T>(reinterpret_cast<T*>(_data + index.value));
    }
};

using BufferBase = MemoryBase<uint32_t>;
using Buffer = Memory<uint32_t, max_align_t>;
using ReadOnlyBuffer = ReadOnlyMemory<uint32_t>;

template <size_t SIZE>
constexpr size_t MEMORY_INIT_STACK_MIN = SIZE < sizeof(max_align_t) ? sizeof(max_align_t) : SIZE;

// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define MEMORY_INIT_STACK_ARGS(ALLOC_TYPE, SIZE) static_cast<ALLOC_TYPE*>(__builtin_alloca_with_align(SIZE, alignof(ALLOC_TYPE) * 8)), SIZE
#define MEMORY_INIT_STACK(SIZE_TYPE, ALLOC_TYPE, SIZE) Memory<SIZE_TYPE, ALLOC_TYPE>::from_stack(MEMORY_INIT_STACK_ARGS(ALLOC_TYPE, SIZE))
#define BUFFER_INIT_STACK(SIZE) Buffer::from_stack(MEMORY_INIT_STACK_ARGS(max_align_t, SIZE))

template <typename ARRAY_TYPE, typename ELEMENT_TYPE, size_t LENGTH>
constexpr size_t MEMORY_INIT_ARRAY_SIZE = (LENGTH * sizeof(ELEMENT_TYPE) + sizeof(ARRAY_TYPE) - 1) / sizeof(ARRAY_TYPE);

template <typename ELEMENT_TYPE, size_t LENGTH>
constexpr size_t BUFFER_INIT_ARRAY_SIZE = MEMORY_INIT_ARRAY_SIZE<max_align_t, ELEMENT_TYPE, LENGTH>;
