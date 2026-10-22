# SP4TSource4In Module

## Description

A basic ambisonic encoder for up to four input point sources, or up to 64 of
them with polyphonic cables.

The module takes up to four input point sources and converts them into an
ambisonic output signal. Each source, represented by a column in the panel, can
be located in the 3D space (panner) via their two "azimuth" (AZ) and "elevation"
(ELEV) angles. Each angle has a dedicated knob and CV input.

A source input also accepts a polyphonic cable, and then encodes each of its
channels as a point source of its own: up to 16 per column, 64 in total. The AZ
and ELEV CV inputs are polyphonic as well and are read channel per channel, so
a polyphonic CV spreads the channels of a column over as many directions, while
a monophonic one (or the knob alone) sends them all to the same direction.

An omni-directional source can also be added via the OMNI jack input. 

## Specifications

- Width: 10 HP
- 1 Ambisonic jack output (polyphonic) 
- 2x4 Knobs
  - 4 azimuth (-180 to 180 degrees, default 0 degrees)
  - 4 elevation (-90 to 90 degrees, default 0 degrees)
- 2x4 CV jack inputs (monophonic or polyphonic):
  - 4 azimuth (bipolar)
  - 4 elevation (bipolar)
  - each row is normalled from left to right (sources 1 to 4): a CV input with
    no cable plugged in takes the signal of the nearest patched input of its row
    on its left, and no CV at all when there is none. Triangles drawn between
    the jacks on the panel show it.
- 4 source jack inputs (monophonic or polyphonic, one point source per channel)
- 1 omni-directional jack input (its channels are summed)
- 1 switch
  - select ambisonic order 1 (FOA), 2 (HOA-2) or 3 (HOA-3) for the output
- Right-click menu options:
  - 3D view mode (see panel design below)
  - Flip azimuth sign

## Panel design

The design of the panel is done in the SP4TSource4 SVG files.

## Implementation

Reuse the ambisonic encoder used for the Stereo In module. The principle is
exactly the same here, but for up to 4x16 sources: one per channel of each
source input, packed into the encoder's slots in input then channel order. Do
not process a source input that has no cable plugged in.

Each point source takes the AZ and ELEV knobs of its column, plus the channel
of its own index in the CV signals in force for that column (which, following
Rack's convention for polyphonic ports, is channel 1 of a monophonic CV, and
0 V past the last channel of a polyphonic CV with fewer channels than the
source).

Mix the encoded sources with the omni input, if a cable is plugged in.

The flip azimuth sign option (right-click menu item here) behaves exactly like
the equivalent flip switch in the Stereo In module.

In the screen display, show only the sources being encoded: a source input with
nothing plugged in has nothing to show, and neither has the module in the
browser. Label the sources with absolute numbers, from 1 in the order they are
encoded in: input 1 to input 4, and within one input the channels of its cable.
A number belongs to no fixed input or channel then -- patching a cable, or one
with a different number of channels, renumbers the sources to its right -- and
that is fine. For all sources, use the same orange color than the one used for
the M (mono) label in the Stereo In module.
