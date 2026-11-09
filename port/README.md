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

Close the window to quit. F11 or Alt+Enter switches between a window and fullscreen at once, even while the overlay is shown, and the next run opens the same way. F1 shows or hides sonarlib's overlay over the game, with its settings, its statistics, and an Animal Crossing page listing the controls, where your files are and the memory card in each slot. While the overlay is shown, the game doesn't get the keyboard or gamepads.

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

Without `--window` it runs headless, as the tests do: it runs to the first frame of play as fast as it can, prints one `ac_port: play …` line and exits with 0. Among the line's counts, `unowned` counts the actors drawn with no identity of their own, and `strays` the markers the game's draws left that opened or closed nothing; both are 0 when every moving thing is kept apart. Its sound is mixed but not played. It exits with 1 if it fails or hits the tick limit, and with 2 for bad arguments.

Options:

| Option | What it does |
|---|---|
| `--window` | shows the game in a window, plays its sound, and reads the keyboard and gamepads; each image it shows blends the game's last two frames at the clock's alpha for the time it is shown |
| `--user-dir <dir>` | with `--window`: keeps every user directory under `dir` instead of your own, the settings file at `dir/config/settings.toml`, the memory cards in `dir/saves` and the pipeline list in `dir/cache`, for a portable install |
| `--set <key>=<value>` | with `--window`, any number of times: sets a setting for this run only, over the settings file, and never saves it, as in `--set sonar.audio.volume=30`. One that isn't `key=value`, names no setting the port has, or gives a value its setting doesn't take prints `ac_port: --set <key>=<value>: …` and exits with 2 before the game starts |
| `--ticks <n>` | headless: gives up after `n` game frames if play hasn't started (3600 by default); with `--window`, `--measure` or `--replay`: quits after `n` game frames (no limit by default with `--window` or `--replay`) |
| `--validate` | rechecks the game's display lists on every use |
| `--diff` | draws each frame's 3D twice, once through sonarlib and once through the game's own renderer (emu64), and compares the two |
| `--goldens <scenes> <dir>` | headless, on the GPU with no window: writes the frames a scenes file names into `dir`, for the goldens test |
| `--measure` | headless, on the GPU with no window: runs to the tick limit, presenting each frame once unless `--present-hz` is given, and prints how long the game, the renderer's preparation and each present took from play's first frame on, and what the presents drew, then a presents line: the presents, those held, the display times passed late, the frames dropped, any present going backward, and the snaps by cause |
| `--present-hz <n>` | with `--measure`, not with `--reference`: presents each frame at a display's rate of `n` Hz (1 to 1000) instead of once, at each of the display's times in the game frame it is handed over in, blended between frames as the clock says |
| `--reference` | draws every frame with the game's own renderer (emu64) through the GX front end instead of sonarlib's kit, to measure the kit against; with `--measure` or `--goldens`, not with `--window`, `--diff` or `--validate` |
| `--replay <recording>` | headless: plays the controllers from a recording, which sonarlib's `sl-replay` (built at `port/build/sonarlib/tools/sl-replay`) builds from a script of presses and moves, such as the town replay test's `port/tests/town_replay.txt`. It runs past the first frame of play until the recording ends, prints an `ac_port: replay …` line and exits with 0; each line it prints ends with the scene the game is in. Not with `--window`, `--goldens`, `--measure`, `--diff` or `--reference` |
| `--qrand-seed <hex>` | seeds the game's random numbers with this value instead of the clock, as the goldens test does with Dolphin's |

To run the tests:

```sh
ctest --test-dir port/build
```

The tests that need your disc are skipped when `AC_DISC` isn't set.

For how the port works inside (the package, the glue, every edit to the game's code), see [NOTES.md](NOTES.md).

## Settings

With `--window` the port keeps your settings in `settings.toml` and prints where it is as it starts, as `ac_port: settings <path>`. On Linux that is `~/.config/sonarlib/animal-crossing/settings.toml`, and your user directory is `~/.local/share/sonarlib/animal-crossing`. With `--user-dir <dir>` they are `<dir>/config/settings.toml` and `<dir>`. The overlay's Animal Crossing page shows both. Runs without `--window` never read or write them.

The first time the game shows a scene, the GPU has to prepare how it draws it, which can make the first moments stutter. The port remembers what it prepared in `pipelines.slpc`, written a few seconds after each new scene settles, and prepares all of it before the next run's first frame. On Linux it is `~/.cache/sonarlib/animal-crossing/pipelines.slpc`, or `<dir>/cache/pipelines.slpc` with `--user-dir <dir>`. Deleting it is safe: the port only stutters again until it has written it anew.

Change settings on the overlay's Settings page, or edit the file while the game isn't running. A change is saved as soon as the overlay is hidden: one made on the overlay once you close it, F11's at once, or when you close the overlay if it is shown, and any still unsaved when the game quits. A save writes the file anew, without your comments. A run that changes nothing leaves the file as it is, and `--set` is only for that run. Removing a line from the file restores that setting's default. The ones that matter most:

| Setting | Values |
|---|---|
| `sonar.video.mode` | `windowed` (the default), `borderless` or `exclusive` fullscreen, on the display the window is on. F11 and Alt+Enter set it. If the display refuses a mode, the window stays as it was and the setting keeps your choice, to try again next run; F11 then switches between a window and the last fullscreen the display took, borderless if it took none |
| `sonar.video.pacing` | `vsync` (the default), one frame per refresh; `mailbox`, VSync with less latency; `immediate`, with no waiting and tearing allowed; or `limiter`, at `frame_limit`'s rate |
| `sonar.video.frame_limit` | the limiter's rate in Hz, 0 to 1000; 0 (the default) is the display's refresh rate |
| `sonar.audio.volume` | 0 to 100, 100 by default |
| `sonar.input.rumble` | the rumble's strength, 0 to 100, 100 by default |
| `sonar.system.sound_mode` | `mono`, `stereo` or `surround`: the game's options set it, and it lasts, as the console kept it |

If the file doesn't parse, the port prints where, as `ac_port: <path>:<line>:<column>: <message>; it is left as it is`, and the game runs at the default settings, with any `--set`. It doesn't save the file that run, so your text is never overwritten and nothing you change that run is kept. Fix the line or delete the file.

## Saves

With `--window` the game saves to memory cards that the port keeps as files. On Linux they are in `~/.local/share/sonarlib/animal-crossing/saves`, or `<dir>/saves` with `--user-dir <dir>`, and the overlay's Animal Crossing page shows where. A card is `<name>.slsave`, its header, and one `<name>-f<nnn>.slsave` for each file the game saved on it, `<nnn>` being the file's number on the card from `000`. Each write keeps the copy it replaces as `.slsave.1` and the one before that as `.slsave.2`, and a save that is damaged loads from its newest good copy. The files hold the bytes a GameCube writes to its card, except the ones listed below.

The two slots are settings:

| Setting | Values |
|---|---|
| `kit.card.slot_a` | the card in slot A, `card-a` by default |
| `kit.card.slot_b` | the card in slot B, none by default |

A card's name is 1 to 59 characters of `a` to `z`, `0` to `9`, `_` and `-`. An empty value leaves the slot empty. Change them on the overlay's Animal Crossing page, under Memory cards, in `settings.toml` while the game isn't running, or for one run with `--set`, as in `--set kit.card.slot_b=card-b`. sonarlib's Settings page doesn't list them.

A change applies at the game's next frame, as pulling one card out of the console and putting the other in does, so don't change a slot while the game is saving. Changing both at once swaps the two cards. A name the port has no card for yet gets a new, formatted 16 Mbit card. A name that isn't a card's name, the card already in the other slot, or a card that can't be read leaves the slot empty, and the port logs why. The overlay page still shows the name you set.

Runs without `--window` have no cards: the game finds both slots empty.

Not saved as a GameCube saves them yet:

- The NES games' saves. The NES games don't start in the port yet, and once they do, parts of their saves will be in your computer's byte order rather than the GameCube's.
- Anything that needs the Game Boy Advance link: the port has no Game Boy Advance to connect.

### Moving a town to and from Dolphin

sonarlib's `sl-card`, built with the port at `port/build/sonarlib/tools/sl-card`, copies saves between the port's cards and Dolphin's. Quit both the game and Dolphin first: each holds its cards while it runs, and nothing stops the other from changing them meanwhile. Give it the port's saves as `--app animal-crossing --org sonarlib`, or `--user-dir <dir>` if you play with `--user-dir`.

Dolphin keeps the US game's slot A card in one of two places, under its user folder (`~/.local/share/dolphin-emu`, or `~/.dolphin-emu` if you have one, on Linux; `~/Library/Application Support/Dolphin` on macOS; `%APPDATA%\Dolphin Emulator`, or `Documents\Dolphin Emulator` if you have one, on Windows):

- a GCI folder, Dolphin's default: `GC/USA/Card A`;
- a card image, if you picked one in Dolphin's settings: `GC/MemoryCardA.USA.raw`, or with the card's blocks before `.raw` for a card smaller than 2043 blocks, as in `MemoryCardA.USA.251.raw`.

The port's first `--window` run makes `card-a`, so bringing a card from Dolphin over it needs `--replace`. Or import it under another name and set that name as `kit.card.slot_a`. On Linux, with Dolphin's GCI folder:

```sh
sl=port/build/sonarlib/tools/sl-card
dolphin=~/.local/share/dolphin-emu/GC/USA/'Card A'
# Dolphin's whole card, as card-a:
$sl --app animal-crossing --org sonarlib import card-a "$dolphin" --replace
# or only Animal Crossing's files, over the ones card-a has:
$sl --app animal-crossing --org sonarlib add card-a "$dolphin" --game GAFE --replace
# and back to Dolphin:
$sl --app animal-crossing --org sonarlib extract card-a "$dolphin" --game GAFE --replace
```

`extract` writes each file over the one Dolphin loads for it, so your other games' saves in the folder stay as they are. With a card image, `import card-a <image> --replace` brings it in, and `export card-a <image> --replace` writes the whole card back as an image. `sl-card` with no arguments lists every command.
