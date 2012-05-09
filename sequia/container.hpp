#ifndef _CONTAINER_HPP_
#define _CONTAINER_HPP_

#include <vector>
#include <map>

namespace sequia
{
    namespace container
    {
        template <typename T, size_t N>
        using fixedvector = std::vector<T, memory::fixed_identity_allocator<T, N>>;

        template <typename K, typename V, size_t N, typename Compare = std::less<K>>
        using fixedmap = std::map<K, V, Compare, memory::rebind_allocator<memory::fixed_unity_allocator<std::pair<const K, V>, N>>>;
    }
}

#endif