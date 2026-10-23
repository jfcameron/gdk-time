# gdk-time
game development kit timing lib.

## Getting Started
Install cmake 3.21 or greater, and a c++20 compatible compiler 
```
# initialize the tree
git submodule update --init --recursive

# generate the script to build the library on linux using gcc
cmake --preset linux-gcc

# build the library
cmake --build --preset linux-gcc
```
See `CMakePresets.json` for the full list of supported platforms and compilers. See `.github/workflows/ci.yml` to see exactly how CI runners build it on different platforms.

## Usage Example
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
