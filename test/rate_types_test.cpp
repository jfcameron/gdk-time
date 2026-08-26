// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>

#include <gdk/timing/types.h>

using namespace gdk::timing;

TEST_CASE("rate types", "[rates]") {
    SECTION("uncapped is zero, and says so by name") {
        REQUIRE(frames_per_second::uncapped().value == 0);
        REQUIRE(frames_per_second::uncapped() == frames_per_second{0});
    }

    SECTION("equality is on the value") {
        REQUIRE(frames_per_second{60} == frames_per_second{60});
        REQUIRE(frames_per_second{60} != frames_per_second{30});
        REQUIRE(steps_per_second{60} == steps_per_second{60});
        REQUIRE(steps_per_second{60} != steps_per_second{50});
    }

    SECTION("a rate computed at runtime can be written") {
        int fromSomeSetting = 0;

        for (int i = 0; i < 3; ++i) fromSomeSetting = 60 + i;

        const frames_per_second cap(fromSomeSetting);

        REQUIRE(cap.value == 62);
        REQUIRE(cap == frames_per_second{62});
    }

    SECTION("usable in a constant expression, so a rate can be a compile time constant") {
        constexpr frames_per_second sixty{60};
        static_assert(sixty.value == 60, "");
        static_assert(frames_per_second::uncapped().value == 0, "");
        static_assert(sixty != frames_per_second::uncapped(), "");
        REQUIRE(sixty.value == 60);
    }
}
