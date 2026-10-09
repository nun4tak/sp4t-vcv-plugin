# Nun4tak SP4T VCV Rack plugin

A VCV Rack plugin that provides modules for Ambisonic 3D spatial audio.

## Project configuration and build

All instructions can be found in the README.md file. Prefer running configure
and compile commands via the pixi tool when possible (e.g., when a pixi task
exists). You can still suggests adding pixi tasks for common tasks. 

Module C++ source files are in the `src/` folder and module panel + component
SVG files are in the `res/` folder.

## General Design

All the ambisonic signals generated or processed by the modules of this plugin
use the ACN channel ordering, SN3D normalization conventions ("ambiX" format),
unless otherwise specified. See https://ambisonics.ch/learn/ambisonics-formats/
for more details. Ambisonic signals are carried out using VCV-Rack polyphonic
cables with either 4 channels (order 1, FOA), 9 channels (order 2, HOA-2) or 16
channels (order 3, HOA-3). For example in the case of FOA the polyphonic cable
channels correspond to:

- 1st channel: omni-directional signal
- 2nd channel: left-right component (Y axis)
- 3rd channel: top-down component (Z axis)
- 4th channel: front-rear component (X axis)

Follow VCV-Rack's standard conventions for voltages, as described in
https://vcvrack.com/manual/VoltageStandards.

The module audio processing kernels are implemented with the help of the
"Spatial Audio Framework" (SAF):

- source: https://github.com/leomccormack/Spatial_Audio_Framework
- documentaion: https://leomccormack.github.io/Spatial_Audio_Framework 

SAF is added here has a git submodule in the "SDKs" directory. Use SAF when it
is possible instead of re-implementing ambisonic audio processing features
from-scratch. Check the examples provided with SAF. Don't forget that VCV-Rack
audio kernels' process audio sample per sample (no buffer).

Notes on the name of the modules:

- All modules of this plugin have the same "SP4T" prefix.
- The name of the module in the C++ source is like "SP4TModuleName" where
  ModuleName is in PascalCase
- The module slug in `plugin.json` is obtained by converting PascalCase above
  with hyphens, e.g., "SP4T-Module-Name"
- The name of the module shown on the panel (SVG file) and in any documentation
  is obtained by converting PascalCase above with spaces, e.g., "SP4T Module
  Name"

Notes on the general design of the module panels (SVG files):

Use the panel SVG files found in `res/proto` as a reference. They should already
follow the following general guidelines.

- Follow the recommendations here: https://vcvrack.com/manual/Panel, with some exceptions:
  - Do not convert any text or font into paths when generating or editing SVG
    files. Keep them as objects so these can be edited manually if needed
  - Do not use fancy patterns, and keep color gradients to the deliberate
    exceptions below. Only simple linear gradients, as Rack does not render
    radial ones (which is why the spherical harmonic illustrations are flat):
    - the channel cell backgrounds of the Gain, Mute, Split and Merge panels: a
      vertical linear gradient, `gradientUnits="userSpaceOnUse"`, from the panel
      background color at the top of the cell to the cell color at the bottom,
      one gradient per row of the grid. The cell opacity still encodes the
      Ambisonic order (1.0 / 0.8 / 0.5 / 0.25 for order 0 / 1 / 2 / 3)
    - spherical harmonic function illustrations
- Keep the panel design clear, minimal and elegant. Light background is silver
  and black background is dark-grey. Use flat and matte design. Use colors
  inspired by the Ayu theme (https://ayutheme.com/). Avoid using too much colors
  on the panel.
- At the top of the panel, just under the screws, add the name of the module in
  capital letters, with a vertically centered dot surrounded by spaces after
  "SP4T" (e.g., "SP4T · MODULE NAME") and the
  "4" colored in the Ayu theme blue (no subtitle).
- At the bottom of the panel, between the bottom screws, add the
  "\nu.na.tak\" brand text with a bold and bright font, almost as big as the title.
- For the title and "\nu.na.tak\" texts, use a rounded, less common font family
  (e.g., "M PLUS Rounded 1c"). Prefer static (non-variable) font files, which
  render correctly in VCV Rack once converted to paths.
- For small panels less or equal to 6HP: use only two screws (those on the
  left). If necessary, split the prefix "SP4T" with the rest of the title and
  place it next to the top screw ; at the bottom of the panel place the brand
  text next to the screw and, if needed, use the short "\n.tk\" brand text.
- For the control and jack labels, use a common, compact (condensed) but readable
  font family, with a clear and readable font size (not too small) and slightly
  increased letter spacing. Use capital letters.
- For the panel components, reuse (for now) Rack's Component Library graphics
  found in https://github.com/VCVRack/Rack/tree/v2/res/ComponentLibrary (CC
  BY-NC 4.0). Use the "Round*" knobs (not the Befaco, Rogan or Davies ones). You
  can modify it in order to get a more flat and matte design, but the position
  of the knobs or switches should stay clear. Surround knobs closely with a thin
  stroke arc drawn on the panel, similarly to the VCV Fundamental modules, with a
  subtle color (low contrast with the panel background). For jacks
  keep the components as-is, except the ambisonic ones (see below).
- jack inputs and outputs carrying ambisonic signals (via polyphonic cables)
  must all use the `SP4TAmbiPort` component (`res/components/SP4TAmbiPort.svg`
  and `SP4TAmbiPort-dark.svg`): Rack's PJ301M jack with its outer ring in the
  same Ayu theme blue as the "4" of the title (#22A4E6 on light panels,
  #59C2FF on dark ones). Only that ring is recoloured, so the component keeps
  the stock jack size and no contour is drawn on the panel. Their label must be
  "AMB", for both input and output ambisonic jack ports.

## General implementation notes

In the C++ plugin code, the enum items declared in each module (parameters, CVs,
inputs, outputs, etc.) are named after their corresponding components of the
panel SVG files. Use uppercase characters and underscores for the C++ names.
Avoid changing the order of the items when refactoring (this would break
existing VCV Rack patches, which rely on item index), unless explicitly asked.

Avoid adding too many verbose comments directly in the code. Do not comment
something that can be easily understood just by looking at the code. It is OK to
add one-line comments to clearly distinguish the parts of long blocks of code.
If a comment is useful in general (project-scope), it is better to update this
section instead.

## Module Design and implementation

For each module of this plugin, there's a corresponding design document found in
the `.claude/modules` directory. Use that document to build, update and/or
document the corresponding module. Each design document has a title that
corresponds to the module name + " Module" (filename corresponds to the module
name + ".md" file extension) and the following sections:

- description: short (one-line) description followed by a more detailed description
- specifications: module width (number of HPs), parameters (on the module panel
  or accessed via the right-click menu), input/output jacks for either CV, audio
  or ambisonic (polyphonic) signal
- panel design: notes on the panel design, e.g., how jacks and controls are
  arranged, positioned and/or aligned on the panel, the size of the knobs (e.g.,
  big, normal, small trim), screen size (if any)
- implementation: notes on the implementation details

## Continuous integration and distribution

The VCV Library builds open-source plugins itself from a commit hash, with
VCVRack/rack-plugin-toolchain, which runs `make clean`, `make cleandep`,
`make dep` and `make dist` in the plugin directory. The `Makefile` exists only
for that: it translates RACK_DIR, CC, CXX and STRIP into a CMake invocation and
leaves the build itself to CMake, so the sources are described in one place. It
is not used for development; the pixi tasks are.

`.github/workflows/build.yml` builds a `.vcvplugin` bundle per platform, on a
release, on changes to the build configuration, and on demand. Those bundles
are for direct download and for checking that the plugin still builds
everywhere -- what users install through the Plugin Manager is VCV's own build.

Decisions behind both, worth not re-deriving:

- SAF needs a BLAS/LAPACK back end (`SAF_PERFORMANCE_LIB`). Apple Accelerate on
  macOS, OpenBLAS elsewhere. Intel MKL is ruled out: SAF expects a custom-built
  shared library for it, which Windows would not resolve from the plugin
  directory at load time, on top of its proprietary licence and its size.
- OpenBLAS is linked statically, so a bundle needs no library of its own. On
  Linux it is built from source (`NOFORTRAN=1`, which builds LAPACK and LAPACKE
  from the f2c-translated C sources): the distributions' builds are compiled
  with gfortran, and Ubuntu ships no position-independent `libgfortran.a`, so
  linking its runtime into the plugin fails on a TLS relocation in `async.o`.
  LAPACK reaches that runtime through one symbol, `_gfortran_concat_string`,
  which `dgesvd` and friends use. The Makefile's `dep` target builds the same
  thing for the Library's cross builds.
- `SAF_ENABLE_SIMD` is off on Linux and Windows: SAF compiles itself with
  `-march=native` there (`framework/CMakeLists.txt`), which would tie a bundle
  to the machine that built it. On macOS its flags are per-arch baselines.
- Windows uses MSYS2's MINGW64 and not the UCRT64 that MSYS2 now recommends:
  Rack and the plugins VCV builds target msvcrt, and Rack deletes widgets and
  modules the plugin allocated, so a plugin on the other C runtime would free
  its memory on the wrong heap.
- The workflow's `library-build` job cross-compiles to Windows through the
  Makefile with the distribution's mingw-w64, as a cheap check that the
  Library's build path still works; it is not VCV's toolchain. Building that
  (VCVRack/rack-plugin-toolchain, the `v2` branch, not `master`, which is still
  Rack v1) takes hours and needs a MacOSX SDK tarball extracted from Xcode for
  its macOS half, so it is a local rehearsal before a submission, not CI.
- There is no `lin-arm64` Rack SDK, so there is no Linux ARM bundle. The Linux
  job builds on the oldest image GitHub offers, which sets the glibc floor
  (2.35 on ubuntu-22.04, against VCV's own 2.23); an older container would
  lower it if that ever matters.
