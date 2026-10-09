# SP4TSplitInsert Module

## Description

Ambisonic dual effect inserts (omni + directional)

Allows two external inserts to be applied in parallel via the send/return ports
respectively for the isotropic (omni) and directional components of an input
ambisonic signal. The components are then merged back together (possibly mixed
with the input) into the output ambisonic signal.

## Specifications

- Width: 10 HP
- 1 AMB Ambisonic jack input (polyphonic)
- 1 AMB Ambisonic jack output (polyphonic)
- 1 Dry-Wet Knob (default: full wet) + CV input + attenuverter
- 2 panel sections omidirectional (1st ambisonic channel) vs. directional (all other channels), each with:
  - 1 pre-gain knob (0x to 2x, default to 1x)
  - 1 pre-gain CV input jack
  - 1 post-gain knob (0x to 2x, default to 1x)
  - 1 send output jack (single-channel for omni section, polyphonic for directional section)
  - 1 return input jack (single-channel for omni section, polyphonic for directional section)
  - send is normalized to return
  - 1 delay knob (sample compensation, default: 0 samples) 

## Panel design

The design of the panel is done in the SP4TSplitInsert SVG files.

## Implementation

The Ambisonic order of the input signal is given by the number of channels of
the polyphonic cable (4, 9 or 16).

Delay compensation and dry/wet must be handled carefully. The delay
compensation control of an insert declares the latency of its whole chain,
which cannot be known programmatically, so the module takes that value at face
value whether or not the return is patched: a send normalled to its return is a
chain of "virtual" modules introducing exactly that latency, i.e. a plain delay
of that many samples. Plugging a cable into the return then changes nothing but
where the signal comes from.

Each signal is held once and the dry/wet mix happens last:

- 1. Hold the dry signal of both insert chains by the largest of the two delay
  compensation values.
- 2. Hold what comes back from each insert by what separates its own
  compensation from that largest one: the difference for a return, which
  already carries the latency its control declares, and the whole of it for a
  normalled send, which carries none.
- 3. Apply the post gain and mix the held dry and wet signals of each chain,
  both according to the global dry-wet value. The two chains are mixed from
  signals that left their insert at the very same moment, so the post gains and
  the dry-wet value act on them together.
- 4. Route the mixed outputs of the two inserts to the ambisonic polyphonic
  output.

Linear dry-wet crossfading is usually the safer default for correlated signals
(equal-power will slightly bump the level).

The delay compensation knobs span 0 to 4096 samples, which covers the latency
of the usual block-based effects (the afSTFT modules of this plugin report 1664
samples). They are set in whole samples, with a square-law taper: a compensation
of a few samples (a zero-latency insert, down to the single sample of a cable)
takes as much of the travel as the last hundreds, and a block latency is best
typed in. The taper lives in the knob's ParamQuantity, so the parameter itself
is the knob position (0 to 1) rather than a sample count.

The right-click menu reports it as "Latency for the insert chain", the
compensation of the longest insert: what both chains are aligned to, rather
than something the module adds on top of them.
