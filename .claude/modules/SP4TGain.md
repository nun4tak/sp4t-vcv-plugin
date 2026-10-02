# SP4TGain Module

## Description

Ambisonic gains.

This basic module allows changing or modulating the gain independently for each
channel of an input ambisonic signal and route the result all through the
ambisonic output. This is mostly for convenience and for experimental purpose.

ACN ordered Ambisonic channels are arranged on the panel as a grid, from
top-left to bottom-right. Each channel has a knob and CV input to control the
amount of gain, and also has a LED light that is sensitive to its audio-rate
output signal.

## Specifications

- Width: 10 HP
- 16 Knobs
  - gain attenuator: at leftmost position value is zero and at rightmost position
    value is 2x
- 16 CV jack inputs:
  - gain cv control, normalized to 5 volts so that when the attenuator is at the
    mid position the resulting gain is 1x
- 16 LEDs
- 1 AMB Ambisonic jack input (polyphonic)
- 1 AMB Ambisonic jack output (polyphonic)

## Panel design

The design of the panel is done in the SP4TGain SVG files.

## Implementation

The implementation is straightforward: multiply each channel of the input
ambisonic signal by its corresponding CV input, attenuated by the channel gain
control (knob). Divide the result from the multiplication by 5 in order to keep
the output voltage within acceptable limits. This way, when no jack is
connected to the CV input (normalized to 5 volts) and the attenuator is at the
mid position, the output produces a unity gain.

The LEDs should emit blue light (same color than for AMB jack ports and SP4T
titles, from the ayu theme) for the audio-rate ambisonic channel output signals.
Use same settings than the LEDs of the Stereo Out module (reuse the same C++
component if possible, rename the component if relevant).

This is the `SP4TSignalLight` component (`src/components.hpp`), named after what
it shows rather than after the speaker cones of the Stereo Out panel where it
first appeared. The brightness follows the peak of the channel over the last 256
samples, full scale at 5 V with a cube-root response. The LEDs of the channels
that the input signal does not carry are off. Here the glow of the component is
kept small (`haloSpread` of 1.5 px beyond the rim of the LED, instead of
2.3 mm), as the LEDs sit close to the knobs and to their arcs.
