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

When a CV input jack is connected for a given angle, the corresponding knob acts
as an attenuverter of the CV input for that angle.

## Specifications

- Width: 10 HP
- 3 Knobs
  - azimuth (-180 to 180 degrees, default 0 degrees)
  - elevation (-90 to 90 degrees, default 0 degrees)
  - spread (0 to 90 degrees, default 45 degrees)
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

## Panel design

There is a screen at the top of the module that draws the location of the
input mono or stereo source on a sphere:

- The screen takes the width of the module (minus some reasonable margin)
- The sphere is represented using either the Hammer projection, or a pair of
  orthographic projected hemispheres for the front (+x axis) and the rear (-x
  axis) shown side by side. The projection type is chosen via a dedicated option
  in the right click menu (default: orthographic pair).
- (hemi)sphere graticules are drawn (fine stroke width, little contrast color)
  to give the illusion of rounded surface and to provide landmarks
- The position of the mono source is represented by the letter "M". In case of a
  stereo input, the "M" letter is grayed out and the letters "L" and "R" are
  drawn for the left and right actual source positions respectively.
- For the orthographic pair, sources located exactly on the boundary between
  the front and rear hemispheres (e.g., left, right, top, bottom) are shown on
  the front hemisphere, unless the mono source is on the rear hemisphere (e.g.,
  left and right sources for a mono source at the rear with a 90 degrees spread).

Below the screen, the three knobs are vertically aligned. The switch is
positioned right next to the knobs, vertically aligned with the group of knobs.
The knobs and the switch are horizontally spaced evenly, i.e., with the same gap
between them and with the panel borders.

At the bottom of the module, there's two rows of 3 jacks each, evenly spread
across the module width. The 1st row contains the 3 CV inputs and the 2nd row
contains the Left (Mono) and Right inputs and the Ambisonic output. Add a label
just above of each jack.

## Implementation

Look at the SAF example `ambi_enc` (basic Ambisonic encoder). That example
supports an arbitrary number of input sources. For this module it is fine to
implement a common encoder with an arbitrary number of input sources as well,
even tough here it is limited to Mono (1 source) or Stereo (2 sources). The
common, generic encoder could then be reused later for another more advanced
rack module.
