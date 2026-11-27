# SP4TUhjOut Module

## Description

A two-channel UHJ stereo encoder for Ambisonic signals.

The module takes an ambisonic input signal (polyphonic cable) and turns it into
the two L/R channels of the standard two-channel UHJ format (Gerzon, 1985),
played back directly on a pair of stereo speakers or headphones.

Strictly speaking, going from B-format to UHJ is an *encoding*, but here it is
only used as an Ambisonic "decoder" for direct stereo playback, hence the "Out"
name like the other modules that end an Ambisonic chain (Stereo Out, Ears Out).
There is no plan for a UHJ decoder module.

Compared to Stereo Out, which decodes to two virtual speakers in front of the
listener, UHJ keeps information about the whole horizontal sound field in the
phase relationship between the two channels:

- played as plain stereo, front and side sources are sharply imaged and the
  stage often sounds wider than the speakers; rear sources are folded into the
  front stage with a less focused, "recessed" quality.
- summing L and R gives a clean mono signal (no comb filtering). The mono sum
  is a slightly forward-oriented pickup, so a rear source is about 5 dB quieter
  than the same source in front.

Only the first-order horizontal components (W, X, Y) are used: height (Z) and
all higher-order components are discarded. Like for Ears Out, there's no
orientation control built in this module: the front is the +X axis. Use the
Rotate module before this one.

## Specifications

- Width: 4 HP
- No knob, no CV input
- 1 AMB Ambisonic jack input (polyphonic)
- 2 audio jack outputs:
  - L (left)
  - R (right)
- 2 LEDs in the screen drawing
- No right-click menu option

## Panel design

The design of the panel is done in the SP4TUhjOut SVG files. The layout follows
Ears Out (no controls): screen at the top, AMB input and L/R outputs at the
same positions as Stereo Out and Ears Out.

The panel title is "SP4T · UHJ OUT" (the acronym stays in capitals; in C++ the
module name is still `SP4TUhjOut` and the slug `SP4T-Uhj-Out`, following the
naming rules).

Screen drawing (inspired by the interlocking circles of the Ambisonic logo,
without reproducing it): two overlapping rings stacked vertically.

- the upper ring (front stage) is drawn with a solid line. The two LEDs sit on
  its upper arc, at +/-40 degrees from the vertical axis, like a pair of stereo
  speakers. The ring is broken around each LED (a small gap on each side) so
  that the LEDs stand out from the outline.
- the lower ring (rear) is drawn with a dashed, faded line: the rear of the
  sound field is folded into the front stage.
- no listener, no other element.

Reference geometry, in a 120 x 170 unit box to be scaled to the screen size of
the other panels (colors to be taken from the existing panels):

```svg
<rect x="0" y="0" width="120" height="170" rx="4" fill="#0f1215"/>
<g fill="none" stroke="#cfd3d8" stroke-width="2" stroke-linecap="round">
  <!-- rear ring (faded, dashed) -->
  <circle cx="60" cy="113" r="26" stroke-dasharray="3 5" opacity="0.4"/>
  <!-- front ring, broken around each LED -->
  <path d="M81.70 62.69 A26 26 0 1 1 38.30 62.69"/>
  <path d="M49.68 53.14 A26 26 0 0 1 70.32 53.14"/>
</g>
<!-- LED positions (L, R), placed in C++ as SP4TSignalLight widgets -->
<circle cx="43.3" cy="57.1" r="4.5" fill="#59C2FF"/>
<circle cx="76.7" cy="57.1" r="4.5" fill="#59C2FF"/>
```

The LED dots above only mark the positions: they are not part of the panel SVG.
If the LEDs end up larger, widen the gaps in the front ring by the same amount.

## Implementation

SAF has no UHJ encoder, and its `hilbert()` function works offline on a whole
buffer (FFT), which can't be used for per-sample processing. The module is
implemented by a small reusable kernel `sp4t::UhjEncoder`
(`src/UhjEncoder.hpp`), with the same `process(const float* in, float* out)`
interface as `sp4t::AmbiStereoDecoder`. No memory is allocated after
construction.

### Encoding equations

Two-channel UHJ encoding, with FuMa-normalized W:

S = 0.9396926 * W + 0.1855740 * X
D = j(-0.3420201 * W + 0.5098604 * X) + 0.6554516 * Y
L = 0.5 * (S + D)
R = 0.5 * (S - D)

where j is a wide-band +90 degree phase shift.

The equations expect FuMa B-format (W, X, Y, Z order, W at -3 dB), while the
input is ambiX (ACN/SN3D). Each sample, the first four input channels are
copied to a local 4-sample buffer and converted with SAF's
`convertHOAChannelConvention()` (`HOA_CH_ORDER_ACN` to `HOA_CH_ORDER_FUMA`) and
`convertHOANormConvention()` (`HOA_NORM_SN3D` to `HOA_NORM_FUMA`), with
`order = 1` and `signalLength = 1`. Both convert in place and allocate nothing.
At first order this only reorders the channels and scales W by 1/sqrt(2). Z is
then ignored.

Let T = -0.3420201 * W + 0.5098604 * X, the term that gets the 90 degree shift.

Higher-order input cables (9 or 16 channels) are accepted: only the first four
channels are read (the copy keeps SAF from zeroing the higher-order channels of
the module's own buffer). With fewer than 4 channels, the missing channels are
zero.

The standard coefficients are used as-is, which keeps UHJ's known stereo image
and mono sum. Since no UHJ decoder needs to read the output, a fixed make-up
gain may be added if the output level turns out too low compared to Stereo Out
(a front source comes out at about 0.45 times the input W level on each
channel).

### Phase-shift network

No filter shifts a single signal by 90 degrees at all frequencies. Instead, two
allpass chains A and B are designed so that the phase of B's output leads A's
output by 90 degrees over the audio band. Every term goes through one of them:

L = 0.5 * (A(S) + 0.6554516 * A(Y) + B(T))
R = 0.5 * (A(S) - 0.6554516 * A(Y) - B(T))

i.e. three chains in total: A on S, A on Y, B on T (this is also how OpenAL
Soft's UHJ encoder is structured). Getting the sign of the shift wrong (B
lagging instead of leading) still produces a plausible stereo signal, but not
the UHJ one: the phase between L and R, and so the image of side and rear
sources, differs from the standard. This must be tested (see below).

The chains are polyphase IIR allpass filters, taken from Laurent de Soras' HIIR
library (`hiir::PhaseHalfPi`, header-only, WTFPL licence, vendored in
`src/dep`), or an equivalent hand-written pair of allpass chains. The
coefficients are designed for the current sample rate (HIIR's
`PolyphaseIir2Designer`) so that the 90 degree difference holds from about
20 Hz up to near Nyquist, recomputed in `onSampleRateChange()` with the filter
states cleared. Fixed coefficients computed for 44.1/48 kHz would move the
accurate band up in frequency at higher sample rates and degrade the low end.

### Latency

The allpass chains add no fixed latency (no lookahead, no buffering), unlike an
FIR Hilbert transformer, which would need a delay of half its length (thousands
of samples to reach 20 Hz). The group delay is frequency-dependent: negligible
above a few hundred Hz, largest near the low edge of the band. Both outputs go
through the same kind of chains, so L and R stay aligned and the mono sum is not
affected. Mixing the output with an unprocessed copy of the same material (e.g.
Stereo Out fed from the same signal) may cause some low-frequency coloration;
this is worth one line in the manual.

### Lights

The two LEDs have the same settings as in the Stereo Out module: blue light
(Ayu theme blue, as for AMB jack ports and SP4T titles) showing the peak of the
audio-rate L,R output signals.

### Testing

- Reference: compare the output with OpenAL Soft's `uhjencoder` utility, which
  implements the same equations, on a B-format test file (after ambiX to FuMa
  conversion).
- Expected values for a unit-amplitude SN3D source in the horizontal plane:
  - front (0 degree): equal L/R levels.
  - left (90 degrees): L about 15 dB above R.
  - rear (180 degrees): equal L/R levels, mono sum (L + R) about 5 dB below a
    front source.
- Check the 90 degree phase difference between the A and B chains over the
  band at 44.1, 48, 96 and 192 kHz.
