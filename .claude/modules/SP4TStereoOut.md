# SP4TStereoOut Module

## Description

A basic stereo speakers Ambisonic decoder.

The module takes an ambisonic input signal (polyphonic cable) and decodes it
into two left/right stereo (virtual) speaker channels. The speakers are
positioned at the front hemisphere in the X-Y axis plan (no elevation), each
with an angle between the speaker and the X axis given by the "spread"
parameter. The latter ranges from 0 degrees ("mono" setting where left/right
outputs are equivalent) to 90 degrees (hard left/right panning).

"Spread" also has a CV input with a dedicated attenuator (trim knob).

This decoder is CPU-efficient but only provides basic spatial decoding. It even
has a "crude" mode, almost CPU-free, that uses only the two first input ACN
ambisonic channels W,Y and ignores all the other channels (i.e., this is
"mid-side" decoding). When listening with headphones, use "Ears Out" (binaural
decoder, more CPU-expensive).

## Specifications

- Width: 4 HP
- 1 Knob
  - spread (0 to 90 degrees, default 45 degrees)
- 1 small knob (trim): CV attenuator for spread
- 1 CV jack inputs:
  - spread (unipolar)
- 1 AMB Ambisonic jack input (polyphonic) 
- 2 audio jack outputs:
  - L (left)
  - R (right)
- 2 LEDs in the speaker drawings
- 1 right-click menu option:
  - boolean option to enable/disable the "crude" mode (default: disabled).

## Panel design

The design of the panel is done in the SP4TStereoOut SVG files.

## Implementation

When the crude mode is enabled, the L,R stereo outputs are computed trivially
from the two first input ambisonic channels W,Y and the spread angle using these
formulas:

L = 0.5 * (W + sin(spread) * Y)
R = 0.5 * (W − sin(spread) * Y)

When the crude mode is disabled, use a 3D-aware decoding technique that works
with all input ambisonic channels (FOA or HOA). Look at the SAF example
`ambi_dec`, although ideally here we want a simpler version of the algorithm,
purely time-dependent (no frequency-dependent), CPU-efficient, with reasonable
accuracy for the simple virtual speaker settings here. One matrix-product with
coefficients pre-computed for the current value set for "spread" should ideally
be enough.

This is implemented by the reusable `sp4t::AmbiStereoDecoder`
(`src/AmbiStereoDecoder.hpp`), which decodes one sample with a 2 x (order+1)^2
matrix product. The matrix is a sampling Ambisonic decoder (SAD) with max-rE
weighting (SAF's `getLoudspeakerDecoderMtx()` for `LOUDSPEAKER_DECODER_SAD`),
i.e., one hyper-cardioid beam per virtual speaker, recomputed without
allocating whenever the spread, the order or the mode changes. Its scaling
differs from SAF's: the matrix is normalized so that a source at a speaker
direction decodes at unity gain into that speaker, which keeps the output level
bounded whatever the order. Since the beams get sharper with the order, a source
between the two speakers is quieter at higher orders; this is preferred over a
normalization that would keep the level constant across orders at the price of
decoding some sources above the input amplitude. SAF's own scaling targets a
real speaker array fed at line level rather than Rack's +/- 5 V convention: it
leaves a gain of sqrt(4*pi)/2 on the W channel alone, and the on-axis gain of a
beam grows with the order (4.83, 9.42 and 15.54 for order 1, 2 and 3), so the
matrix here is SAF's divided by that on-axis gain, a constant per order that
does not depend on the spread.

Crude mode uses the same matrix product, restricted to the W and Y channels. In
both modes the gains are linearly interpolated over 32 samples, which avoids
zipper noise when the spread is modulated.

The Ambisonic order of the input signal is given by the number of channels of
the polyphonic cable (4, 9 or 16).

The two LEDs should emit blue light (same color than for AMB jack ports and SP4T
titles, from the ayu theme) for the audio-rate L,R output signals. Like VCV Viz
module (https://vcvrack.com/Free#Viz) emits yellow light for audio-rate signals.
