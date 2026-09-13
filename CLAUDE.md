# Nun4tak SP4T VCV Rack plugin

A VCV Rack plugin that provides modules for Ambisonics 3D spatial audio.

## Project configuration and build

All instructions can be found in the README.md file. Prefer running configure
and compile commands via the pixi tool when possible (e.g., when a pixi task
exists). You can still suggests adding pixi tasks for common tasks. 

Module C++ source files are in the `src/` folder and module panel SVG files are
in the `res/` folder.

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

Follow VCV-Rack's standard conventions for voltages, as described in https://vcvrack.com/manual/VoltageStandards.

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
- The name of the module shown on the panel (SVG file) and in any documentation is obtained by converting PascalCase above with spaces, e.g., "SP4T Module Name"

Notes on the general design of the module panels (SVG files):

- Follow the recommandations here: https://vcvrack.com/manual/Panel, with some exceptions:
  - Do not convert any text or font into paths when generating or editing SVG files. Keep them as objects so these can be edited manually if needed
  - Do not use any color gradient nor fancy patterns
- Keep the panel design clear, minimal and elegant. Light background is silver and black background is dark-grey. Use flat and matte design. Use colors inspired by the Ayu theme (https://ayutheme.com/). Avoid using too much colors on the panel.
- At the top of the panel, add the name of the module. Just below the name, add a very brief one-line description (3-4 words maximum) if there's enough room.
- At the bottom of the panel add "\nu.na.tak\" with small sized font.
- For the panel components, reuse (for now) Rack's Component Library graphics
  found in https://github.com/VCVRack/Rack/tree/v2/res/ComponentLibrary (CC
  BY-NC 4.0). Use the "Round*" knobs (not the Befaco, Rogan or Davies ones).
- jack inputs and outputs carrying ambisonic signals (via polyphonic cables)
  must all have the same identifiable contour (border), for example the jack
  plug component may be drawn on top of the ambisonics circular logo
  (https://en.wikipedia.org/wiki/Ambisonics#/media/File:AmbisonicLogo.svg), the
  two being center aligned together but the latter has a slightly bigger size.

## Module Design

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
