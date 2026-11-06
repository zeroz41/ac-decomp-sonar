# Animal Crossing on sonarlib

## What it is

A native PC build of Animal Crossing (GameCube, USA, `GAFE01_00`). It compiles the game's code from this decompilation for your computer and runs it on [sonarlib](https://github.com/zeroz41/sonarlib) instead of an emulator.

You need your own copy of the game. This repository holds no game data. The importer reads what the port needs from your disc.

**Status: early.** The game boots and runs to the first frame of play, then stops. Nothing is shown and there is no sound or input yet. A window and pictures come next.

## How to build

You need:

- the decompilation's requirements (see the [main README](../README.md#building)): Python, ninja and your disc in `orig/GAFE01_00`;
- CMake 3.25 or newer and a C/C++20 compiler (GCC or Clang);
- a checkout of sonarlib. The build looks for it next to this repository (`../sonarlib`) unless you pass `-DSONARLIB_DIR`.

First, build the decompilation once. The port reads files it generates:

```sh
python configure.py
ninja
```

Then build the port:

```sh
cmake -S port -B port/build -G Ninja -DAC_DISC=orig/GAFE01_00
cmake --build port/build
```

`AC_DISC` is optional. It points at your disc, either an image or an extracted folder, and is only used by the tests.

## How to run

Import your disc once. This writes the game's data into a package file:

```sh
port/build/ac-import orig/GAFE01_00 ac.pkg import-cache
```

Then run the game:

```sh
port/build/ac_port --package ac.pkg
```

It prints one `ac_port: play …` line when it reaches play's first frame and exits with 0. It exits with 1 if it fails or hits the tick limit, and with 2 for bad arguments.

Options:

| Option | What it does |
|---|---|
| `--ticks <n>` | gives up after `n` game frames if play hasn't started (3600 by default) |
| `--validate` | rechecks the game's display lists on every use |
| `--diff` | draws each frame's 3D twice, once through sonarlib and once through the game's own renderer (emu64), and compares the two |

To run the tests:

```sh
ctest --test-dir port/build
```

The tests that need your disc are skipped when `AC_DISC` isn't set.

For how the port works inside (the package, the glue, every edit to the game's code), see [NOTES.md](NOTES.md).
