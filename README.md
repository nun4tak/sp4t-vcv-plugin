# Nun4tak-SP4T

VCV Rack plugin providing Ambisonics 3D spatial audio modules.

All ambisonic signals generated or processed by the modules use the ACN channel
ordering, SN3D normalization (ambiX) convention, unless otherwise specified.

## What is ambisonics?

Ambisonics encodes a 3D sound field using spherical harmonics: it is a
multi-channel audio encoding technique where each channel carries a given
(normalised) harmonic function (figure 1 TODO). This encoding is *scene-based*
and lives alongside *channel-based* (stereo, 5.1) and *object-based* formats. It
contrasts most directly with object-based techniques such as Dolby Atmos, where
each object carries its own audio stream plus positional metadata and is placed
in the 3D environment at render time.

Despite being less popular, scene-based ambisonic encoding has several
advantages that open a lot of interesting possibilities for sound design and
audio synthesis:

- It is a relatively simple and non-proprietary technique.
- It is speaker-agnostic. The same encoded signal can be decoded in many
  different ways, e.g., to binaural or to any speaker layout, whether standard
  or arbitrary. Separately, virtual microphones pointing in arbitrary directions
  can be derived from it.
- It allows processing the whole sound field in simple and efficient ways (e.g.,
  rotating or flipping the sound field can be done exactly and
  non-destructively). It is a powerful and advanced panning tool.
- Applications go well beyond panning: for example, the omni vs. directional
  components of the sound field can be easily decoupled and processed
  independently.

It also has a few limitations:

- It is directional / far-field only (some tricks can mimic the perception of
  distance to near sources, but those are not part of ambisonics itself).
- The spherical harmonics are truncated at a given order, with a corresponding
  fixed number of channels — a full-3D order *N* has *(N+1)²* channels
  (1st-order = 4, 2nd order = 9, 3rd order = 16). This sets the finite
  "directional" resolution and sharpness of the encoded sound field.

## Why ambisonics in modular synthesis / VCV Rack?

The point of ambisonics in a modular context goes well beyond immersive-audio
playback. Because each ambisonic channel is just an ordinary audio signal, the
whole sound field becomes something you can patch, modulate, process and route
like anything else on the rack.

A typical flow might be: **map or move** arbitrary sources onto the sphere — or
**directly synthesize** the 3D sound field — then **rotate, flip, decouple and
process** that field, and finally **decode** it to a mono, stereo or binaural
signal. And there is no obligation to represent a realistic scene: the ambisonic
channels can just as well carry different sound *textures* mapped across a
sphere, turning spatial operations into a sound-design tool in their own right.

The usual caveat applies — some operations only stay meaningful when applied
consistently across the whole channel set — but gain, mute, mixing and routing
all behave like normal audio. Although still largely underused this way, modular
synthesis offers exactly the right abstraction and workflow for exploring the
full range of ambisonic possibilities.

VCV Rack's polyphonic cables make it also very convenient to carry ambisonic
signals between modules (up to 3rd order = 16 channels).

## Interpreting the ambisonic channels

A First-Order Ambisonics (FOA) signal has 4 channels, and each is easy to
interpret. This set is the classic *B-format*. The table below follows the ACN
channel-ordering convention.

| Channel (ACN) | Order | Axis | Direction | Plot |
|---|---|---|---|---|
| 0 | 0th | — | isotropic / omnidirectional | *(TODO: same picture as gain/mute modules)* |
| 1 | 1st | Y | left–right | *(TODO)* |
| 2 | 1st | Z | top–bottom | *(TODO)* |
| 3 | 1st | X | front–rear | *(TODO)* |

The first two ACN channels are thus equivalent — up to some gain normalization —
to the familiar "mid-side" encoding of a stereo signal. Ambisonics may indeed be
viewed as a generalisation of the "mid-side" concept to 3D spatial audio!

Subsequent channels (if any) represent Higher-Order Ambisonics (HOA). They are
harder to interpret, since they correspond to more complex harmonic functions,
most of which vary along more than one axis (figure 1).

## Requirements

- [VCV Rack SDK](https://vcvrack.com/Rack#sdk) 2.6.6, unpacked as
  `SDKs/Rack-SDK` (not vendored in this repo)
- CMake 3.19 or newer and a C/C++ toolchain
- Optional: [pixi](https://pixi.sh), which provides CMake and the task shortcuts
  below

Download the SDK for your platform, replacing `mac-arm64` with `mac-x64`,
`lin-x64` or `win-x64` as needed:

```sh
curl -L -o Rack-SDK.zip https://vcvrack.com/downloads/Rack-SDK-2.6.6-mac-arm64.zip
unzip -d SDKs Rack-SDK.zip
```

## Configure, build, install

```sh
pixi run configure   # cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
pixi run build       # cmake --build build --parallel 4
pixi run install     # cmake --install build
pixi run dist        # cmake --build build --target plugin_dist
pixi run clean       # cmake -E rm -rf build
```

`build` and `dist` configure first, and `install` builds first, so
`pixi run install` is enough from a clean checkout. Without pixi, run the CMake
commands shown in the comments directly.

## Module panels

Panel SVGs are edited in `res/staging/`, where text is kept as editable text
objects. Rack does not render SVG text, so convert it to paths before building:

```sh
pixi run export-panels   # res/staging/*.svg -> res/*.svg (requires Inkscape)
```

The exported files in `res/` are generated: edit the staging files instead.
Fonts used by the panels but not installed by default (e.g., M PLUS Rounded 1c, SIL OFL)
are shipped in `res/staging/fonts/`, which the export task makes available to
Inkscape via `res/staging/fonts/fonts.conf`.

## Build output

`build` produces a ready-to-load plugin directory at `build/Nun4tak/`. `install`
copies it into the Rack user plugin folder, which defaults to
`~/Library/Application Support/Rack2/plugins-mac-arm64` on macOS,
`$XDG_DATA_HOME/Rack2/plugins-lin-x64` on Linux and
`%LOCALAPPDATA%/Rack2/plugins-win-x64` on Windows. Restart Rack afterwards.
`dist` writes a distributable `.vcvplugin` archive to `build/dist/`.

If the SDK lives elsewhere, pass `-DRACK_SDK_DIR=<path>` when configuring. To
install to a different folder, pass `-DCMAKE_INSTALL_PREFIX=<path>`. For a debug
build, configure with `-DCMAKE_BUILD_TYPE=Debug`.
