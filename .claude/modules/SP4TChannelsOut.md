# SP4TChannelsOut Module

## Description

A multi-channel Ambisonic decoder.

The module takes an ambisonic input signal (polyphonic cable) and decodes it
into up to 16 output channels, one per direction of a layout, with optional
LFE channels. A channel is a direction rather than a speaker as such: feeding
a real or virtual rig is the usual use, but SAD, for one, points a virtual
microphone along each direction instead of panning onto speakers. Inspired by SPARTA's AmbiDEC
(https://leomccormack.github.io/sparta-site/docs/plugins/sparta-suite/#ambidec)
and built on the same SAF decoder designs, but simpler: no binauralisation, no
SOFA import, at most 16 channels, and no latency (purely time-domain processing,
sample per sample).

The speaker positions are fixed: there is no CV control over them. For moving
sounds, modulate the encoded sources instead (e.g., with the Source4 In module)
or the sound field (Rotate module). What this module offers is precise
positioning: the azimuth and elevation of each channel are typed in a table of
text fields on the panel, which is also filled from presets of common surround
speaker layouts (quadraphonic, 5.1, 7.1, 7.1.4, rings, regular solids...).

Each output channel has its own jack, and all of them are also available
together on a polyphonic output jack. Both always carry the same channels, in
the same order.

The module covers the common use cases. For more advanced setups (more than 16
speakers, bass management, distance compensation...), route the ambisonic
signal outside of VCV Rack and decode it with dedicated tools in a DAW.

## Specifications

- Width: 14 HP
- 1 AMB Ambisonic jack input (polyphonic)
- 16 audio jack outputs, one per channel (1 to 16)
- 1 POLY audio jack output (polyphonic), carrying channels 1 to N where N is the
  highest used channel (see below)
- 16 LEDs, one per channel output jack
- 4 LEDs showing the current page of the channel table
- 4 buttons:
  - previous / next layout preset
  - previous / next page of the channel table
- 1 screen: 3D view of the speaker positions
- 1 layout display (`LedDisplayChoice`): name of the current layout preset, or
  "Custom". Clicking it opens the layout preset menu.
- 1 channel table: 16 rows (channels), 4 rows per page, 4 pages always
  reachable. Columns:
  - CH: channel number (not editable)
  - AZ: azimuth, text field (-180 to 180 degrees, positive to the left)
  - ELEV: elevation, text field (-90 to 90 degrees, positive to the top)
  - LFE: toggle (click, or Space when the row has the keyboard focus)
- Right-click menu options:
  - layout preset (same list as the layout display menu)
  - decoding method: AllRAD (default), EPAD, SAD
  - max-rE weighting (default: on)
  - dual-band decoding (default: off), and when enabled:
    - crossover frequency (default: 800 Hz; a short list of values, e.g., 200,
      400, 800, 1600 Hz)
    - low band order and high band order (1 to 3, default: 3 for both, capped by
      the input order)
  - LFE feed: silent (default), or the omni (W) channel low-passed at 120 Hz
  - 3D view mode (same options as the Source4 In and Stereo In modules)
- Right-click menu of a table row: clear channel (back to unused)

## Panel design

The design of the panel is done in the SP4TChannelsOut SVG files.

The panel has two parts:

- On the left, from top to bottom:
  - the 3D view screen: same height and place as in Source4 In, slightly
    narrower
  - LAYOUT label, then the layout display between the previous / next layout
    buttons, with small triangles between each button and the display
  - CH / AZ / ELEV / LFE column labels above the table screen. The table sits
    close to the PAGE controls below, so that they read as one group.
  - the bottom row: the AMB input, then the page controls (previous / next page
    buttons around the 4 page LEDs). The AMB, PAGE and POLY labels share the
    same baseline (y = 107.4 mm, jacks at y = 114 mm, as in the other modules).
- On the right: a grouping block holding the 16 channel output jacks on a 2 x 8
  grid, numbered row by row (1-2 on the first row, 3-4 on the second...), so
  that the usual channel pairs (L/R, C/LFE, Ls/Rs...) sit side by side. Each
  number label is centered above its jack, with the LED at its left. The POLY
  output is at the bottom of the block. It uses a plain PJ301M jack, as it
  carries speaker channels and not an ambisonic signal.

### Channel table

Each row is in one of three states, each with its own look:

- speaker: AZ and ELEV show their values (one decimal), LFE shows a dim "/"
- LFE: AZ and ELEV show a dim "/", LFE shows "LFE" (Ayu orange)
- unused: dim channel number and dim "---" placeholders in the three fields.
  Unused rows are dimmer than the "/" of a used row.

Editing rules:

- Setting AZ or ELEV makes the row a speaker; setting LFE makes it an LFE
  channel. The two are exclusive: a used channel has either both AZ and ELEV
  set, or LFE set.
- Typing a value in the AZ field of an unused row sets its ELEV to 0, and the
  other way round, so that a row is never half set.
- Toggling LFE off brings back the angles the channel had before (they stay in
  the parameters), or makes the row unused if it had none.
- Emptying a field and pressing Enter, or "Clear channel" in the row menu, makes
  the row unused.
- A value is parsed and clamped when Enter is pressed or when the field loses
  the focus. If it cannot be parsed, the field goes back to its last value.
- Tab / Shift-Tab move along AZ and ELEV fields (`TextField::nextField` and
  `prevField`), skipping the fields of LFE rows, and flip to the next / previous
  page past the last / first row of a page.

Any edit that leaves a layout different from all presets shows "Custom" in the
layout display.

### 3D view screen

Reuse `AmbisonicDisplay`. Show the speaker channels only, labeled with their
channel number (Ayu purple, to tell them apart from the orange sources of the
encoder modules). LFE channels have no direction and are not drawn; instead, a
note in the bottom-left corner lists them (e.g., "LFE: 4", or "LFE: 4 10"). With
no speaker channel at all, the screen says so.

## Implementation

### Channels and parameters

Each channel has three parameters with no panel widget (they are edited through
the table): azimuth, elevation and state (unused / speaker / LFE), i.e., 48
parameters. Being parameters, they are saved with the patch and the `.vcvm`
presets, reset by "Initialize" and undoable: every edit from the table goes
through a `history::ParamChange`. Disable randomization on all of them
(`randomizeEnabled = false`), otherwise "Randomize" would scramble the layout.

`LedDisplayTextField` is a full `ui::TextField` (editable, `onAction` on Enter,
`onChange` on each key), but its text is not saved: the table widgets only
display the parameters and write to them, like the Notes module of Rack keeps
its text in the module.

The number of channels N is the highest used channel. The POLY output has N
channels, and channel jacks above N are not active. An unused channel at or
below N (a gap in the layout) outputs 0 V, on its jack and in the poly cable
alike. Its LED stays off.

### Layout presets

Taken from SAF's loudspeaker presets
(`saf_utility_loudspeaker_presets.h`) where they exist, for instance: stereo,
LCR, quad, 5.0, 5.1, 7.0, 7.1, 5.1.2, 5.1.4, 7.1.4, hexagon, octagon, 16-speaker
ring, octahedron, cube, icosahedron. A preset sets the state and angles of all
16 channels; the channels it does not use become unused. Presets with an LFE
follow the SMPTE channel order (L R C LFE Ls Rs..., i.e., LFE on channel 4).
Check SAF's channel order and azimuth sign against the plugin conventions
before reusing a preset.

The layout display shows the name of the preset that matches the current
channels, if any, else "Custom".

### Decoding

Do not use SAF's `ambi_dec` processing: it runs an afSTFT filterbank on blocks
of samples, which adds latency. Use only SAF's decoder design functions, and
apply the matrices sample per sample:

- `getLoudspeakerDecoderMtx()` with `LOUDSPEAKER_DECODER_ALLRAD`, `_EPAD` or
  `_SAD`, given the directions of the speaker channels only (LFE and unused
  channels are left out, and their rows of the final matrix are zero)
- `getMaxREweights()` for the max-rE weighting
- the normalisation of `ambi_dec.c`: amplitude preserving for the low band,
  energy preserving for the high band (and for single-band decoding). Check
  the resulting levels against Rack's +/- 5 V convention, as was done for Stereo
  Out.
- for horizontal-only layouts with AllRAD, add two virtual speakers at the
  poles before designing the matrix, then remove their rows, as `ambi_dec.c`
  does, so that the triangulation does not fail
- convert the SN3D input to N3D before applying SAF's matrices, as
  `AmbiStereoDecoder` does

The Ambisonic order of the input signal is given by the number of channels of
the polyphonic cable (4, 9 or 16). When the layout has too few speaker channels
for that order (e.g., 3rd order on a quad), cap the decoding order to what the
layout supports (2N+1 speakers for order N on a horizontal layout).

Single-band decoding is one matrix product, at most 16 x 16 multiply-adds per
sample.

Dual-band decoding uses a 4th-order Linkwitz-Riley crossover. Its low-pass and
high-pass outputs sum to an all-pass AP, so with HP = AP - LP:

out = D_hi · AP(x) + (D_lo − D_hi) · LP(x)

where x is the input ambisonic signal and D_lo, D_hi the matrices of each band
(their own order, max-rE weighting and normalisation). That is three biquads per
input channel (LP: two, AP: one) and two matrix products. There is no block
latency; the all-pass phase shift is the same on all channels, so it does not
change the decoded image. When both bands have the same matrix, skip the filters
and decode with one matrix product.

A different order per band is a matrix whose higher-order coefficients are
zero (then renormalised), not a separate mechanism: lower orders give a more
diffuse sound, higher orders a sharper one.

AllRAD designs its matrix from a 480-point t-design with VBAP, which is too slow
for the audio thread. Compute the matrices (all three orders, both bands) on a
worker thread, woken up whenever a channel, a decoding option or the sample rate
changes, as in the Ears Out module. The audio thread picks up a new set at a
block boundary and interpolates the gains over 32 samples, as the other
decoders do. Keep decoding with the previous set while a new one is computed.

### LFE

Channels in the LFE state carry the LFE feed: silence by default, or the W
channel through a 120 Hz low-pass filter (menu option). All LFE channels carry
the same signal.

### LEDs

The 16 channel LEDs are `SP4TSignalLight`s with the same settings as in the Gain
module: peak of the channel output, full scale at 5 V with a cube-root
response, small glow (`haloSpread` of 1.5 px). The LEDs of unused and inactive
channels are off.

The 4 page LEDs show the current page of the channel table (one lit at a time).
The current page is a UI state, saved in `dataToJson()` / `dataFromJson()`.
