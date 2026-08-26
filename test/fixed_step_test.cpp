// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>

#include <gdk/timing/game_loop.h>
#include <gdk/timing/exception.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace gdk::timing;

namespace {
    struct body final {
        double y{100};
        double v{0};

        void integrate(const double aDeltaTime) {
            v -= 9.81 * aDeltaTime;
            y += v * aDeltaTime;
        }
    };
}

TEST_CASE("a fixed step simulates identically at any frame rate", "[fixed][determinism]") {
    constexpr int STEPS = 60;

    std::vector<double> fixedResults;
    std::vector<double> variableResults;

    for (const auto cap : {frames_per_second{300}, frames_per_second{1000},
        frames_per_second::uncapped()}) {
        body fixedBody;
        body variableBody;

        game_loop(cap,
            [&](const game_loop::frame aFrame) {
                variableBody.integrate(aFrame.delta);
                return game_loop::SHOULD_CONTINUE;
            },
            steps_per_second{600},
            [&](const game_loop::step aStep) {
                fixedBody.integrate(aStep.delta);
                return aStep.index + 1 >= STEPS ? game_loop::SHOULD_CLOSE
                                                : game_loop::SHOULD_CONTINUE;
            }).run();

        fixedResults.push_back(fixedBody.y);
        variableResults.push_back(variableBody.y);
    }

    SECTION("the fixed result is the same number every time") {
        REQUIRE(fixedResults.size() == 3);
        REQUIRE(fixedResults[0] == fixedResults[1]);
        REQUIRE(fixedResults[1] == fixedResults[2]);
    }

    SECTION("and integrating on the frame delta instead does not give that") {
        REQUIRE(variableResults[0] != variableResults[2]);
    }
}

TEST_CASE("steps and frames are counted independently", "[fixed]") {
    SECTION("no steps run until a frame has taken some time") {
        std::uint64_t stepsBeforeFirstFrame = 0;
        std::uint64_t steps = 0;
        int frames = 0;

        game_loop(frames_per_second::uncapped(),
            [&](const game_loop::frame aFrame) {
                if (aFrame.index == 0) stepsBeforeFirstFrame = steps;
                return ++frames >= 20 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
            },
            steps_per_second{2000},
            [&](const game_loop::step) { ++steps; return game_loop::SHOULD_CONTINUE; }).run();

        REQUIRE(stepsBeforeFirstFrame == 0);
    }

    SECTION("step elapsed is computed from the index, so it cannot drift") {
        std::vector<game_loop::step> seen;

        game_loop(frames_per_second::uncapped(),
            [](const game_loop::frame) { return game_loop::SHOULD_CONTINUE; },
            steps_per_second{1000},
            [&](const game_loop::step aStep) {
                seen.push_back(aStep);
                return aStep.index + 1 >= 50 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
            }).run();

        REQUIRE(seen.size() == 50);

        for (std::size_t i = 0; i < seen.size(); ++i) {
            REQUIRE(seen[i].index == i);
            REQUIRE(seen[i].delta == 0.001);
            REQUIRE(seen[i].elapsed == static_cast<double>(i) * 0.001);
        }
    }

    SECTION("a step can end the loop, and no frame runs after it does") {
        std::uint64_t steps = 0;
        int framesAfterClose = 0;
        bool closed = false;

        game_loop(frames_per_second{1000},
            [&](const game_loop::frame) {
                if (closed) ++framesAfterClose;
                return game_loop::SHOULD_CONTINUE;
            },
            steps_per_second{500},
            [&](const game_loop::step) {
                ++steps;
                closed = true;
                return game_loop::SHOULD_CLOSE;
            }).run();

        REQUIRE(steps == 1);
        REQUIRE(framesAfterClose == 0);
    }
}

TEST_CASE("interpolation says where a frame sits between two steps", "[fixed][interpolation]") {
    double lowest = 2;
    double highest = -1;
    int frames = 0;

    game_loop(frames_per_second{700},
        [&](const game_loop::frame aFrame) {
            if (aFrame.index > 0) {
                lowest = std::min(lowest, aFrame.interpolation);
                highest = std::max(highest, aFrame.interpolation);
            }

            return ++frames >= 60 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        },
        steps_per_second{300},
        [](const game_loop::step) { return game_loop::SHOULD_CONTINUE; }).run();

    REQUIRE(lowest >= 0);
    REQUIRE(highest < 1);
}

TEST_CASE("the delta ceiling bounds how many steps one frame can drive", "[fixed][spiral]") {
    constexpr double CEILING = 0.05;
    constexpr double RATE = 200;

    std::uint64_t stepsAtFrameStart = 0;
    std::uint64_t steps = 0;
    std::uint64_t mostInOneFrame = 0;
    int frames = 0;

    game_loop loop(frames_per_second::uncapped(),
        [&](const game_loop::frame) {
            mostInOneFrame = std::max(mostInOneFrame, steps - stepsAtFrameStart);
            stepsAtFrameStart = steps;

            if (frames == 1) std::this_thread::sleep_for(std::chrono::milliseconds(250));

            return ++frames >= 5 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        },
        steps_per_second{RATE},
        [&](const game_loop::step) { ++steps; return game_loop::SHOULD_CONTINUE; },
        CEILING);

    loop.run();

    REQUIRE(mostInOneFrame <= static_cast<std::uint64_t>(CEILING * RATE) + 1);
    REQUIRE(mostInOneFrame > 1);
}

TEST_CASE("a fixed loop refuses what it cannot simulate", "[fixed][validation]") {
    const auto onFrame = [](const game_loop::frame) { return game_loop::SHOULD_CLOSE; };
    const auto onStep = [](const game_loop::step) { return game_loop::SHOULD_CLOSE; };

    SECTION("a step rate of zero, which would divide by it") {
        REQUIRE_THROWS_AS(game_loop(frames_per_second{60}, onFrame, steps_per_second{0}, onStep),
            exception);
    }

    SECTION("a negative or non-numeric step rate") {
        REQUIRE_THROWS_AS(game_loop(frames_per_second{60}, onFrame, steps_per_second{-60}, onStep),
            exception);
        REQUIRE_THROWS_AS(
            game_loop(frames_per_second{60}, onFrame, steps_per_second{std::nan("")}, onStep),
            exception);
    }

    SECTION("an empty fixed behaviour") {
        REQUIRE_THROWS_AS(game_loop(frames_per_second{60}, onFrame, steps_per_second{60},
            game_loop::fixed_behavior_type{}), exception);
    }

    SECTION("a loop with no simulation reports a step rate of zero") {
        int frames = 0;
        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            return ++frames >= 1 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        REQUIRE(loop.step_rate() == steps_per_second{0});
    }

    SECTION("and one with a simulation reports the rate it was given") {
        int frames = 0;
        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            return ++frames >= 1 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        }, steps_per_second{120}, [](const game_loop::step) { return game_loop::SHOULD_CONTINUE; });

        REQUIRE(loop.step_rate() == steps_per_second{120});
    }
}

TEST_CASE("an edge sampled once a frame must be latched for the steps", "[fixed][input]") {
    constexpr int STALL_FRAME = 2;

    const auto run_with_latch = [](const bool aStall) {
        int frames = 0;
        int taken = 0;
        bool requested = false;

        game_loop(frames_per_second{1000},
            [&](const game_loop::frame) {
                if (frames == STALL_FRAME) {
                    requested = true;

                    if (aStall) std::this_thread::sleep_for(std::chrono::milliseconds(60));
                }

                return ++frames >= 8 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
            },
            steps_per_second{500},
            [&](const game_loop::step) {
                if (requested) { ++taken; requested = false; }
                return game_loop::SHOULD_CONTINUE;
            }).run();

        return taken;
    };

    SECTION("one request is taken exactly once, whether or not the frame stalled") {
        REQUIRE(run_with_latch(false) == 1);
        REQUIRE(run_with_latch(true) == 1);
    }
}

TEST_CASE("time scale moves the world against the clock", "[fixed][scale]") {
    SECTION("a loop starts at normal scale") {
        int frames = 0;
        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            return ++frames >= 1 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        REQUIRE(loop.time_scale() == game_loop::NORMAL_TIME_SCALE);
        REQUIRE(loop.time_scale() == 1);
    }

    SECTION("paused, no step runs and no game time passes, but frames keep coming") {
        std::uint64_t steps = 0;
        int frames = 0;
        double gameTime = 0;
        double wallTime = 0;

        game_loop loop(frames_per_second{1000},
            [&](const game_loop::frame aFrame) {
                gameTime = aFrame.elapsed;
                wallTime += aFrame.unscaled_delta;
                return ++frames >= 60 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
            },
            steps_per_second{200},
            [&](const game_loop::step) { ++steps; return game_loop::SHOULD_CONTINUE; });

        loop.set_time_scale(game_loop::PAUSED);
        loop.run();

        REQUIRE(steps == 0);
        REQUIRE(gameTime == 0);

        REQUIRE(frames == 60);
        REQUIRE(wallTime > 0);
    }

    SECTION("half scale runs half as many steps for the same wall clock") {
        const auto steps_at = [](const double aScale) {
            std::uint64_t steps = 0;
            const auto started = std::chrono::steady_clock::now();

            game_loop loop(frames_per_second::uncapped(),
                [&](const game_loop::frame) {
                    return std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - started).count() > 0.25
                        ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
                },
                steps_per_second{400},
                [&](const game_loop::step) { ++steps; return game_loop::SHOULD_CONTINUE; });

            loop.set_time_scale(aScale);
            loop.run();

            return steps;
        };

        const auto full = steps_at(1.0);
        const auto half = steps_at(0.5);

        REQUIRE(half > 0);
        REQUIRE(static_cast<double>(half) < static_cast<double>(full) * 0.75);
        REQUIRE(static_cast<double>(half) > static_cast<double>(full) * 0.25);
    }

    SECTION("the step delta never changes, whatever the scale") {
        for (const double scale : {0.25, 1.0, 4.0}) {
            std::vector<double> deltas;

            game_loop loop(frames_per_second::uncapped(),
                [](const game_loop::frame) { return game_loop::SHOULD_CONTINUE; },
                steps_per_second{500},
                [&](const game_loop::step aStep) {
                    deltas.push_back(aStep.delta);
                    return aStep.index + 1 >= 10 ? game_loop::SHOULD_CLOSE
                                                 : game_loop::SHOULD_CONTINUE;
                });

            loop.set_time_scale(scale);
            loop.run();

            REQUIRE(deltas.size() == 10);

            for (const auto delta : deltas) REQUIRE(delta == 0.002);
        }
    }

    SECTION("game time runs at the scale while wall clock does not") {
        double gameTime = 0;
        double wallTime = 0;
        int frames = 0;

        game_loop loop(frames_per_second{1000}, [&](const game_loop::frame aFrame) {
            gameTime = aFrame.elapsed;
            wallTime += aFrame.unscaled_delta;
            return ++frames >= 100 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        loop.set_time_scale(0.5);
        loop.run();

        REQUIRE(wallTime > 0);
        REQUIRE(gameTime == Approx(wallTime * 0.5).epsilon(0.01));
    }

    SECTION("the scale can be changed while running") {
        std::uint64_t steps = 0;
        int frames = 0;

        game_loop loop(frames_per_second{2000},
            [&](const game_loop::frame) {
                if (frames == 20) loop.set_time_scale(game_loop::PAUSED);
                return ++frames >= 60 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
            },
            steps_per_second{500},
            [&](const game_loop::step) { ++steps; return game_loop::SHOULD_CONTINUE; });

        loop.run();

        const auto whileRunning = steps;

        REQUIRE(loop.time_scale() == game_loop::PAUSED);
        REQUIRE(whileRunning > 0);
    }

    SECTION("a scale that cannot be simulated is refused") {
        int frames = 0;
        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            return ++frames >= 1 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        REQUIRE_THROWS_AS(loop.set_time_scale(-1), exception);
        REQUIRE_THROWS_AS(loop.set_time_scale(std::nan("")), exception);
        REQUIRE_THROWS_AS(loop.set_time_scale(INFINITY), exception);
        REQUIRE(loop.time_scale() == game_loop::NORMAL_TIME_SCALE);
    }
}
