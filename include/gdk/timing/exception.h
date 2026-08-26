// © Joseph Cameron - All Rights Reserved

#ifndef GDK_TIMING_EXCEPTION_H
#define GDK_TIMING_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace gdk::timing {
    /// \brief root exception type for this library
    class exception final : public std::runtime_error {
    public:
        explicit exception(const std::string &aWhat)
        : std::runtime_error(aWhat)
        {}
    };
}

#endif
