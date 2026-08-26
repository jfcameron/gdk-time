// © Joseph Cameron - All Rights Reserved

#ifndef GDK_TIMING_TIME_TYPES_H
#define GDK_TIMING_TIME_TYPES_H

namespace gdk::timing {
    /// \brief the unit every rate and duration in this library is expressed in
    using time_type = double;

    /// \brief a rate in frames per second
    struct frames_per_second final {
        time_type value{};

        constexpr frames_per_second() = default;

        explicit constexpr frames_per_second(const time_type aValue) : value(aValue) {}

        /// \brief the rate that means "do not wait at all"
        [[nodiscard]] static constexpr frames_per_second uncapped() { return frames_per_second{0}; }

        [[nodiscard]] constexpr bool operator==(const frames_per_second a) const {
            return value == a.value;
        }
    };

    /// \brief a rate in simulation steps per second, for a loop's fixed update
    struct steps_per_second final {
        time_type value{};

        constexpr steps_per_second() = default;

        explicit constexpr steps_per_second(const time_type aValue) : value(aValue) {}

        [[nodiscard]] constexpr bool operator==(const steps_per_second a) const {
            return value == a.value;
        }
    };
}

#endif
