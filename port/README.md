# Animal Crossing on sonarlib

## What it is

A native PC build of Animal Crossing (GameCube, USA, `GAFE01_00`). It compiles the game's code from this decompilation for your computer and runs it on [sonarlib](https://github.com/zeroz41/sonarlib) instead of an emulator.

You need your own copy of the game. This repository holds no game data. The importer reads what the port needs from your disc.

**Status: early.** The game runs in a window with its music and sounds, the boot jingle among them, and reads your keyboard and gamepads.

## How to build

You need:

- the decompilation's requirements (see the [main README](../README.md#building)): Python, ninja and your disc in `orig/GAFE01_00`;
- CMake 3.25 or newer and a C/C++20 compiler (GCC or Clang);
- a GPU with Vulkan drivers, for the window;
- a checkout of sonarlib. The build looks for it next to this repository (`../sonarlib`) unless you pass `-DSONARLIB_DIR`.

First, build the decompilation once. The port reads files it generates:

```sh
python configure.py
ninja
```

If you built it before the port played the boot jingle, build it again: the port needs the jingle's files it now extracts, and its build stops and says so without them.

Then build the port:

```sh
cmake -S port -B port/build -G Ninja -DAC_DISC=orig/GAFE01_00
cmake --build port/build
```

`AC_DISC` is optional. It points at your disc, either an image or an extracted folder, and is only used by the tests.

The goldens test, `ac_port_goldens`, also needs Dolphin, as the reference the port's pictures are compared with. Build its headless player from upstream's tag 2609 and pass it as `AC_DOLPHIN`:

```sh
git clone --branch 2609 --recurse-submodules https://github.com/dolphin-emu/dolphin.git
cmake -S dolphin -B dolphin/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT=OFF
cmake --build dolphin/build --target dolphin-nogui
cmake -S port -B port/build -G Ninja -DAC_DISC=orig/GAFE01_00 -DAC_DOLPHIN=$PWD/dolphin/build/Binaries/dolphin-emu-nogui
```

It runs with no window, through SDL's offscreen video driver and Mesa's lavapipe Vulkan driver, which it needs installed (`lvp_icd*.json`). The first run plays the disc in Dolphin, which takes under a minute, and keeps its frames in the build tree; later runs reuse them.

## How to run

Import your disc once. This writes the game's data into a package file. Run it again when `ac_port` says the package is from another version of the port, as it does after an update that changes the package:

```sh
port/build/ac-import orig/GAFE01_00 ac.pkg import-cache
```

Then play the game in a window, with sound:

```sh
port/build/ac_port --package ac.pkg --window
```

Close the window to quit.

| Keyboard | GameCube |
|---|---|
| W A S D | control stick |
| I J K L | C-stick |
| Space | A |
| Left Shift | B |
| E | X |
| Q | Y |
| R | Z |
| Return | Start |
| arrow keys | D-pad |
| 1 / 3 | L / R |

The keyboard is controller 1. Gamepads work too: each one you plug in takes the next free controller.

Without `--window` it runs headless, as the tests do: it runs to the first frame of play as fast as it can, prints one `ac_port: play …` line and exits with 0. Its sound is mixed but not played. It exits with 1 if it fails or hits the tick limit, and with 2 for bad arguments.

Options:

| Option | What it does |
|---|---|
| `--window` | shows the game in a window, plays its sound, and reads the keyboard and gamepads |
| `--ticks <n>` | headless: gives up after `n` game frames if play hasn't started (3600 by default); with `--window` or `--measure`: quits after `n` game frames (no limit by default with `--window`) |
| `--validate` | rechecks the game's display lists on every use |
| `--diff` | draws each frame's 3D twice, once through sonarlib and once through the game's own renderer (emu64), and compares the two |
| `--goldens <scenes> <dir>` | headless, on the GPU with no window: writes the frames a scenes file names into `dir`, for the goldens test |
| `--measure` | headless, on the GPU with no window: runs to the tick limit, presenting each frame once unless `--present-hz` is given, and prints how long the game, the renderer's preparation and each present took from play's first frame on, and what the presents drew, then a presents line: the presents, those held, the display times passed late, the frames dropped, any present going backward, and the snaps by cause |
| `--present-hz <n>` | with `--measure`, not with `--reference`: presents each frame at a display's rate of `n` Hz (1 to 1000) instead of once, at each of the display's times in the game frame it is handed over in, blended between frames as the clock says |
| `--reference` | draws every frame with the game's own renderer (emu64) through the GX front end instead of sonarlib's kit, to measure the kit against; with `--measure` or `--goldens`, not with `--window`, `--diff` or `--validate` |
| `--qrand-seed <hex>` | seeds the game's random numbers with this value instead of the clock, as the goldens test does with Dolphin's |

To run the tests:

```sh
ctest --test-dir port/build
```

The tests that need your disc are skipped when `AC_DISC` isn't set.

For how the port works inside (the package, the glue, every edit to the game's code), see [NOTES.md](NOTES.md).
