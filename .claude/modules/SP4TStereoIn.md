# SP4TStereoIn Module

## Description

A basic Ambisonic encoder for one panned mono or stereo input source.

The module takes an input mono or stereo source and converts it into an
Ambisonic output signal. The source can be located in the 3D space (panner) via
two "azimuth" and "elevation" angles. The source is either mono if only the left
input jack is connected or stereo if both left and right input jacks are
connected. In case of an input stereo source, the "spread" angle controls the
angular distance between the "mono" direction (as determined by the azimuth and
elevation angles) and each of the actual left and right sources.

Each angle also has a CV input with a dedicated attenuverter (trim knob). The
attenuated CV is added to the value of the corresponding angle knob.

## Specifications

- Width: 10 HP
- 3 Knobs
  - azimuth (-180 to 180 degrees, default 0 degrees)
  - elevation (-90 to 90 degrees, default 0 degrees)
  - spread (0 to 90 degrees, default 45 degrees)
- 3 small knobs (trim): CV attenuverters for azimuth, elevation and spread
- 1 switch
  - select ambisonic order 1 (FOA), 2 (HOA-2) or 3 (HOA-3) for the output
- 3 CV jack inputs:
  - azimuth (bipolar)
  - elevation (bipolar)
  - spread (unipolar)
- 2 audio jack inputs:
  - Left (Mono)
  - Right
- 1 Ambisonic jack output (polyphonic) 
- Right-click menu options:
  - 3D view mode (see panel design below)
  - flip azimuth (boolean, default disabled): flips the sign of the azimuth
    angle (knob + CV) so that turning the knob to the right or feeding a
    positive CV moves the source to the right. This feels more natural for
    eurorack users despite it doesn't follow the Ambisonic convention (positive
    azimuth = anticlockwise, i.e., to the left), hence disabled by default.

## Panel design

There is a screen at the top of the module that draws the location of the
input mono or stereo source on a sphere:

- The screen takes the width of the module (minus some reasonable margin)
- The sphere is represented using one of the following projections, chosen via
  the "3D view mode" option in the right click menu (default: orthographic
  pair):
  - a pair of orthographic projected hemispheres for the front (+x axis) and
    the rear (-x axis) shown side by side
  - the Hammer projection
  - a cartesian projection: a 2D rectangular plane that takes the whole screen
    area, with the origin at the center, azimuth on the horizontal (Y) axis
    (-180 to 180 degrees, positive to the left) and elevation on the vertical
    (Z) axis (-90 to 90 degrees, positive to the top). Both axes are drawn,
    with labeled graticules every 45 degrees.
  - a 3D sphere view: 3D view (orthographic projection) of the sphere and its X,
    Y, Z axes (labeled), with every 45 degrees. The sphere boundary (outline) is
    not drawn; the three great circles in the XY, XZ and YZ planes are drawn
    like the other graticules. The axes are drawn as diameters of these great
    circles, each with a small arrow inside the sphere whose tip touches the
    sphere surface at the positive end. Graticules and sources on the hidden
    side of the sphere are faded.
- (hemi)sphere graticules are drawn every 45 degrees in all projections (fine
  stroke width, little contrast color) to give the illusion of rounded surface
  and to provide landmarks
- The position of the mono source is represented by the letter "M". In case of a
  stereo input, the "M" letter is grayed out and the letters "L" and "R" are
  drawn for the left and right actual source positions respectively.
- For the orthographic pair, sources located exactly on the boundary between
  the front and rear hemispheres (e.g., left, right, top, bottom) are shown on
  the front hemisphere, unless the mono source is on the rear hemisphere (e.g.,
  left and right sources for a mono source at the rear with a 90 degrees spread).

Below the screen, there is one row for each angle (azimuth, elevation, spread),
with from left to right: the CV input, the CV attenuverter (trim) and the main
knob (large, i.e., about 117% of the normal knob size; both knobs with arcs).
Each row spans the module width (same left/right margins as the screen), its
components are linked by thin horizontal lines (same stroke as the knob arcs)
and the angle name is centered above the main knob. The rows are stacked and
evenly spaced inside a rounded rectangle area (same width and rounding as the
screen, slightly darker or lighter than the panel background). The screen is
vertically centered between the title and that area.

At the bottom of the module, there's a single row aligned with the screen
margins, evenly spaced: the Left (Mono) and Right inputs, the order switch and
the Ambisonic output. Add a label just above each jack and the switch (the
Ambisonic output is labeled "A-OUT"). The
switch positions are labeled "1st", "2nd" and "3rd" from top to bottom.

## Implementation

Look at the SAF example `ambi_enc` (basic Ambisonic encoder). That example
supports an arbitrary number of input sources. For this module it is fine to
implement a common encoder with an arbitrary number of input sources as well,
even tough here it is limited to Mono (1 source) or Stereo (2 sources). The
common, generic encoder could then be reused later for another more advanced
rack module.
