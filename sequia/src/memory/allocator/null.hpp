#ifndef _NULL_ALLOCATOR_HPP_
#define _NULL_ALLOCATOR_HPP_

namespace sequia
{
    namespace memory
    {
        namespace allocator
        {
            //=========================================================================
            // Implements null-operation semantics
            // Fulfills stateful allocator concept
            // Fulfills terminal allocator concept

            template <typename State>
            class null
            {
                public:
                    typedef State                           state_type;
                    typedef typename state_type::value_type value_type;

                    typedef std::false_type propagate_on_container_copy_assignment;
                    typedef std::false_type propagate_on_container_move_assignment;
                    typedef std::false_type propagate_on_container_swap;

                public:
                    // default constructor
                    null () = default;

                    // stateful copy constructor
                    template <typename Allocator>
                    null (Allocator const &copy) :
                        null {copy.state()} {}

                    // stateful constructor
                    explicit null (State const &state) :
                        state_ {state} {}

                    // destructor
                    ~null () = default;

                public:
                    // return max allocation of zero
                    size_t max_size () const { return 0; }

                    // allocation is a null operation
                    T *allocate (size_t num, const void* = 0) {}

                    // deallocation is a null operation
                    void deallocate (T *ptr, size_t num) {}

                public:
                    state_type const &state() const { return state_; }

                private:
                    state_type  state_;
            };
        }
    }
}

#endif
