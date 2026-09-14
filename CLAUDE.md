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

- Follow the recommendations here: https://vcvrack.com/manual/Panel, with some exceptions:
  - Do not convert any text or font into paths when generating or editing SVG
    files. Keep them as objects so these can be edited manually if needed
  - Do not use any color gradient nor fancy patterns
- Keep the panel design clear, minimal and elegant. Light background is silver
  and black background is dark-grey. Use flat and matte design. Use colors
  inspired by the Ayu theme (https://ayutheme.com/). Avoid using too much colors
  on the panel.
- At the top of the panel, just under the screws, add the name of the module in
  capital letters, with a vertically centered dot surrounded by spaces after
  "SP4T" (e.g., "SP4T · MODULE NAME") and the
  "4" colored in the Ayu theme blue (no subtitle).
- At the bottom of the panel, between the bottom screws, add
  "\nu.na.tak\" with a bold and bright font, almost as big as the title.
- For the title and "\nu.na.tak\" texts, use a rounded, less common font family
  (e.g., "M PLUS Rounded 1c"). Prefer static (non-variable) font files, which
  render correctly in VCV Rack once converted to paths.
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
  keep the components as-is.
- jack inputs and outputs carrying ambisonic signals (via polyphonic cables)
  must all have the same identifiable contour that is distinct from the other
  jacks (CV or audio): a simple thick stroke circle around the jack, in the
  same Ayu theme blue as the "4" of the title. Their labels must also start with
  the "A-" prefix (e.g., "A-IN", "A-OUT").

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
