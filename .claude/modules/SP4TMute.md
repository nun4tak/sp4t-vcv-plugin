# SP4TMute Module

## Description

Ambisonic mutes.

This basic module allows to independently mute each channel of an input
ambisonic signal and route the result all through the ambisonic output. This is
mostly for convenience and for experimental purpose.

ACN ordered Ambisonic channels are arranged on the panel as a grid, from
top-left to bottom-right. Each channel has a button and CV input (trigger/gate)
to set the mute state, and also has a LED light that is sensitive to its
audio-rate output signal. A switch at the bottom is used to set the behavior of
the buttons and CV inputs (momentary/gate vs. latch/trigger).

## Specifications

- Width: 10 HP
- 16 buttons
  - channel muted when enabled or toggled (depending on the MOM vs. LATCH switch).
- 16 CV jack inputs:
  - mute CV control, accepting a gate signal (switch in MOM position) or a
    trigger signal (switch in LATCH position).
- 16 LEDs
- 1 switch (MOM vs. LATCH mode, default: LATCH)
- 1 AMB Ambisonic jack input (polyphonic)
- 1 AMB Ambisonic jack output (polyphonic)

## Panel design

The design of the panel is done in the SP4TMute SVG files.

## Implementation

The implementation is straightforward: mute all channels that have their mute
state enabled / togggled.

The mute state of a channel is the value of its button param, so it is saved
with the patch and the lit cap of the button always shows it. The
`SP4TModeButton` component (`src/components.hpp`) is the latching button of the
panel, which the MOM position of the switch turns into a momentary one. The CV
input sets that same state, on the edges of its signal, exactly as a click on
the button does: in the MOM position the state follows the gate (muted while
high), in the LATCH position each trigger toggles it.

Channels are faded in and out over 5 ms instead of being cut abruptly, which
avoids clicks (a click on the omni channel of an ambisonic signal is as audible
as on a mono signal).

Only the multiplication of the channels carried by the input signal is done at
audio rate. The buttons and the mute CV inputs are read once every 16 samples,
which is well under the 1 ms of the shortest trigger of the voltage standard,
and the fades are advanced only while a channel is actually fading. Reading the
16 buttons and CV inputs at audio rate made the module cost about five times
more than it needs to.

For the lights: use same settings than the lights of the Gain module.
