// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>

#include <gdk/timing/game_loop.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace gdk::timing;

namespace {
    constexpr double GAP_SECONDS = 0.03;

    [[nodiscard]] std::vector<game_loop::frame> ticked_with_gaps(game_loop &aLoop, const int aTicks) {
        std::vector<game_loop::frame> seen;

        for (int i = 0; i < aTicks; ++i) {
            (void)aLoop.tick();

            std::this_thread::sleep_for(std::chrono::duration<double>(GAP_SECONDS));
        }

        return seen;
    }
}

TEST_CASE("a frame is measured from one tick's start to the next's", "[tick][timing]") {
    std::vector<game_loop::frame> seen;

    game_loop loop(frames_per_second::uncapped(), [&](const game_loop::frame aFrame) {
        seen.push_back(aFrame);
        return game_loop::SHOULD_CONTINUE;
    });

    SECTION("the wait between ticks belongs to the frame, whoever calls tick") {
        const auto started = std::chrono::steady_clock::now();

        (void)ticked_with_gaps(loop, 4);

        const auto wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

        REQUIRE(seen.size() == 4);
        REQUIRE(seen[0].delta == 0);
        REQUIRE(seen[0].elapsed == 0);

        for (std::size_t i = 1; i < seen.size(); ++i) {
            INFO("frame " << i);

            REQUIRE(seen[i].delta >= GAP_SECONDS * 0.8);
            REQUIRE(seen[i].elapsed == Approx(seen[i - 1].elapsed + seen[i].delta));
        }

        REQUIRE(seen.back().elapsed >= (wall - GAP_SECONDS) * 0.8);
        REQUIRE(seen.back().elapsed <= wall);
    }

    SECTION("a gap longer than the ceiling is clamped, as a long frame under run is") {
        game_loop capped(frames_per_second::uncapped(), [&](const game_loop::frame aFrame) {
            seen.push_back(aFrame);
            return game_loop::SHOULD_CONTINUE;
        }, 0.01);

        (void)capped.tick();

        std::this_thread::sleep_for(std::chrono::milliseconds(40));

        (void)capped.tick();

        REQUIRE(seen.size() == 2);
        REQUIRE(seen[1].delta == Approx(0.01));
    }

    SECTION("reset forgets the last tick, so the first frame after it reports no time again") {
        (void)loop.tick();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        loop.reset();

        (void)loop.tick();

        REQUIRE(seen.size() == 2);
        REQUIRE(seen[1].delta == 0);
        REQUIRE(seen[1].elapsed == 0);
        REQUIRE(seen[1].index == 0);
    }
}
