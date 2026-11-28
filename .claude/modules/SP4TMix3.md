# SP4TMix3 Module

## Description

Ambisonic mixer for up to three input signals.

The module sums up to three ambisonic input signals into one ambisonic output
signal. Each input, represented by a column on the panel, has its own level
knob and level CV input with a trim (CV attenuator).

Ambisonic signals are linear, so mixing them is a channel-wise weighted sum.
Inputs of different orders can be mixed together: the output has the highest
order of the patched inputs, and the channels a lower-order input does not carry
count as zero.

A small screen shows the level of each ambisonic channel of each input, as a
visual hint (not a precise monitoring meter).

## Specifications

- Width: 8 HP
- 3 Knobs
  - input level: 0 to 1x (default 1x, i.e., unity gain). No boost: the
    maximum is unity, which keeps modulation simple (the CV can only bring the
    level down from the knob position) and the sum of the inputs within range
- 3 small knobs (trim): level CV attenuators, 0 to 1 (default 1, fully
  clockwise)
- 3 CV jack inputs (monophonic):
  - level CV control (unipolar, 0 to 10 V), normalled to 10 V when no cable is
    plugged in
- 3 AMB Ambisonic jack inputs (polyphonic)
- 1 AMB Ambisonic jack output (polyphonic)
- 1 display
- Right-click menu options:
  - Display mode: "Group by signal" (default) or "Group by channel"

## Panel design

The prototype panel is `res/proto/SP4TMix3Test01.svg` (light theme). The
staging and production panels are still to be done.

- The layout follows the Rotate module: same screen (size and position), and
  three columns at the x positions of the Rotate large knobs, one column per
  input (1, 2, 3, from left to right).
- Each column, from top to bottom: the input number label (same font size as
  the source number labels of Source4 In, same color as all the other labels),
  the level knob (large), the trim (small knob), the level CV jack input, then
  the AMB jack input with its label.
- The knob, trim and CV jack of a column are linked by discontinuous vertical
  lines, drawn only in the gaps between the knob arcs and the jack, like the
  SPREAD control of the Stereo Out module. The vertical spacing between these
  three components is the same as in Stereo Out.
- The AMB jack output is centered at the bottom of the panel, on a darker
  background block like the output of Rotate.
- The panel uses no slot color: the three input colors only appear on the
  screen.

### Display

The screen shows one vertical bar per ambisonic channel and per input, in the
color of the input (Ayu dark):

- input 1: orange `#FFB454` (as the sources of Source4 In)
- input 2: purple `#D4BFFF` (as the channel directions of Channels Out)
- input 3: green `#AAD94C`

Bars use a dB scale from -48 to 0 dB, with faint grid lines every 12 dB (the
shared `SCREEN_GRID` color) and no dB labels. The channels an input does not
carry, or all the channels of an unpatched input, are drawn as short flat dashes
(`UNUSED_COLOR` of Channels Out), so that the order of each input can be read
at a glance. Ticks separate the orders (ACN channels 0 | 1-3 | 4-8 | 9-15), and
the order numbers 0 to 3 are drawn below them.

Two display modes, selected in the right-click menu:

- Group by signal (default): the three inputs are stacked as three rows of 16
  bars. Each row is introduced by its input number in the input color, with the
  same font and size as the source labels of Source4 In (`SOURCE_FONT`,
  `LABEL_SIZE`). The order numbers use the font and size of the tick labels of
  the cartesian view (`drawGridLabel`). No output meter in this mode: a fourth
  row would look like a fourth input.
- Group by channel: 16 groups (ACN channels 0 to 15) of three thin bars (inputs
  1 to 3). The level of the output is drawn on each group as a thin horizontal
  tick (`SCREEN_TEXT` color), not as a fourth bar.

## Implementation

The output signal is, for each channel `c` up to the output channel count:

    out[c] = sum over patched inputs i of gain_i * in_i[c]

with `in_i[c] = 0` when input `i` carries fewer than `c + 1` channels, and

    gain_i = level_i * clamp(trim_i * cv_i / 10 V, 0, 1)

where `cv_i` is the first channel of the level CV input (10 V when no cable is
plugged in). The level is a scalar applied equally to all the channels of an
input, so it does not change the spatial image. A negative gain, which would
flip the polarity of a whole sound field, is not possible.

The output channel count is the highest channel count among the patched inputs,
updated as cables are plugged in or out. Unpatched inputs are not processed. The
module follows the idle behavior of the other modules of the plugin when no
input is patched.

No SAF function is needed: there is no ambisonic transform, only a sum. All
inputs are expected in the plugin's ambiX convention (ACN / SN3D), so no
conversion is done.

The gains are smoothed (e.g., a one-pole filter or a short linear ramp) to
avoid zipper noise when the knobs or CVs move quickly.

### Metering

Metering must stay cheap: it is a visual hint, not a monitoring tool.

- In `process()`, accumulate the square of each active channel after the gain
  (what actually enters the sum), and of the output channels in "Group by
  channel" mode only: at most 48 (+16) multiply-adds per sample.
- Every 512 samples, turn each sum into an RMS value, smooth it with a one-pole
  filter (about 300 ms, VU-like) and store it in a float array read by the
  widget. No peak hold.
- The dB conversion (0 dB for 5 V, Rack's audio level) and drawing are done in
  the widget only, at most 64 `log10` per frame and none in the audio path.
- No metering for unpatched inputs.
