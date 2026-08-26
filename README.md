## gdk-time

game development kit timing lib.

Example:

```cpp
#include <gdk/timing/game_loop.h>

using namespace gdk::timing;

game_loop(frames_per_second{60},
    [&](const game_loop::frame aFrame) {
        render(aFrame.interpolation);
        return game_loop::SHOULD_CONTINUE;
    },
    steps_per_second{60},
    [&](const game_loop::step aStep) {
        world.integrate(aStep.delta);
        return game_loop::SHOULD_CONTINUE;
    }).run();
```
