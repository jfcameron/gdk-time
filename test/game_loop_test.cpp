// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>

#include <gdk/timing/game_loop.h>
#include <gdk/timing/exception.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace gdk::timing;

namespace {
    [[nodiscard]] auto close_after(int &aCounter, const int aFrames) {
        return [&aCounter, aFrames](const game_loop::frame) {
            return ++aCounter >= aFrames ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        };
    }

    [[nodiscard]] double seconds_since(const std::chrono::steady_clock::time_point a) {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
    }
}

TEST_CASE("a loop runs its behaviour and stops when told", "[loop]") {
    SECTION("exactly as many times as asked") {
        int frames = 0;

        game_loop(frames_per_second::uncapped(), close_after(frames, 10)).run();

        REQUIRE(frames == 10);
    }

    SECTION("the first frame reports no time having passed") {
        game_loop::frame first{-1, -1, -1, -1, 999};

        game_loop(frames_per_second::uncapped(), [&](const game_loop::frame aFrame) {
            first = aFrame;
            return game_loop::SHOULD_CLOSE;
        }).run();

        REQUIRE(first.elapsed == 0);
        REQUIRE(first.delta == 0);
        REQUIRE(first.index == 0);
    }

    SECTION("elapsed is the time at the start of the frame, not one frame behind it") {
        std::vector<game_loop::frame> seen;

        int frames = 0;
        game_loop(frames_per_second{500}, [&](const game_loop::frame aFrame) {
            seen.push_back(aFrame);
            return ++frames >= 5 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        }).run();

        REQUIRE(seen.size() == 5);

        for (std::size_t i = 1; i < seen.size(); ++i) {
            REQUIRE(seen[i].delta > 0);
            REQUIRE(seen[i].index == i);

            REQUIRE(seen[i].elapsed == Approx(seen[i - 1].elapsed + seen[i].delta));
        }
    }

    SECTION("interpolation stays zero when there is nothing to interpolate between") {
        int frames = 0;
        double highest = -1;

        game_loop(frames_per_second{1000}, [&](const game_loop::frame aFrame) {
            highest = std::max(highest, aFrame.interpolation);
            return ++frames >= 10 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        }).run();

        REQUIRE(highest == 0);
        REQUIRE(frames == 10);
    }
}

TEST_CASE("the frame rate cap paces the loop", "[loop][pacing]") {
    SECTION("a capped loop takes at least the time its cap implies") {
        int frames = 0;
        const auto started = std::chrono::steady_clock::now();

        game_loop(frames_per_second{200}, close_after(frames, 20)).run();

        const auto elapsed = seconds_since(started);

        REQUIRE(elapsed > 0.019 * 0.8);
        REQUIRE(elapsed < 1.0);
    }

    SECTION("an uncapped loop does not wait") {
        int frames = 0;
        const auto started = std::chrono::steady_clock::now();

        game_loop(frames_per_second::uncapped(), close_after(frames, 20000)).run();

        REQUIRE(frames == 20000);
        REQUIRE(seconds_since(started) < 0.5);
    }

    SECTION("the cap can be changed while running, and takes effect immediately") {
        int frames = 0;

        game_loop loop(frames_per_second{500}, [&](const game_loop::frame) {
            if (frames == 5) loop.set_maximum_frames_per_second(frames_per_second::uncapped());
            return ++frames >= 10 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        loop.run();

        REQUIRE(loop.maximum_frames_per_second() == frames_per_second::uncapped());
    }

    SECTION("the cap can be changed from another thread while the loop runs") {
        std::atomic<bool> running{false};
        int frames = 0;

        game_loop loop(frames_per_second{2000}, [&](const game_loop::frame) {
            running.store(true, std::memory_order_release);
            return ++frames >= 400 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        std::thread setter([&] {
            while (!running.load(std::memory_order_acquire)) std::this_thread::yield();

            for (int i = 0; i < 200; ++i)
                loop.set_maximum_frames_per_second(frames_per_second(1000 + i));
        });

        loop.run();
        setter.join();

        REQUIRE(frames == 400);
        REQUIRE(loop.maximum_frames_per_second() == frames_per_second{1199});
    }

    SECTION("a delta longer than the ceiling is clamped before anyone sees it") {
        int frames = 0;
        double largest = 0;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame aFrame) {
            largest = std::max(largest, aFrame.delta);

            if (frames == 1) std::this_thread::sleep_for(std::chrono::milliseconds(120));

            return ++frames >= 4 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        }, 0.05);

        loop.run();

        REQUIRE(largest <= 0.05);
        REQUIRE(largest == Approx(0.05));
    }
}

TEST_CASE("a loop refuses what it cannot run", "[loop][validation]") {
    const auto behavior = [](const game_loop::frame) { return game_loop::SHOULD_CLOSE; };

    SECTION("a negative or non-numeric frame rate") {
        REQUIRE_THROWS_AS(game_loop(frames_per_second{-1}, behavior), exception);
        REQUIRE_THROWS_AS(game_loop(frames_per_second{std::nan("")}, behavior), exception);
        REQUIRE_THROWS_AS(game_loop(frames_per_second{INFINITY}, behavior), exception);
    }

    SECTION("zero is not an error: it is how uncapped is spelled") {
        REQUIRE_NOTHROW(game_loop(frames_per_second{0}, behavior));
    }

    SECTION("a delta ceiling of zero or less, which would clamp every delta to nothing") {
        REQUIRE_THROWS_AS(game_loop(frames_per_second{60}, behavior, 0.0), exception);
        REQUIRE_THROWS_AS(game_loop(frames_per_second{60}, behavior, -1.0), exception);
    }

    SECTION("an empty behaviour, rather than a bad_function_call from inside the first frame") {
        REQUIRE_THROWS_AS(game_loop(frames_per_second{60}, game_loop::loop_behavior_type{}),
            exception);
    }

    SECTION("the setter judges a rate by the same rule the constructor does") {
        int frames = 0;
        game_loop loop(frames_per_second{60}, close_after(frames, 1));

        REQUIRE_THROWS_AS(loop.set_maximum_frames_per_second(frames_per_second{-1}),
            exception);

        REQUIRE(loop.maximum_frames_per_second() == frames_per_second{60});
    }
}

TEST_CASE("a loop runs once at a time", "[loop][reentrancy]") {
    SECTION("running from inside a running loop throws") {
        int frames = 0;
        bool threw = false;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            try { loop.run(); } catch (const exception &) { threw = true; }
            return ++frames >= 2 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        loop.run();

        REQUIRE(threw);
    }

    SECTION("a behaviour that throws does not strand the loop") {
        struct from_the_behaviour final {};

        game_loop loop(frames_per_second::uncapped(), [](const game_loop::frame) -> bool {
            throw from_the_behaviour{};
        });

        REQUIRE_THROWS_AS(loop.run(), from_the_behaviour);

        REQUIRE_THROWS_AS(loop.run(), from_the_behaviour);
    }

    SECTION("a finished loop stays finished until it is reset, however it is driven") {
        std::vector<std::uint64_t> firstIndices;
        std::vector<std::uint64_t> secondIndices;
        std::vector<std::uint64_t> *pTarget = &firstIndices;

        int frames = 0;
        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame aFrame) {
            pTarget->push_back(aFrame.index);
            return ++frames >= 3 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        loop.run();

        frames = 0;
        pTarget = &secondIndices;

        loop.run();

        REQUIRE(firstIndices == std::vector<std::uint64_t>{0, 1, 2});
        REQUIRE(secondIndices.empty());

        loop.reset();
        loop.run();

        REQUIRE(secondIndices == firstIndices);
    }
}

TEST_CASE("closing does not wait out the frame budget", "[loop][pacing]") {
    SECTION("a behaviour that closes immediately returns immediately") {
        const auto started = std::chrono::steady_clock::now();

        game_loop(frames_per_second{4}, [](const game_loop::frame) {
            return game_loop::SHOULD_CLOSE;
        }).run();

        REQUIRE(seconds_since(started) < 0.1);
    }

    SECTION("and so does one closed by a fixed step") {
        const auto started = std::chrono::steady_clock::now();
        int frames = 0;

        game_loop(frames_per_second{4},
            [&](const game_loop::frame) { ++frames; return game_loop::SHOULD_CONTINUE; },
            steps_per_second{1000},
            [](const game_loop::step) { return game_loop::SHOULD_CLOSE; }).run();

        REQUIRE(frames == 1);
        REQUIRE(seconds_since(started) < 0.4);
    }
}
