// © Joseph Cameron - All Rights Reserved

#include <gdk/timing/game_loop.h>

#include <gdk/timing/exception.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

using namespace gdk::timing;

namespace {
    class running_scope final {
    public:
        explicit running_scope(std::atomic<bool> &aRunning) : m_Running(aRunning) {}
        ~running_scope() { m_Running.store(false, std::memory_order_release); }
        running_scope(const running_scope &) = delete;
        running_scope &operator=(const running_scope &) = delete;

    private:
        std::atomic<bool> &m_Running;
    };
}

time_type game_loop::_checked_frames_per_second(const frames_per_second a) {
    if (!(a.value >= 0) || !std::isfinite(a.value))
        throw exception("game_loop: frames per second must be a finite, non-negative "
            "number; frames_per_second::uncapped() removes the cap. Was "
            + std::to_string(a.value));

    return a.value;
}

time_type game_loop::_checked_time_scale(const time_type a) {
    if (!(a >= 0) || !std::isfinite(a))
        throw exception("game_loop: time scale must be a finite, non-negative number; "
            "game_loop::PAUSED stops the simulation. Was " + std::to_string(a));

    return a;
}

game_loop::game_loop(const frames_per_second aMaximumFramesPerSecond,
    loop_behavior_type aLoopBehavior, const time_type aMaximumDeltaTime)
: m_MaximumFramesPerSecond(_checked_frames_per_second(aMaximumFramesPerSecond))
, m_LoopBehavior(std::move(aLoopBehavior))
, m_MaximumDeltaTime(aMaximumDeltaTime) {
    if (!(aMaximumDeltaTime > 0) || !std::isfinite(aMaximumDeltaTime))
        throw exception("game_loop: maximum delta time must be finite and above zero, "
            "was " + std::to_string(aMaximumDeltaTime));

    if (!m_LoopBehavior)
        throw exception("game_loop: loop behavior must not be empty");
}

game_loop::game_loop(const frames_per_second aMaximumFramesPerSecond,
    loop_behavior_type aLoopBehavior, const steps_per_second aStepsPerSecond,
    fixed_behavior_type aFixedBehavior, const time_type aMaximumDeltaTime)
: game_loop(aMaximumFramesPerSecond, std::move(aLoopBehavior), aMaximumDeltaTime) {
    if (!(aStepsPerSecond.value > 0) || !std::isfinite(aStepsPerSecond.value))
        throw exception("game_loop: steps per second must be finite and above zero, "
            "was " + std::to_string(aStepsPerSecond.value));

    if (!aFixedBehavior)
        throw exception("game_loop: fixed behavior must not be empty");

    m_StepsPerSecond = aStepsPerSecond;
    m_FixedBehavior = std::move(aFixedBehavior);
    m_FixedDelta = 1.0 / aStepsPerSecond.value;
}

steps_per_second game_loop::step_rate() const {
    return m_FixedBehavior ? m_StepsPerSecond : steps_per_second{0};
}

void game_loop::set_maximum_frames_per_second(const frames_per_second aMaximumFramesPerSecond) {
    m_MaximumFramesPerSecond.store(_checked_frames_per_second(aMaximumFramesPerSecond),
        std::memory_order_relaxed);
}

frames_per_second game_loop::maximum_frames_per_second() const {
    return frames_per_second{m_MaximumFramesPerSecond.load(std::memory_order_relaxed)};
}

void game_loop::set_time_scale(const time_type aTimeScale) {
    m_TimeScale.store(_checked_time_scale(aTimeScale), std::memory_order_relaxed);
}

time_type game_loop::time_scale() const {
    return m_TimeScale.load(std::memory_order_relaxed);
}

void game_loop::request_stop() noexcept {
    m_StopRequested.store(true, std::memory_order_release);
}

bool game_loop::stop_requested() const noexcept {
    return m_StopRequested.load(std::memory_order_acquire);
}

void game_loop::reset() {
    if (m_Running.exchange(true, std::memory_order_acq_rel))
        throw exception("game_loop: cannot reset while a tick is in progress");

    const running_scope runningScope(m_Running);

    m_Frame = frame{0, 0, 0, 0, 0};
    m_Accumulator = 0;
    m_StepIndex = 0;
    m_ShouldClose = false;
    m_StopRequested.store(false, std::memory_order_release);
}

bool game_loop::tick() {
    using namespace std::chrono;

    if (m_Running.exchange(true, std::memory_order_acq_rel))
        throw exception("game_loop: already running");

    const running_scope runningScope(m_Running);

    if (stop_requested()) m_ShouldClose = true;

    if (m_ShouldClose) return SHOULD_CLOSE;

    constexpr time_type SPIN_MARGIN(0.0002);

    const steady_clock::time_point currentFrameStartTimePoint(steady_clock::now());

    if (m_FixedBehavior) {
        m_Accumulator += m_Frame.delta;

        while (m_Accumulator >= m_FixedDelta && !m_ShouldClose) {
            m_ShouldClose = m_FixedBehavior(step{m_StepIndex * m_FixedDelta, m_FixedDelta,
                m_StepIndex});

            ++m_StepIndex;
            m_Accumulator -= m_FixedDelta;
        }

        m_Frame.interpolation = m_Accumulator / m_FixedDelta;
    }

    if (!m_ShouldClose) m_ShouldClose = m_LoopBehavior(m_Frame);

    if (m_ShouldClose || stop_requested()) {
        m_ShouldClose = true;

        return SHOULD_CLOSE;
    }

    const time_type framesPerSecond(maximum_frames_per_second().value);

    const time_type frameTime(framesPerSecond == frames_per_second::uncapped().value
        ? 0
        : 1.0 / framesPerSecond);

    time_type timeSpentOnCurrentFrame(0);

    for (;;) {
        timeSpentOnCurrentFrame =
            duration<time_type>(steady_clock::now() - currentFrameStartTimePoint).count();

        const time_type remaining(frameTime - timeSpentOnCurrentFrame);

        if (remaining <= 0) break;

        if (remaining > SPIN_MARGIN)
            std::this_thread::sleep_for(duration<time_type>(remaining - SPIN_MARGIN));
    }

    m_Frame.unscaled_delta = std::min(timeSpentOnCurrentFrame, m_MaximumDeltaTime);
    m_Frame.delta = m_Frame.unscaled_delta * time_scale();
    m_Frame.elapsed += m_Frame.delta;
    ++m_Frame.index;

    return SHOULD_CONTINUE;
}

void game_loop::run() {
    while (tick() != SHOULD_CLOSE);
}
