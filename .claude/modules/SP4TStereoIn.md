# SP4TStereoIn Module

## Description

A basic Ambisonic encoder for mono or stereo input signals.

The module takes an input mono or stereo source and converts it into an
Ambisonic signal. The source can be located in the 3D space (panner) via the
"azimuth" and "elevation" angles. The source is either mono if only the left
input jack is connected or stereo if both left and right input jacks are
connected. For an input stereo signal, the "spread" angle controls the angular
distance between the "mono" direction (as determined by the azimuth and
elevation angles) and each of the left and right actual sources.

## Specifications

- Width: 6 HP
- 3 Knobs
  - azimuth (-180 to 180 degrees, default 0 degrees)
  - elevation (-180 to 180 degrees, default 0 degrees)
  - spread (0 to 90 degrees, default 45 degrees)
- 1 switch
  - select ambisonic order 1 (FOA), 2 (HOA-2) or 3 (HOA-3) for the output
- 3 CV jack inputs:
  - azimuth (bipolar)
  - elevation (bipolar)
  - spread (bipolar)
- 2 audio jack inputs:
  - Left (Mono)
  - Right
- 1 Ambisonic jack output (polyphonic) 

## Panel design

A small screen at the top of the module that draws the location of the input mono or stereo source on a sphere:

- The screen takes the width of the module (minus some reasonable margin)
- The sphere is represented using either the Hammer projection, or a pair of orthographic projected hemispheres for the front (+x axis) and the rear (-x axis) shown side by side. The projection type is chosen via the right click menu (default: orthographic pair).
- subtle graticules are added to give the illusion of rounded surface and to provide landmarks
- The source position is represented by a small filled circle. In case of a stereo input the "mono" position is grayed out and two filled circles are added for the left and right actual source positions.

Below the screen, the three knobs are vertically aligned. The switch is right next, vertically aligned with the group of knobs.

At the bottom of the module, two rows of 3 jacks. The 1st row contains the 3 CV
inputs and the 2nd row contains the Left (Mono) and Right inputs and the Ambisonic output. 

## Implementation

Look at the SAF example `ambi_enc` (basic Ambisonic encoder). That example works
with an arbitrary number of input sources. For this module it is fine to
implement a common encoder with an arbitrary number of input sources, even tough
here it is limited to Mono (1 source) or Stereo (2 sources). The common, generic
encoder could then be reused later for another rack module.
