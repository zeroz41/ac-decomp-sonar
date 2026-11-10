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
| `--window` | shows the game in a window, plays its sound, and reads the keyboard and gamepads; each image it shows blends the game's last two frames at the clock's alpha for the time it is shown, unless `sonar.video.interpolation` is off |
| `--user-dir <dir>` | with `--window`: keeps every user directory under `dir` instead of your own, the settings file at `dir/config/settings.toml`, the memory cards in `dir/saves` and the pipeline list in `dir/cache`, for a portable install |
| `--set <key>=<value>` | with `--window`, or with `--display` as below, any number of times: sets a setting for this run only, over the settings file, and never saves it, as in `--set sonar.audio.volume=30`. One that isn't `key=value`, names no setting the port has, or gives a value its setting doesn't take prints `ac_port: --set <key>=<value>: …` and exits with 2 before the game starts |
| `--resize <tick> <width>x<height>` | with `--window`, up to 8 times with rising ticks: resizes the window to that size once game frame `tick` has begun and prints `ac_port: resize <tick> <width>x<height>: <result>`, as the state test does |
| `--ticks <n>` | headless: gives up after `n` game frames if play hasn't started (3600 by default); with `--window`, `--measure` or `--replay`: quits after `n` game frames (no limit by default with `--window` or `--replay`); with `--record`: records `n` game frames (until you close the window by default) |
| `--validate` | rechecks the game's display lists on every use |
| `--goldens <scenes> <dir>` | headless, on the GPU with no window: writes the frames a scenes file names into `dir`, for the goldens test |
| `--display <width>x<height>` | with `--goldens`: writes the frames as a window of that size in pixels shows them, without opening one, so wider than the console's for a size wider than 4:3, as in `--display 854x480`. Each `--set` with it sets a video setting for the run, as in `--set sonar.video.widescreen=false`; no settings file is read |
| `--measure` | headless, on the GPU with no window: runs to the tick limit, presenting each frame once unless `--present-hz` is given, and prints how long the game, the renderer's preparation and each present took from play's first frame on, and what the presents drew, then a presents line: the presents, those held, the display times passed late, the frames dropped, any present going backward, and the snaps by cause |
| `--present-hz <n>` | with `--measure`: presents each frame at a display's rate of `n` Hz (1 to 1000) instead of once, at each of the display's times in the game frame it is handed over in, blended between frames as the clock says |
| `--replay <recording>` | plays the controllers from a recording, which sonarlib's `sl-replay` (built at `port/build/sonarlib/tools/sl-replay`) builds from a script of presses and moves, such as the town replay test's `port/tests/town_replay.txt`, or `--record` records. It runs past the first frame of play until the recording ends, prints an `ac_port: replay …` line and exits with 0; each line it prints ends with the scene the game is in. Headless it runs as fast as it can. With `--window` you watch it at the game's speed, and it plays exactly as headless: from the same date, with the memory cards the recording started from, or none if it has none, and the system settings the recording was made with (your settings file keeps yours, and a `--set` of a system setting is refused); it then closes the window and prints the window line too. A replay with memory cards also prints an `ac_port: saves …` line, which names the cards' saves as the run left them; its cards are in memory, start from the recording's and are never written, so your own cards are never read or changed. Not with `--goldens`, `--measure` or `--record` |
| `--record <recording>` | with `--window`: records the controllers into a recording for `--replay`, from the start until you close the window, or for `n` game frames with `--ticks`, at the game's speed and your date and system settings, with your memory cards: the game saves to them as it always does, and the recording keeps the cards as they were at its start, for its replays. Then it prints an `ac_port: record …` line and an `ac_port: saves …` line, which a replay of it prints too, the first named `replay`, closes the window and prints the window line. It exits with 0 once every byte is written, and with 1 if writing failed |
| `--date <date>` | with `--record`: starts the console's clock at this local date and time, `YYYY-MM-DDTHH:MM:SS` from 2000 on, such as `2001-12-14T12:00:00`, instead of your computer's, as the save test does |
| `--input-script <script>` | with `--record`: plays a script of devices added, presses, moves, settings changes and the window's close instead of your keyboard and gamepads, such as the record test's `port/tests/title_input.txt` (sonarlib's `docs/platform.md`, Device scripts) |
| `--unpaced` | with `--window` and `--replay` or `--record`: runs the replay or recording as fast as it can instead of at the game's speed, as the tests do |
| `--qrand-seed <hex>` | seeds the game's random numbers with this value instead of the clock, as the goldens test does with Dolphin's; not with a `--replay` of a recording `--record` made, which gives its own |

The build also makes `ac_port_reference`, a test program of the same game that a player never needs. It takes every option above and three more, which `ac_port` refuses: `--diff` draws each frame's 3D twice, once through sonarlib and once through the game's own renderer (emu64), and compares the two, headless, to play's first frame or over a `--replay` to its end; `--diff-from <tick>`, with `--diff`, still compares every frame but counts and prints only those from that tick on; `--reference` draws every frame with emu64 through the GX front end instead of sonarlib's kit, to measure the kit against, with `--measure` or `--goldens`, not with `--window`, `--diff`, `--validate`, `--present-hz` or `--replay`. Without `--diff` or `--reference` it plays as `ac_port` does ([NOTES.md](NOTES.md), The reference's program).

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
| `sonar.video.interpolation` | `true` (the default) blends the game's last two frames at each refresh, so motion is smooth at any refresh rate; `false` shows each frame once, as the game drew it, as the console did, with less work for the computer |
| `sonar.video.widescreen` | `true` (the default) makes the picture as wide as the window, whatever its shape, and follows it as you resize it or go fullscreen; the game's camera shows more of the world at each side, at the height it had, and the world keeps its shape. The trees, villagers, buildings, items and ground the console left out beyond its 4:3 picture are drawn out to the window's edges, and the game plays the same at every shape. `false` keeps the console's 4:3, with bars at the sides of a wider window, drawn as the console drew it. In a wider picture the text, the menus and the title's logo stay in the middle at their 4:3 size, the clock keeps to its bottom corner (`safe_area`), and a fade covers the whole window. For now a scene change's wipe covers the window with its shapes stretched, the title's letters are cut at the 4:3 picture's edges as they fly in, and fish, insects and falling rain, snow, petals and leaves show only inside the 4:3 picture |
| `sonar.video.ui_scale` | 50 to 100: the size of the text, the menus, the clock and the title's logo, in percent of the console's. 100 (the default) is the console's size; below it they shrink, the clock toward its corner, a line of dialogue toward the bottom, the rest toward the middle, and the world stays as it is. The name shown over an item you stand at keeps its size, as it follows the item. It works in a 4:3 window too |
| `sonar.video.safe_area` | how far out in a wider picture the clock stands: `16x9` (the default) at the corner a 16:9 picture has, so on an ultrawide display it doesn't go out to the far edge; `21x9`; `display`, at the window's own corner; or `frame`, where the console's 4:3 picture has it |
| `sonar.video.render_scale` | 0 to 16: how many times the console's 640x480 the game is drawn at. 0 (the default) picks the smallest that fills the window and follows it as its size changes; 1 is the console's own resolution, for the original look or a slower computer |
| `sonar.video.supersampling` | 1 to 4: draws the game at this many times your window's resolution a side and averages it down, which smooths every edge, texture, line of text and menu alike. 1 (the default) is off. It costs the square of the number in the GPU's work and memory, so 2 draws four times the pixels and 4 sixteen times. Above 1 it decides the resolution itself, whatever `render_scale` says, and a very large window gets as much of it as the GPU's largest picture allows |
| `sonar.video.antialiasing` | `off` (the default), `smaa` or `smaa_ultra`: smooths jagged edges after the frame is drawn, for much less work than supersampling. `smaa_ultra` follows each edge further |
| `sonar.video.msaa` | `off` (the default), `2x`, `4x` or `8x`: multisampling, which smooths the edges of shapes as they are drawn, with that many samples a pixel: the outline of a villager, a roof or a tree trunk against what is behind it. It doesn't smooth an edge that is cut inside a texture, nor the texture itself; `supersampling` smooths those too. It costs that many times the picture's memory and a little more of the GPU's work, and it can be used with `antialiasing` and `supersampling`. If your GPU can't do the number you ask for, it uses the highest it can below it |
| `sonar.video.upscale` | how the picture is fitted to the window when the two differ in size: `bilinear` (the default), `point` for hard pixels, `sharp_bilinear` for hard pixels that don't shimmer, `area`, `bicubic`, or `fsr1`, AMD's FSR 1. At the default `render_scale` the game is drawn at least as large as the window, so a filter shows most when you draw smaller than the window: on a slower computer, try `render_scale` 1 with `fsr1` and `sharpen` at `rcas` |
| `sonar.video.sharpen` | `off` (the default) or `rcas`, FSR 1's sharpening, applied after the picture is fitted to the window |
| `sonar.video.sharpness` | 1 to 100, 87 by default: how strongly `rcas` sharpens |
| `sonar.video.specialized_shaders` | `true` (the default) draws each of the game's materials with a shader made for it, which is much less work for the GPU than one shader for them all, most of all in a large window or with `supersampling`. Each is made in the background the first time the game shows its material, and until it is ready the general shader draws it, so nothing waits and the picture is the same. `false` draws everything with the general shader: the same picture for several times the GPU's work, to try if your GPU's driver has trouble with the others |
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

Runs without `--window` have no cards: the game finds both slots empty, except a `--replay` of a recording that `--record` made, which has the cards the recording started from, in memory.

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
