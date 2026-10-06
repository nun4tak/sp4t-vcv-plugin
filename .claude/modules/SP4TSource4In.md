# SP4TSource4In Module

## Description

A basic ambisonic encoder for up to four input point sources.

The module takes up to four input point sources and converts them into an
ambisonic output signal. Each source, represented by a column in the panel, can
be located in the 3D space (panner) via their two "azimuth" (AZ) and "elevation"
(ELEV) angles. Each angle has a dedicated knob and CV input.

An omni-directional source can also be added via the OMNI jack input. 

## Specifications

- Width: 10 HP
- 1 Ambisonic jack output (polyphonic) 
- 2x4 Knobs
  - 4 azimuth (-180 to 180 degrees, default 0 degrees)
  - 4 elevation (-90 to 90 degrees, default 0 degrees)
- 2x4 CV jack inputs:
  - 4 azimuth (bipolar)
  - 4 elevation (bipolar)
- 4 source jack inputs
- 1 omni-directional jack input
- 1 switch
  - select ambisonic order 1 (FOA), 2 (HOA-2) or 3 (HOA-3) for the output
- Right-click menu options:
  - 3D view mode (see panel design below)
  - Flip azimuth sign

## Panel design

The design of the panel is done in the SP4TSource4 SVG files.

## Implementation

Reuse the ambisonic encoder used for the Stereo In module. The principle is
exactly the same here, but for 4 sources. Do not process a source if its
corresponding jack input port has no cable plugged in.

Mix the encoded sources with the omni input, if a cable is plugged in.

The flip azimuth sign option (right-click menu item here) behaves exactly like
the equivalent flip switch in the Stereo In module.

In the screen display, label each source by their number. For all sources, use
the same orange color than the one used for the M (mono) label in the Stereo In
module.
