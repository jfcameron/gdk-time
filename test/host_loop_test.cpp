// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>

#include <gdk/timing/game_loop.h>
#include <gdk/timing/exception.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

using namespace gdk::timing;

TEST_CASE("a caller's own loop can drive the library", "[tick]") {
    SECTION("one tick is one frame") {
        std::vector<std::uint64_t> indices;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame aFrame) {
            indices.push_back(aFrame.index);
            return game_loop::SHOULD_CONTINUE;
        });

        for (int i = 0; i < 5; ++i) REQUIRE(loop.tick() != game_loop::SHOULD_CLOSE);

        REQUIRE(indices == std::vector<std::uint64_t>{0, 1, 2, 3, 4});
    }

    SECTION("the shape a browser needs: a host loop that stops when told") {
        int frames = 0;
        int hostIterations = 0;
        bool cancelled = false;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            return ++frames >= 10 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        while (!cancelled && hostIterations < 100) {
            ++hostIterations;

            if (loop.tick() == game_loop::SHOULD_CLOSE) cancelled = true;
        }

        REQUIRE(frames == 10);
        REQUIRE(hostIterations == 10);
        REQUIRE(cancelled);
    }

    SECTION("a finished loop stays finished, and does not run its behaviours again") {
        int frames = 0;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            ++frames;
            return game_loop::SHOULD_CLOSE;
        });

        REQUIRE(loop.tick() == game_loop::SHOULD_CLOSE);
        REQUIRE(frames == 1);

        for (int i = 0; i < 5; ++i) REQUIRE(loop.tick() == game_loop::SHOULD_CLOSE);

        REQUIRE(frames == 1);
    }

    SECTION("reset starts it again") {
        std::vector<std::uint64_t> indices;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame aFrame) {
            indices.push_back(aFrame.index);
            return aFrame.index >= 2 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        while (loop.tick() != game_loop::SHOULD_CLOSE);

        loop.reset();

        while (loop.tick() != game_loop::SHOULD_CLOSE);

        REQUIRE(indices == std::vector<std::uint64_t>{0, 1, 2, 0, 1, 2});
    }

    SECTION("ticking drives fixed steps, exactly as running does") {
        std::vector<std::uint64_t> stepIndices;
        int frames = 0;

        game_loop loop(frames_per_second::uncapped(),
            [&](const game_loop::frame) {
                return ++frames >= 40 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
            },
            steps_per_second{2000},
            [&](const game_loop::step aStep) {
                stepIndices.push_back(aStep.index);
                return game_loop::SHOULD_CONTINUE;
            });

        while (loop.tick() != game_loop::SHOULD_CLOSE);

        REQUIRE(frames == 40);

        for (std::size_t i = 0; i < stepIndices.size(); ++i) REQUIRE(stepIndices[i] == i);
    }
}

TEST_CASE("run is a loop around tick and nothing more", "[tick][dogfood]") {
    struct body final {
        double y{100};
        double v{0};

        void integrate(const double aDeltaTime) { v -= 9.81 * aDeltaTime; y += v * aDeltaTime; }
    };

    const auto simulate = [](const bool aUseRun) {
        body simulated;
        std::uint64_t steps = 0;

        game_loop loop(frames_per_second::uncapped(),
            [](const game_loop::frame) { return game_loop::SHOULD_CONTINUE; },
            steps_per_second{500},
            [&](const game_loop::step aStep) {
                simulated.integrate(aStep.delta);
                ++steps;
                return aStep.index + 1 >= 100 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
            });

        if (aUseRun) loop.run();
        else while (loop.tick() != game_loop::SHOULD_CLOSE);

        return std::make_pair(simulated.y, steps);
    };

    const auto viaRun = simulate(true);
    const auto viaTick = simulate(false);

    REQUIRE(viaRun.second == 100);
    REQUIRE(viaTick.second == viaRun.second);
    REQUIRE(viaTick.first == viaRun.first);
}

TEST_CASE("a tick cannot be driven twice at once", "[tick][reentrancy]") {
    SECTION("ticking from inside a behaviour throws") {
        int frames = 0;
        bool threw = false;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            try { (void)loop.tick(); } catch (const exception &) { threw = true; }
            return ++frames >= 2 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        loop.run();

        REQUIRE(threw);
    }

    SECTION("resetting from inside a behaviour throws, rather than clearing the frame under it") {
        int frames = 0;
        bool threw = false;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            try { loop.reset(); } catch (const exception &) { threw = true; }
            return ++frames >= 2 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        loop.run();

        REQUIRE(threw);
    }
}

TEST_CASE("the loop can be stopped from outside a behaviour", "[tick][stop]") {
    SECTION("a request between ticks stops it before the next frame") {
        int frames = 0;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            ++frames;
            return game_loop::SHOULD_CONTINUE;
        });

        REQUIRE(loop.tick() == game_loop::SHOULD_CONTINUE);
        REQUIRE(frames == 1);
        REQUIRE_FALSE(loop.stop_requested());

        loop.request_stop();

        REQUIRE(loop.stop_requested());
        REQUIRE(loop.tick() == game_loop::SHOULD_CLOSE);

        REQUIRE(frames == 1);
    }

    SECTION("a request from a behaviour does not wait out the frame rate cap") {
        const auto started = std::chrono::steady_clock::now();
        int frames = 0;

        game_loop loop(frames_per_second{4}, [&](const game_loop::frame) {
            ++frames;

            loop.request_stop();

            return game_loop::SHOULD_CONTINUE;
        });

        loop.run();

        REQUIRE(frames == 1);
        REQUIRE(std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count() < 0.1);
    }

    SECTION("a request from another thread ends a running loop") {
        std::atomic<bool> started{false};
        std::uint64_t frames = 0;

        game_loop loop(frames_per_second{2000}, [&](const game_loop::frame) {
            started.store(true, std::memory_order_release);
            ++frames;
            return game_loop::SHOULD_CONTINUE;
        });

        std::thread stopper([&] {
            while (!started.load(std::memory_order_acquire)) std::this_thread::yield();

            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            loop.request_stop();
        });

        loop.run();
        stopper.join();

        REQUIRE(loop.stop_requested());
        REQUIRE(frames > 0);
    }

    SECTION("requesting twice is harmless") {
        int frames = 0;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame) {
            ++frames;
            return game_loop::SHOULD_CONTINUE;
        });

        loop.request_stop();
        loop.request_stop();
        loop.run();

        REQUIRE(frames == 0);
    }

    SECTION("reset clears the request, since starting again is what reset means") {
        int frames = 0;

        game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame aFrame) {
            ++frames;
            return aFrame.index >= 2 ? game_loop::SHOULD_CLOSE : game_loop::SHOULD_CONTINUE;
        });

        loop.request_stop();
        loop.run();

        REQUIRE(frames == 0);
        REQUIRE(loop.stop_requested());

        loop.reset();

        REQUIRE_FALSE(loop.stop_requested());

        loop.run();

        REQUIRE(frames == 3);
    }
}
