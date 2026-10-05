# SP4TFlip Module

## Description

Ambisonic sound field flip.

Flips an input ambisonic encoded sound field across the X (front-rear), Y
(left-right) and/or Z (top-down) symmetry axes.

The flipped sound field can be blended with the original one using the dry/wet
control. Two crossfading curves can be selected: equal-power or through-zero.

## Specifications

- Width: 6 HP
- 1 AMB Ambisonic jack input (polyphonic)
- 1 AMB Ambisonic jack output (polyphonic)
- 3 buttons: flip along each of the X, Y, Z axes
  - 3 CV inputs, one for each axis, accepting a gate signal (bottom switch in
    MOM position) or a trigger signal (bottom switch in LATCH position)
- 2 switches:
  - crossfader curve (top: equal-power, bottom: through-zero)
  - MOM vs. LATCH mode, default: LATCH
- 1 Dry/Wet Knob
  - 1 CV input + 1 attenuverter 

## Panel design

The design of the panel is done in the SP4TFlip SVG files.

## Implementation

Flip a sound field across the X,Y or Z axis consists in multiplying the input
ambisonic channels with the following -1/+1 factors (look at the corresponding
axis column).

| ACN | X (F/R) | Y (L/R) | Z (T/B) |
|-----|:---------:|:---------:|:---------:|
| 0   |    +1     |    +1     |    +1     |
| 1   |    +1     |    -1     |    +1     |
| 2   |    +1     |    +1     |    -1     |
| 3   |    -1     |    +1     |    +1     |
| 4   |    -1     |    -1     |    +1     |
| 5   |    +1     |    -1     |    -1     |
| 6   |    +1     |    +1     |    +1     |
| 7   |    -1     |    +1     |    -1     |
| 8   |    +1     |    +1     |    +1     |
| 9   |    +1     |    -1     |    +1     |
| 10  |    -1     |    -1     |    -1     |
| 11  |    +1     |    -1     |    +1     |
| 12  |    +1     |    +1     |    -1     |
| 13  |    -1     |    +1     |    +1     |
| 14  |    +1     |    +1     |    -1     |
| 15  |    -1     |    +1     |    +1     |

Apply a ramp when changing the flip for an axis in order to avoid clicks/noises
(like it is done in the Mute module).

Flip buttons and their respective CV inputs + the MOM/LATCH switch behave like
for the Mute module.

The Ambisonic order of the input signal is given by the number of channels of
the polyphonic cable (4, 9 or 16).

The flipped ambisonic channels are mixed with the input ambisonic channels
according to the cross-fading curve. In both curves the dry/wet control is fully
dry at one end and fully flipped at the other, both at unity gain; they differ in
between:

- equal-power: the two fields overlap over the whole travel, their gains
  crossing at -3 dB (cosine / sine), which keeps the power constant for
  uncorrelated signals;
- through-zero: each gain falls linearly to zero at the mid position, where the
  output is silent, so the two fields are never heard over each other.
