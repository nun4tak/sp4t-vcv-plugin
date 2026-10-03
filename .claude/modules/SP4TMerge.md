# SP4TMerge Module

## Description

Ambisonic channel merge.

Combine multiple individual ambisonic channels into a multiple-channel ambisonic
output signal.

It is exactly what does VCV Rack's
[Merge](https://library.vcvrack.com/Fundamental/Merge) module, here with a panel
that is specific to ambisonic signals. ACN ordered Ambisonic channels are
arranged on the panel as a grid, from top-left to bottom-right.

Select the order 1,2,3 of the ambisonic output signal using the switch. Any
missing input cable is normalized to 0V. Cables that are plugged in input ports
beyond the selected order (>4 for order 1, >9 for order 2) are simply ignored.

## Specifications

- Width: 6 HP
- 16 CV jack inputs (ambisonic individual channels)
- 1 switch
  - select ambisonic order 1 (FOA), 2 (HOA-2) or 3 (HOA-3) for the output
- 1 AMB Ambisonic jack output (polyphonic)

## Panel design

The design of the panel is done in the SP4TMerge SVG files.

## Implementation

The implementation is straightforward, it is simply channel routing.

The switch selects the order with 1 at the top, matching the panel labels, so
the param value 0, 1, 2 maps to order 1, 2, 3 and the output carries 4, 9 or 16
channels. It defaults to order 1, the top position the switch graphic draws at
rest. `Port::setChannels()` clears the channels above the selected order, and an
unplugged input reads as 0 V, so the missing channels come out silent on their
own; the inputs beyond the selected order are simply left unread. Routing keeps
no state, so with nothing plugged into the AMB output the per-sample work stops.

The switch is a `<use>` of the `SP4TSwitchThree` symbol in the proto, like every
other component, so `make_staging_panels.py` generates its placeholder and the
panel stays the single source of its position. It was first drawn as a group of
plain rects instead, which the script skips silently, since it only collects
`<use>` elements: worth keeping in mind when a control goes missing from the
generated placeholders.
