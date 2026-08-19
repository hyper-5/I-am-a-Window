# I am a Window

Shrinks the **Geometry Dash** game window into a compact player box and projects a panoramic level overlay across your physical desktop!

---

## Features

* The game window automatically scales down and follows Player 1's physical position in real time.
* Adaptive surface: perfectly aligns the bottom border to the ground and smoothly transitions to anchor the top window border to the ceiling under inverted gravity.
* Fully compatible with Mini mode, vehicle resizing, and Scale Trigger transformations.
* Automatically spawns an independent **2P inverted mirror window** when entering Dual Mode.

---

## Notes

* **Please set the game to windowed mode and the aspect ratio to 16:9, otherwise you might encounter bugs.**
* Requires native Windows desktop composition APIs; **Windows only**.
* Exiting a level or resetting automatically restores the game window to its original size, position, and z-order.
* If you run into any camera bugs, try increasing the window_scale_ratio in config.json inside the mod directory.

## Building from Source

### Prerequisites
* [Geode CLI](https://github.com/geode-sdk/cli) installed and configured.
* **Clang** compiler with C++23 support.
* CMake 3.27+.

### Build
   ```bash
   geode build
   ```