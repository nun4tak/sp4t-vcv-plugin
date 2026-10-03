# SP4TEarsOut Module

## Description

A binaural Ambisonic decoder.

The module takes an ambisonic input signal (polyphonic cable) and decodes it
into two left/right stereo (headphones) channels. 

There's no head orientation built in this module. Use the Rotate module before
this one.

## Specifications

- Width: 4 HP
- 1 AMB Ambisonic jack input (polyphonic) 
- 2 audio jack outputs:
  - L (left)
  - R (right)
- 2 LED lights in the head drawing at the ears positions
- Right-click menu items
  - decoding method: least-squares (default), least-squares + diffuse-field EQ,
    spatial resampling, time-alignment, magnitude least-squares
  - max-rE weighting (default: on)
  - diffuse-field covariance matching (default: off)
  - truncation EQ, used by the least-squares method only (default: on)
  - read-only: the latency of the decoding filters

## Panel design

The design of the panel is done in the SP4TEarsOut SVG files.

## Implementation

Implemented in `src/AmbiBinauralDecoder.cpp` on top of SAF's binaural decoders,
the ones behind its `ambi_bin` example, and SAF's default HRIR set (a KEMAR
dummy-head Aural ID, 836 directions, 256 taps at 48 kHz). No other HRIR set is
supported. The HRTFs get the diffuse-field equalisation that is `ambi_bin`'s
default pre-processing, and the decoder options `ambi_bin` exposes as plugin
parameters are right-click menu items here.

The Ambisonic order of the input signal is given by the number of channels of
the polyphonic cable (4, 9 or 16). The two lights have the same settings than in
the Stereo Out module.

`ambi_bin` renders in the frequency domain through an afSTFT filterbank, which
needs blocks. Here the per-bin binaural decoding matrices are turned into one
pair of FIR filters per input channel instead (SAF's
`getBinauralAmbiDecoderFilters()`, inlined so that the truncation EQ can be
applied per bin), and those filters convolve the input sample by sample. At
48 kHz they are 256 taps long, which costs about 1% of a CPU core at order 3.
The HRIR set is resampled to the host sample rate and the filter length follows
it, capped at 1024 taps (the HRIRs get truncated above 192 kHz).

The inverse FFT gives circular filters: only the taps holding the most energy
are kept, and the ones before the first of them are the non-causal part of the
filters, i.e. the latency of the module.

Computing the filters takes about a second, so it is left to a worker thread
that the module starts once and wakes up whenever the sample rate or a menu
option changes. The filters of all three orders are computed in one go, so that
plugging a cable with a different number of channels takes effect at once. The
module outputs silence until the first set is ready and keeps decoding with the
previous set while a new one is computed.

The convolution costs the same whatever the input, so it is skipped altogether
when the AMB input is unplugged or when neither output is, after letting the
tail of the filters ring out. The LEDs then go dark too, i.e. the module is not
a meter on its own: at least one output has to be patched.

### Decoding methods

The five methods fall in two families with opposite trade-offs.

The least-squares ones (LS, LS + diffuse-field EQ, SPR) keep the measured HRTF
phase. Their filters are compact, holding 99.93 to 99.98% of their
impulse-response energy within 1 ms of the peak, so a waveform comes out of the
decoder essentially unchanged in shape and 256 taps reproduce them to about
-50 dB. They pay for it in magnitude: truncating the Ambisonic order colours the
response direction by direction, and the truncation EQ, being a single
diffuse-field gain curve, can only correct the average.

Time-alignment and magnitude least-squares discard the HRTF phase above the band
where hearing uses interaural time differences (~1.5 to 2 kHz) and fit magnitude
only, prioritising ILD over ITD. Their magnitude response is the flatter one,
within a couple of dB at every direction and order, but the filters are
temporally dispersive (only 92% of the energy within 1 ms of the peak) and
visibly reshape a harmonic-rich waveform on a scope. That reshaping is inherent
to the methods rather than an artifact of rendering them as FIR filters: the
decoders are linear to -120 dB, i.e. they put no energy at all at non-harmonic
frequencies, and recomputing the filters with an 8192-point FFT and 4096 taps
changes the result by less than 1 dB.

Least-squares is the default: it has the lowest latency of the five (7 samples
at 48 kHz, against about 125 for magnitude least-squares), it leaves a waveform
intact, and it is the steadiest of the phase-preserving methods. Its trade-off
is a direction-dependent high-frequency coloration: measured against the default
HRIR set, a source at +/-90 degrees comes out 1 to 8 dB hot between 4 and
14 kHz, while one in front loses 10 dB at order 1 to 23 dB at order 3 around 4
and 14 kHz.

Two known limits of the current rendering:

- 256 taps reproduce time-alignment and magnitude least-squares only to about
  -31 dB, against about -50 dB for the phase-preserving methods, because their
  impulse responses are long. Bringing them to the same accuracy would take
  around 2048 taps, i.e. roughly 10% of a CPU core at order 3. This is a
  separate matter from the waveform reshaping above, which more taps do not fix.
- SPR preserves phase, so it inherits the same order-truncation coloration as
  LS, and it is less regular: at order 3, relative to its own low-frequency
  level, it drops about 11 dB at 4 kHz for a source in front and about 24 dB at
  4 kHz for one at 45 degrees. LS is the steadier of the two phase-preserving
  options.
