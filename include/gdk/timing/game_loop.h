// © Joseph Cameron - All Rights Reserved

#ifndef GDK_TIMING_GAME_LOOP_H
#define GDK_TIMING_GAME_LOOP_H

#include <gdk/timing/types.h>

#include <atomic>
#include <cstdint>
#include <functional>

namespace gdk::timing {
    /// \brief calls a behaviour once per frame, at a bounded rate, until it asks to stop
    ///
    /// A loop has one behaviour always and a second optionally:
    ///
    /// - the **frame behaviour** runs once per frame, with whatever delta the frame actually took.
    ///   Presentation lives here: camera, animation, anything whose job is to look smooth.
    /// - the **fixed behaviour**, if the four argument constructor was used, runs zero or more times
    ///   per frame at a rate that never varies. Simulation lives here: integration, collision,
    ///   anything whose job is to be the same regardless of framerate.
    ///
    class game_loop final {
    public:
        /// \brief what the loop knows about the frame that is about to be presented
        struct frame final {
            /// \brief seconds of game time since the loop started
            time_type elapsed;

            /// \brief seconds of game time the previous frame covered, after clamping and scaling
            time_type delta;

            /// \brief seconds of wall clock the previous frame took, after clamping
            time_type unscaled_delta;

            /// \brief how far past the last fixed step this frame sits, from 0 up to but not 1
            time_type interpolation;

            /// \brief how many frames ran before this one, counting from zero
            std::uint64_t index;
        };

        /// \brief what the loop knows about the fixed step that is about to run
        struct step final {
            /// \brief seconds of simulated time before this step
            time_type elapsed;

            /// \brief seconds this step advances the simulation by; should be the same value every step
            time_type delta;

            /// \brief how many fixed steps ran before this one, counting from zero
            std::uint64_t index;
        };

        using loop_behavior_type = std::function<
            bool //shouldClose
            (
                frame
            )
        >;

        using fixed_behavior_type = std::function<
            bool //shouldClose
            (
                step
            )
        >;

        static constexpr bool SHOULD_CLOSE = true;
        static constexpr bool SHOULD_CONTINUE = !SHOULD_CLOSE;

        /// \brief the time scale at which the simulation does not advance at all
        static constexpr time_type PAUSED = 0;

        /// \brief the time scale at which game time and wall clock agree
        static constexpr time_type NORMAL_TIME_SCALE = 1;

        /// \brief the delta ceiling applied unless the caller asks for another in seconds
        static constexpr time_type DEFAULT_MAXIMUM_DELTA_TIME = 0.25;

        /// \brief a loop with no simulation
        ///
        /// \param aMaximumFramesPerSecond frames per second not to exceed, or
        /// frames_per_second::uncapped()
        /// \param aLoopBehavior run once per frame; returns \ref SHOULD_CLOSE to end the loop
        /// \param aMaximumDeltaTime ceiling on the delta handed to the behaviour, in seconds
        /// \exception exception an argument is out of range, or the behaviour is empty
        game_loop(const frames_per_second aMaximumFramesPerSecond, loop_behavior_type aLoopBehavior,
            const time_type aMaximumDeltaTime = DEFAULT_MAXIMUM_DELTA_TIME);

        /// \brief a loop that also simulates, at a rate independent of the frame rate
        ///
        /// \param aMaximumFramesPerSecond frames per second not to exceed, or
        /// frames_per_second::uncapped()
        /// \param aLoopBehavior run once per frame; returns \ref SHOULD_CLOSE to end the loop
        /// \param aStepsPerSecond steps per second of simulated time. 60 is a common choice; higher
        /// costs more and resolves faster motion, lower costs less and tunnels sooner
        /// \param aFixedBehavior run once per step; returns \ref SHOULD_CLOSE to end the loop
        /// \param aMaximumDeltaTime ceiling on the delta handed to the behaviours, in seconds
        /// \exception exception an argument is out of range, or a behaviour is empty
        game_loop(const frames_per_second aMaximumFramesPerSecond, loop_behavior_type aLoopBehavior,
            const steps_per_second aStepsPerSecond, fixed_behavior_type aFixedBehavior,
            const time_type aMaximumDeltaTime = DEFAULT_MAXIMUM_DELTA_TIME);

        /// \brief the step rate this loop simulates at, or a rate of zero if it does not simulate
        [[nodiscard]] steps_per_second step_rate() const;

        /// \brief change the frame rate cap, including while the loop is running
        ///
        /// \exception exception the rate is negative or not a number
        void set_maximum_frames_per_second(const frames_per_second aMaximumFramesPerSecond);

        /// \brief the current frame rate cap, or frames_per_second::uncapped()
        [[nodiscard]] frames_per_second maximum_frames_per_second() const;

        /// \brief scale game time against wall clock, including while the loop is running
        void set_time_scale(const time_type aTimeScale);

        /// \brief the current time scale; \ref PAUSED means the simulation is not advancing
        [[nodiscard]] time_type time_scale() const;

        /// \brief ask the loop to stop, from anywhere
        void request_stop() noexcept;

        /// \brief whether \ref request_stop has been called since the last \ref reset
        [[nodiscard]] bool stop_requested() const noexcept;

        /// \brief advance exactly one frame and return
        ///
        /// \exception exception a tick is already in progress
        bool tick();

        /// \brief tick until a behaviour asks to stop
        ///
        /// \exception exception a tick is already in progress
        void run();

        /// \brief return to a fresh clock, frame count and step count
        ///
        /// \exception exception a tick is in progress
        void reset();

        game_loop(const game_loop &) = delete;
        game_loop &operator=(const game_loop &) = delete;
        game_loop(game_loop &&) = delete;
        game_loop &operator=(game_loop &&) = delete;

        ~game_loop() = default;

    private:
        [[nodiscard]] static time_type _checked_frames_per_second(const frames_per_second a);

        [[nodiscard]] static time_type _checked_time_scale(const time_type a);

        std::atomic<time_type> m_MaximumFramesPerSecond;

        std::atomic<time_type> m_TimeScale{NORMAL_TIME_SCALE};

        time_type m_FixedDelta{0};

        frame m_Frame{0, 0, 0, 0, 0};

        time_type m_Accumulator{0};

        std::uint64_t m_StepIndex{0};

        bool m_ShouldClose{false};

        std::atomic<bool> m_StopRequested{false};

        loop_behavior_type m_LoopBehavior;

        time_type m_MaximumDeltaTime;

        fixed_behavior_type m_FixedBehavior;

        steps_per_second m_StepsPerSecond{0};

        std::atomic<bool> m_Running{false};
    };
}

#endif
