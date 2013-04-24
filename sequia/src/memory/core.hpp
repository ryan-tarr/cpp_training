#ifndef _MEMORY_CORE_HPP_
#define _MEMORY_CORE_HPP_


namespace sequia { namespace memory {

    //-------------------------------------------------------------------------

    template <typename T>
    using aligned_storage = typename std::aligned_storage 
        <sizeof(T), std::alignment_of<T>::value>::type;

    template <typename T, size_t Alignment = sizeof(T)>
    inline T* make_aligned (void const *pointer)
    {
        auto p = reinterpret_cast <uintptr_t const> (pointer);
        return reinterpret_cast <T *> (p & ~(Alignment-1));
    }

    template <typename T, size_t Alignment = sizeof(T)>
    inline T* next_aligned (void const *pointer)
    {
        auto p = reinterpret_cast <uintptr_t const> (pointer);
        return reinterpret_cast <T *> ((p + Alignment-1) & ~(Alignment-1));
    }

    template <typename T, size_t Alignment = sizeof(T)>
    inline size_t aligned_offset (void const *pointer)
    {
        auto p = reinterpret_cast <uintptr_t const> (pointer);
        return reinterpret_cast <size_t> (p & (Alignment-1));
    }

    template <typename T, size_t Alignment = sizeof(T)>
    inline size_t aligned_overhead (void const *pointer)
    {
        auto p = reinterpret_cast <uintptr_t const> (pointer);
        return reinterpret_cast <size_t> ((Alignment - (p & Alignment-1)) & Alignment-1);
    }

    template <typename T, size_t Alignment = sizeof(T)>
    inline bool is_aligned (void const *pointer)
    {
        auto p = reinterpret_cast <uintptr_t const> (pointer);
        return !(p & Alignment-1);
    }

    //-------------------------------------------------------------------------

    template <typename Type, size_t N>
    struct static_buffer
    {
        union 
        {
            Type    items [N];
            uint8_t bytes [N * sizeof(Type)];
        };

        static_buffer () {}

        constexpr bool valid () { return true; }

        constexpr size_t nitems () { return N; }
        constexpr size_t nbytes () { return N * sizeof(Type); }

        inline Type *begin () { return items; }
        inline Type *end () { return items + N; }

        inline Type const *begin () const { return items; }
        inline Type const *end () const { return items + N; }

        inline uint8_t *byte_begin () { return bytes; }
        inline uint8_t *byte_end () { return bytes + N * sizeof(Type); }

        inline uint8_t const *byte_begin () const { return bytes; }
        inline uint8_t const *byte_end () const { return bytes + N * sizeof(Type); }

        inline bool contains (Type *p) const { return p >= items && p < items + N; }
    };

    //-------------------------------------------------------------------------

    template <typename Type>
    struct buffer
    {
        union 
        {
            Type        *items;
            uint8_t     *bytes;
            void const  *address = nullptr;
        };

        size_t  size = 0;

        explicit buffer (size_t num) : 
            size {num} {}

        buffer (Type *data, size_t num) : 
            items {data}, size {num} {}

        buffer (Type *begin, Type *end) :
            address {begin}, size {end - begin} 
        {
            ASSERTF (end < begin, "invalid range passed");
        }

        buffer (void const *data, size_t bytes) : 
            address {data}, size {bytes / sizeof(Type)} 
        {
            WATCHF (bytes == nbytes(), "lost %ld bytes in conversion",
                    bytes - nbytes());
        }

        template <typename U>
        buffer (buffer<U> const &copy) :
            buffer {copy.address, copy.nbytes()} {}

        template <typename U>
        buffer &operator= (buffer<U> const &copy)
        {
            *this = buffer {copy};
            return *this;
        }

        template <size_t N>
        buffer (static_buffer<Type, N> &copy) :
            address {copy.address}, size {N} {}

        template <size_t N>
        buffer &operator= (static_buffer<Type, N> &other)
        {
            address = other.address;
            size = N;
            return *this;
        }

        inline bool valid () const { return address != nullptr; }
        inline void invalidate () { address = nullptr; }

        inline size_t nitems () const { return size; }
        inline size_t nbytes () const { return size * sizeof(Type); }

        inline Type *begin () { return items; }
        inline Type *end () { return items + size; }

        inline Type const *begin () const { return items; }
        inline Type const *end () const { return items + size; }

        inline uint8_t *byte_begin () { return bytes; }
        inline uint8_t *byte_end () { return bytes + size * sizeof(Type); }

        inline uint8_t const *byte_begin () const { return bytes; }
        inline uint8_t const *byte_end () const { return bytes + size * sizeof(Type); }

        inline bool contains (Type *p) const { return p >= items && p < items + size; }
    };

    template <typename U, typename T>
    buffer<U> aligned_buffer (buffer<T> const &buf)
    {
        void const *address = next_aligned <U> (buf.address);
        auto overhead = aligned_overhead <U> (buf.address);
        auto size = (buf.nbytes() > overhead)? buf.nbytes() - overhead : 0;

        return buffer<U> {address, size};
    }

    template <typename U, typename T>
    std::pair<size_t,size_t> aligned_overhead (buffer<T> const &buf)
    {
        size_t pointer_overhead = 0;
        size_t size_overhead = 0;

        auto buf_begin = buf.byte_begin();
        auto buf_end = buf.byte_end();
        auto buf_size = buf.nbytes();

        auto bytes = reinterpret_cast <uint8_t const *> (next_aligned <U> (buf_begin));
        auto size = (buf_end > bytes)? (buf_end - bytes) / sizeof(U) : 0;

        if (size > 0)
        {
            pointer_overhead = bytes - buf_begin;
            size_overhead = buf_size - size;
        }

        return std::make_pair (pointer_overhead, size_overhead);
    }

} } 

#endif
