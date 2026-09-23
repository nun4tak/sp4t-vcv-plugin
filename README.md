# Nun4tak SP4T

A VCV Rack plugin providing
[ambisonic](https://en.wikipedia.org/wiki/Ambisonics) 3D spatial audio modules.

All ambisonic signals generated or processed by the modules use the ACN channel
ordering, SN3D normalization (ambiX) convention, unless otherwise specified.

## What is ambisonics?

Ambisonics encodes a full 3D sound field using spherical harmonic decomposition:
it is a multi-channel audio encoding technique where each channel carries a
given harmonic function (figure 1 TODO) sampled at the audio rate. This
*scene-based* encoding contrasts with *object-based* techniques such as Dolby
Atmos, where each object carries its own audio stream plus positional metadata.

Despite being less popular, scene-based ambisonic encoding has several
advantages that offer exciting perspectives for sound design and audio
synthesis:

- It is a relatively simple and non-proprietary technique.
- It is speaker-agnostic: the same encoded signal can be decoded in many
  different ways, e.g., to binaural or to any speaker layout, whether standard
  or arbitrary.
- The whole sound field can be processed in simple and efficient ways, like
  exact / lossless field rotation or flipping. It is an advanced panning tool.
- Applications go well beyond panning: for example, the omni vs. directional
  components of the sound field can be easily decoupled and processed
  independently.

It also has a few limitations:

- It is directional / far-field only (some tricks are used to mimic the
  perception of distance to near sources, but those are not part of ambisonics
  itself).
- The spherical harmonic decomposition is truncated at a given order, which sets
  the finite spatial resolution and sharpness of the encoded sound field. Lower
  orders are subject to spatial blur.

To each order *N* corresponds of fixed number *(N+1)²** of ambisonic audio
channels (1st-order = 4, 2nd order = 9, 3rd order = 16).

## Why ambisonics in modular synthesis / VCV Rack?

The point of ambisonics in a modular context goes well beyond immersive-audio
playback. Because each ambisonic channel is just an ordinary audio signal, the
whole sound field becomes something that can be patched, modulated, processed
and routed like anything else on the rack (note: ambisonic channels do not carry
"speaker" signals, and some operations stay "spatially" meaningful only when
they are applied consistently across the whole channel set).

There is no obligation to represent a realistic scene: the ambisonic
channels can just as well carry different sound *textures* mapped across a
sphere, turning spatial operations into a powerful sound-design tool.

Modular synthesis has the right workflow for exploring the full range of
ambisonic possibilities. VCV Rack's polyphonic cables make it also very
convenient to carry ambisonic signals between modules (up to 3rd order = 16
channels).

This VCV rack plugin aims at providing a comprehensive set of modules for
typical ambisonic operations such as **encoding** one or more sources into a 3D
sound field - or directly **synthesize** the sound field, **rotate, flip,
decouple and process** that field, and finally **decode** it to a mono, stereo
or binaural signal or to an array of speakers. In the spirit of modular
synthesis, almost anything can be controlled and CV modulated!

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

Basically, channel 0 corresponds to the sound that would be picked by a an
omnidirectional (virtual) microphone while channels 1,2,3 relate to what would
be picked by figure-8 microphones along the X,Y,Z axes.

The first two ACN channels are thus equivalent — up to some gain normalization —
to the familiar "mid-side" representation of a stereo signal. Ambisonics may
indeed be viewed as a generalisation of the "mid-side" concept to 3D spatial
audio!

Subsequent channels, if any, represent Higher-Order Ambisonics (HOA). They are
harder to interpret, since they correspond to more complex harmonics, most of
which contain information across more than one axis (figure 1).

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
