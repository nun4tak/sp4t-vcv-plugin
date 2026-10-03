# SP4TSplit Module

## Description

Ambisonic channel split.

Splits an input ambisonic signal (multi-channel, polyphonic cable) into multiple
single-channels.

It is exactly what does VCV Rack's
[Split](https://library.vcvrack.com/Fundamental/Split) module, here with a panel
that is specific to ambisonic signals. ACN ordered Ambisonic channels are
arranged on the panel as a grid, from top-left to bottom-right.

## Specifications

- Width: 6 HP
- 1 AMB Ambisonic jack input (polyphonic)
- 16 CV jack outputs (single-channel splitted ambisonics)
- 16 lights

## Panel design

The design of the panel is done in the SP4TSplit SVG files.

## Implementation

The implementation is straightforward, it is simply channel routing.

The lights have the same settings than in the Gain module.

The channels the input does not carry are left at 0 V, as VCV Rack's Split does.
Routing keeps no state, so with nothing plugged into the AMB input the outputs
and the lights are zeroed once and the per-sample work stops.

The LEDs sit in the top-left corner of each cell, in the gap between the left
edge of the cell and its spherical harmonic drawing. Their glow is shortened as
in the Gain module, the cells being tight. 
