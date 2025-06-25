#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <limits>
#include <memory>
#include "base.cpp"
#include "helper_types.cpp"
#include "fatal_error.cpp"
#include "logger.cpp"



template <std::unsigned_integral U>
struct MemoryBase {
    template <typename T>
    struct Index {
        INLINE constexpr Index () = default;
        INLINE constexpr Index (U value) : value(value) {}
        
        U value;

        INLINE constexpr Index add (U offset) const {
            return {value + offset};
        }

        INLINE constexpr Index sub (U offset) const {
            return {value - offset};
        }

        INLINE constexpr operator Index<const T>() const {
            return {value};
        }
    };

    template <typename T>
    struct Span {
        INLINE constexpr Span () = default;
        INLINE constexpr Span (Index<T> start_idx, Index<T> end_idx) : start_idx(start_idx), end_idx(end_idx) {}
        INLINE constexpr Span (Index<T> start_idx, U length) : start_idx(start_idx), end_idx(start_idx.add(length)) {}

        Index<T> start_idx;
        Index<T> end_idx;
        template <typename MemoryT>
        INLINE T* begin (const MemoryT& mem) const {
            return mem.get(start_idx);
        }
        template <typename MemoryT>
        INLINE T* end (const MemoryT& mem) const {
            return mem.get(end_idx);
        }

        INLINE constexpr U size () const {
            return end_idx.value - start_idx.value;
        }

        INLINE constexpr bool empty () const {
            return start_idx.value == end_idx.value;
        }
    };

    template <typename T>
    struct View {
        using length_t = fitting_uint<std::numeric_limits<U>::max() / sizeof(T)>;

        INLINE constexpr View () = default;
        INLINE constexpr View (Index<T> start_idx, length_t length) : start_idx(start_idx), length(length) {}
        INLINE constexpr View (Index<T> start_idx, Index<T> end_idx) : start_idx(start_idx), length((end_idx.value - start_idx.value) / sizeof(T)) {}
        Index<T> start_idx;
        length_t length;

        template <typename MemoryT>
        INLINE T* begin (const MemoryT& mem) const {
            return mem.get(start_idx);
        }
        template <typename MemoryT>
        INLINE T* end (const MemoryT& mem) const {
            return mem.get(start_idx.add(length));
        }

        INLINE constexpr U size () const {
            return length * sizeof(T);
        }

        INLINE constexpr bool empty () const {
            return length == 0;
        }
    };

    using index_t = U;

    template <typename T>
    using _Index = MemoryBase<U>::template Index<T>;

    template <typename T>
    using _Span = MemoryBase<U>::template Span<T>;

    template <typename T>
    using _View = MemoryBase<U>::template View<T>;
};

template <std::unsigned_integral U>
struct ReadOnlyMemory;

template <std::unsigned_integral U>
struct Memory : MemoryBase<U> {
    protected:
    static constexpr U max_position = std::numeric_limits<U>::max();
    static constexpr uint8_t grow_factor = 2;
    U capacity;
    U position = 0;
    uint8_t* _data;
    bool in_heap;

    INLINE constexpr Memory (const Memory& other) = default;
    INLINE constexpr Memory& operator = (const Memory& other) = default;

    friend class estd::const_copy_detail; // Give const_copy_detail access to private move constructor

    public:
    template <typename T>
    using Index = MemoryBase<U>::template Index<T>;

    template <typename T>
    using Span = MemoryBase<U>::template Span<T>;

    template <typename T>
    using View = MemoryBase<U>::template View<T>;

    INLINE constexpr Memory (Memory&& other) = default;
    INLINE constexpr Memory& operator = (Memory&& other) = default;

    INLINE Memory (U capacity) : capacity(capacity) {
        max_align_t* allocated = static_cast<max_align_t*>(std::malloc(capacity));
        if (!allocated) {
            INTERNAL_ERROR("memory allocation failed\n");
        };
        if (reinterpret_cast<uintptr_t>(allocated) % sizeof(max_align_t) != 0) {
            INTERNAL_ERROR("allocated memory is not aligned\n");
        }
        _data = reinterpret_cast<uint8_t*>(allocated);
        in_heap = true;
    }

    template <U N>
    INLINE constexpr Memory (max_align_t (&data)[N]) : capacity(sizeof(max_align_t) * N), _data(reinterpret_cast<uint8_t*>(data)), in_heap(false) {
        static_assert(sizeof(max_align_t) * N <= max_position, "capacity overflow");
    }
    
    INLINE constexpr Memory (max_align_t* data, U capacity) : capacity(capacity), _data(reinterpret_cast<uint8_t*>(data)), in_heap(false) {}

    INLINE constexpr void clear () {
        position = 0;
    }

    INLINE void dispose () {
        if (in_heap) {
            std::free(_data);
            // Prevent double free
            in_heap = false;
        }
        _data = nullptr;
        capacity = 0;
        position = 0;
    }


    INLINE constexpr const uint8_t* const& data () const {
        return _data;
    }

    INLINE constexpr const U& current_position () const {
        return position;
    }

    INLINE constexpr const U& current_capacity () const {
        return capacity;
    }

    INLINE constexpr void go_back (U amount) {
        position -= amount;
    }

    INLINE constexpr void go_to (U position) {
        if (position > this->position) {
            INTERNAL_ERROR("[Memory::go_to] cant go forward\n");
        }
        this->position = position;
    }

    INLINE constexpr void go_to_unsafe (U position) {
        this->position = position;
    }

    template <typename T>
    INLINE constexpr Index<T> position_idx () const {
        return {position};
    }

    template <typename T>
    INLINE constexpr T* get (Index<T> index) const {
        return reinterpret_cast<T*>(_data + index.value);
    }

    template <typename T>
    INLINE constexpr T* get_aligned (Index<T> index) const {
        return std::assume_aligned<alignof(T), T>(reinterpret_cast<T*>(_data + index.value));
    }

    template <typename T>
    INLINE T* get_next () {
        return this->get(next_idx<T>());
    }

    template <typename T>
    INLINE T* get_next_aligned () {
        return this->get_aligned(next_idx<T>());
    }

    template <typename T>
    INLINE Index<T> next_idx () {
        if constexpr (sizeof_v<T> == 0) {
            return Index<T>{position};
        } else if  constexpr (sizeof_v<T> == 1) {
            return next_single_byte<T>();
        } else {
            return next_multi_byte<T>(sizeof_v<T>);
        }
    }

    template <typename T>
    INLINE T* get_next_single_byte () {
        return get(next_single_byte<T>());
    }

    template <typename T>
    INLINE T* get_next_single_byte_aligned () {
        return get_aligned(next_single_byte<T>());
    }
    
    template <typename T>
    INLINE Index<T> next_single_byte () {
        if (position == capacity) {
            grow();
        }
        return Index<T>{position++};
    }

    template <typename T, std::unsigned_integral SizeT>
    INLINE T* get_next_multi_byte (SizeT size) {
        return get(next_multi_byte<T>(size));
    }

    template <typename T, std::unsigned_integral SizeT>
    INLINE T* get_next_multi_byte_aligned (SizeT size) {
        return get_aligned(next_multi_byte<T>(size));
    }

    template <typename T, std::unsigned_integral SizeT>
    INLINE Index<T> next_multi_byte (SizeT size) {
        U next = position;
        position += size;
        if (position < next) {
            logger::error("[Memory::next_multi_byte] position overflow\n");
            exit(1);
        }
        if (position >= capacity) {
            grow();
        }
        return Index<T>{next};
    }

    private:
    INLINE void grow () {
        U new_capacity;
        if (position >= (max_position / grow_factor)) {
            logger::warn("[Memory::grow] capped growth\n");
            new_capacity = max_position;
        } else {
            new_capacity = position * grow_factor;
        }
        if (in_heap) {
            _data = reinterpret_cast<uint8_t*>(std::realloc(_data, new_capacity));
        } else {
            uint8_t* mallocated = reinterpret_cast<uint8_t*>(std::malloc(new_capacity));
            std::memcpy(mallocated, _data, capacity);
            _data = mallocated;
            in_heap = true;
        }
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


    INLINE constexpr ReadOnlyMemory (uint8_t* data) : _data(data) {}

    INLINE constexpr ReadOnlyMemory (const Memory<U>& memory) : _data(memory._data) {}

    INLINE constexpr uint8_t* data () const {
        return _data;
    }

    template <typename T>
    INLINE constexpr T* get (Index<T> index) const {
        return reinterpret_cast<T*>(_data + index.value);
    }

    template <typename T>
    INLINE constexpr T* get_aligned (Index<T> index) const {
        return std::assume_aligned<alignof(T), T>(reinterpret_cast<T*>(_data + index.value));
    }
};

typedef MemoryBase<uint32_t> BufferBase;
typedef Memory<uint32_t> Buffer;
typedef ReadOnlyMemory<uint32_t> ReadOnlyBuffer;

template <size_t SIZE>
constexpr size_t MEMORY_INIT_STACK_MIN = SIZE < sizeof(max_align_t) ? sizeof(max_align_t) : SIZE;

#define MEMORY_INIT_STACK(SIZE) static_cast<max_align_t*>(__builtin_alloca_with_align(SIZE, alignof(max_align_t) * 8)), SIZE
#define BUFFER_INIT_STACK(SIZE) MEMORY_INIT_STACK(SIZE)

template <typename ELEMENT_TYPE, size_t LENGTH>
constexpr size_t MEMORY_INIT_ARRAY_SIZE = (LENGTH * sizeof(ELEMENT_TYPE) + sizeof(max_align_t) - 1) / sizeof(max_align_t);

template <typename ELEMENT_TYPE, size_t LENGTH>
constexpr size_t BUFFER_INIT_ARRAY_SIZE = MEMORY_INIT_ARRAY_SIZE<ELEMENT_TYPE, LENGTH>;