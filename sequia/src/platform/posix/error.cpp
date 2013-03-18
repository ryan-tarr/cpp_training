#include <core/standard.hpp>

#include <cerrno>

#include <platform/posix/error.hpp>

namespace sequia { namespace platform { namespace posix {

    void load_last_error_code (std::error_code &error)
    {
        return error.assign (errno, std::system_category ()); 
    }

} } }
