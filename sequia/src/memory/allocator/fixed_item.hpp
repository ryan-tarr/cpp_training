#ifndef _FIXED_ITEM_ALLOCATOR_HPP_
#define _FIXED_ITEM_ALLOCATOR_HPP_

namespace sequia { namespace memory { namespace allocator {

        //=====================================================================
        // Allocates the one item from a fixed buffer each call

        template <typename Type>
        class fixed_item : public std::allocator <impl::block_32<Type>>
        {
            public:
                template <class U> struct rebind { using other = fixed_item<U>; };

            public:
                fixed_item () {}

                fixed_item (size_t const size) :
                    mem_ {size} {}

                fixed_item (fixed_item const &copy) :
                    mem_ {copy.max_size()} {}

                template <class U>
                fixed_item (fixed_item<U> const &copy) :
                    mem_ {copy.max_size()} {}

                ~fixed_item ()
                {
                    delete [] mem_.items;
                    mem_.invalidate ();
                }

            public:
                size_t max_size () const 
                { 
                    return item_count (mem_);
                }

                Type *allocate (size_t num, const void* = 0) 
                { 
                    if (!mem_)
                    {
                        mem_.items = new impl::block_32 <Type> [max_size ()];

                        auto index = 0;
                        for (auto &block : mem_)
                            block.index = ++index;

                        head_ = begin (mem_);
                    }

                    ASSERTF (num == 1, "can only allocate one object per call");
                    ASSERTF (contains (mem_, head_), "free list is corrupt");
                    ASSERTF (item_count (mem_) < 
                            (core::one << (core::min_num_bytes (max_size ()) * 8)),
                            "too many objects for size of free list index type");

                    auto block = head_;
                    head_ = begin (mem_) + head_->index;

                    return reinterpret_cast <Type *> (block);
                }

                void deallocate (Type *ptr, size_t num) 
                {
                    auto block = reinterpret_cast <impl::block_32<Type> *> (ptr);

                    ASSERTF (num == 1, "can only allocate one object per call");
                    ASSERTF (mem_, "not previously allocated");
                    ASSERTF (contains (mem_, block), "pointer is not from this heap");

                    block->index = head_ - begin (mem_);
                    head_ = block;
                }

            private:
                impl::block_32 <Type> *head_;
                memory::buffer <impl::block_32 <Type>> mem_;
        };

} } }

#endif
