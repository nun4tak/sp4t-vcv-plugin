# SP4T Modules

The modules are grouped by what they do to a signal: encode it into a sound
field, decode it back for listening, process it as a whole, or work on it
channel by channel.

Every jack labelled **AMB** carries a whole ambisonic signal over a polyphonic
cable with as many channels as the order it was encoded at (1st order = 4, 2nd
order = 9, 3rd order = 16).

## Encoders

Modules that turn ordinary audio streams into a sound field (input: one or more
regular audio channels --- output: an ambisonic signal).

### SP4T Stereo In

```{image} images/modules/SP4TStereoIn.svg
:alt: SP4T Stereo In
:class: module-panel
```

*Ambisonic encoder for one panned mono or stereo source.*

Places either a mono point source or a pair of L/R stereo point sources in 3D
space with the **AZ** (azimuth) and **ELEV** (elevation) knobs. The source is
mono when only the left input is patched, and stereo when both are. In the
latter case, **SPREAD** sets the angle between each of the two L/R sources and
the "mono" direction that the knobs point at.

The switch next to the **AMB** output selects the order (1, 2, 3) of the encoded
ambisonic signal. Increasing the order encodes the sound field at a higher
spatial resolution, which results in more channels (4, 9, 16) carried by the
output polyphonic cable.

The switch below the **AZ** knob flips the sign of the azimuth angle for both
the knob and input CV: `+` follows the ambisonic convention (a
positive angle turns the source to the left) while `-` may be more intuitive
(turning the knob right moves the source to the right).

The screen shows the position(s) — direction(s) — of the source(s).
Different view modes are available via the right-click menu.

- **Width**: 10 HP
- **In**: L (mono), R — **Out**: AMB
- **Knobs**: AZ (±180°), ELEV (±90°), SPREAD (0–90°, default 45°), each with a
  CV input and a trim

### SP4T Source4 In

```{image} images/modules/SP4TSource4In.svg
:alt: SP4T Source4 In
:class: module-panel
```

*Ambisonic encoder for up to four point sources, or 64 with polyphonic cables.*

One column per source (input at the bottom), each with its own **AZ** and
**ELEV** knobs and CV inputs. In addition, the **OMNI** input can be used to add
an omni-directional source (sound coming from everywhere in 3D space).

A source input also takes a polyphonic cable (up to 16 sources per column, 64 in
total). The **AZ** and **ELEV** CV inputs are read channel per channel, so a
polyphonic CV spreads the channels of a column over as many directions, while a
monophonic one — or the knob on its own — sends them all to the same place.
Channels past the end of a shorter CV stay on the knob direction.

Each CV row is normalled from left to right: a jack with nothing patched takes
the signal of the nearest patched jack on its left.

The switch next to the **AMB** output selects the order (1, 2, 3) of the encoded
ambisonic signal. Increasing the order encodes the sound field at a higher
spatial resolution, which results in more channels (4, 9, 16) carried by the
output polyphonic cable.

The sign of the azimuth angle can be flipped for both the knobs and input CVs
via the right-click menu: the default sign follows the ambisonic convention (a
positive angle turns the source to the left) while the flipped sign may be more
intuitive (turning the knob right moves the source to the right).

The screen shows the positions — directions — of the sources being encoded,
numbered from 1 in the order they are encoded in. Different view modes are
available via the right-click menu.

- **Width**: 10 HP
- **In**: 4 sources (polyphonic, up to 16 channels each), OMNI — **Out**: AMB
- **Knobs**: AZ (±180°) and ELEV (±90°) per source, each with a CV input

## Decoders

Modules that turn an sound field back into something we can listen to (input: an
ambisonic signal --- output: one or more regular audio channels suitable for
playback).

### SP4T Stereo Out

```{image} images/modules/SP4TStereoOut.svg
:alt: SP4T Stereo Out
:class: module-panel
```

*Decodes the sound field to a pair of stereo speakers.*

The two virtual speakers are positioned in front of the listener, in the
horizontal plane. **SPREAD** corresponds to the angle between each speaker and
the X (front-rear) axis: 0° makes both outputs the same (mono), 90° is hard
left/right.

This is a basic decoder that is very cheap on CPU. Although it captures the
whole 3D sound field poorly, it may still be interesting for some sound design
experiments. For even cheaper computation and less accuracy, the "crude mode"
(available via the right-click menu) uses only the first two W,Y ambisonic
channels (i.e., mid/side decoding) and completely ignores the rest. For
headphones, you may want to use the [Ears Out](#sp4t-ears-out) module instead
(binaural decoding, more CPU-hungry).

- **Width**: 4 HP
- **In**: AMB, SPREAD CV — **Out**: L, R
- **Knob**: SPREAD (0–90°, default 45°), with a CV input and a trim

### SP4T Ears Out

```{image} images/modules/SP4TEarsOut.svg
:alt: SP4T Ears Out
:class: module-panel
```

*Decodes the sound field to headphones (binaural).*

A binaural decoder, for listening to the whole field over headphones. It has no
head tracking of its own, but you can use the [Rotate](#sp4t-rotate) module
before it to turn the scene.

Multiple decoding options are available via the right-click menu. Note that some
algorithms are included here for completeness, but may not yield good results
or may have more latency.

- **Width**: 4 HP
- **In**: AMB — **Out**: L, R

### SP4T XY-MS Out

```{image} images/modules/SP4TXY-MSOut.svg
:alt: SP4T XY-MS Out
:class: module-panel
```

*Decodes the sound field to stereo with a pair of virtual coincident
microphones (X/Y or M/S).*

Renders the sound field as if it had been recorded with a pair of coincident
microphones placed at the centre of the scene, in the horizontal plane. The
switch picks the pair:

- **X/Y**: two identical microphones, angled apart by **SPREAD** (the angle
  between each of them and the front axis).
- **M/S**: a forward-facing mid microphone and a sideways (left-facing)
  figure-8, **SPREAD** setting their balance, i.e. the stereo width — 0° is the
  mid microphone alone (mono), 45° the classic M/S balance, 90° the side
  microphone alone.

**PATTERN** sweeps the microphone pattern continuously from omni through
cardioid and hypercardioid to figure-8, so all the classic coincident
techniques are available from the same two knobs (see the table below). In M/S
mode it is the pattern of the mid microphone; the side one is always a
figure-8.

| Pattern | PATTERN | Front/rear pickup |
|---|---|---|
| Omni | 0 | equal all around |
| Subcardioid | 0.3 | front-weighted, no null |
| Cardioid | 0.5 | null at the rear |
| Supercardioid | 0.634 | nulls at ±125°, small inverted rear lobe |
| Hypercardioid | 0.75 | nulls at ±110°, larger inverted rear lobe |
| Figure-8 | 1 | nulls at the sides, rear lobe inverted |

The screen draws the two patterns in the horizontal plane, on a fixed scale
where the outline circle is a gain of 1 and the graticule circle a gain of 0.5,
so a change of level shows as the lobes growing or shrinking. Lobes of inverted
polarity are dashed. The active mode is named in the bottom left corner and the
pattern, abbreviated, in the bottom right one — prefixed with `~` while the
knob sits between two named patterns. The LED in each capsule of the drawing
below shows the level of that microphone.

The module takes an input of any order but uses only the first-order horizontal
channels. This is exact rather than a shortcut: a first-order microphone only
picks up the first-order part of the sound field. There is no orientation
control — use the [Rotate](#sp4t-rotate) module before this one to aim or tilt
the pair. For a loudspeaker-panning rendering, see
[Stereo Out](#sp4t-stereo-out); for headphones, see [Ears Out](#sp4t-ears-out).

Turning **PATTERN** up also makes the microphones quieter in a diffuse field,
by up to 4.8 dB for a figure-8. The "diffuse-field level compensation" option
of the right-click menu takes that out, so that the overall loudness stays
roughly the same across the range. It is off by default: the plain microphone
behaviour is the reference.

Usual techniques:

| Technique | Mode | PATTERN | SPREAD | What it captures / stereo image | Mono and phase |
|---|---|---|---|---|---|
| X/Y cardioid 90° | X/Y | 0.5 | 45° | Front-weighted, rear rejected. Stable, fairly narrow image, little room sound. | Excellent mono, no inverted lobes. |
| X/Y cardioid 110–135° | X/Y | 0.5 | 55–67.5° | Wider image, slightly weaker centre. | Very good; the centre drops a little in mono. |
| X/Y super/hypercardioid | X/Y | 0.634–0.75 | 50–60° | Tighter, more focused image; the small rear lobes add some room sound. | Rear sources partly inverted: diffuse, slightly phasey. |
| Blumlein | X/Y (or M/S with a figure-8 mid) | 1 | 45° | Full 360° pickup. Very accurate front image; rear quadrant mirrored (rear-left shows up on the right); lots of room sound. | Side quadrants inverted between L and R: phasey reverb, side sources vanish in mono. |
| M/S cardioid | M/S | 0.5 | 30–60° | Natural image, width adjustable with SPREAD; centre always solid. | Perfect: the mono sum is the mid microphone. |
| M/S hypercardioid | M/S | 0.75 | 30–60° | Drier, more focused centre. | Mid rear lobe inverted; mono still clean. |
| M/S omni | M/S | 0 | 45° | Equivalent to two back-to-back cardioids at ±90°: very wide and spacious, room sound from all around, vague centre. | Mono = omni, very clean. |
| M/S figure-8 | M/S | 1 | any | Identical to an X/Y figure-8 pair at ±SPREAD (Blumlein at 45°). | As Blumlein. |
| Omni / subcardioid pair | X/Y | 0 / 0.3 | any | Omni gives plain mono; subcardioid a very roomy, narrow image. | Clean. |

All these techniques are coincident: L and R differ only in level and polarity,
never in time, so there is no comb filtering in mono. Mono issues come only
from inverted lobes (supercardioid, hypercardioid, figure-8), which partly
cancel and make some sources or reverb sound phasey. In M/S mode, **SPREAD**
never changes what reaches the mono sum (always the mid microphone, scaled down
as the spread goes up), so the width can be adjusted without affecting mono
compatibility; this is not the case in X/Y mode. The virtual microphones have
ideal, frequency-independent patterns: with a recording made by a real
first-order microphone, the patterns are less accurate at high frequencies.

For separate M/S processing downstream, use M/S mode at **SPREAD** 45° and any
M/S encoder after the L and R outputs: their sum and difference give back the
mid and side signals at equal level.

- **Width**: 8 HP
- **In**: AMB, PATTERN CV, SPREAD CV — **Out**: L, R
- **Knobs**: PATTERN (0–1, default 0.5 = cardioid), SPREAD (0–90°, default
  45°), each with a CV input and a trim
- **Switch**: X/Y or M/S

### SP4T UHJ Out

```{image} images/modules/SP4TUhjOut.svg
:alt: SP4T UHJ Out
:class: module-panel
```

*Decodes the sound field to two-channel UHJ stereo.*

Turns the sound field into the two channels of the standard two-channel UHJ
format (Gerzon, 1985), to be played straight through a pair of speakers or
headphones. Going from B-format to UHJ is strictly speaking an *encoding* —
UHJ carries a sound field rather than feeding speakers — but with no UHJ
decoder to follow it, the module simply ends an ambisonic chain, as Stereo Out
and Ears Out do.

Where [Stereo Out](#sp4t-stereo-out) decodes to two virtual speakers in front
of the listener, UHJ keeps the whole horizontal sound field in the phase
relationship between the two channels:

- played as plain stereo, front and side sources are sharply imaged and the
  stage often sounds wider than the speakers themselves. Rear sources are
  folded into the front stage, where they keep a less focused, recessed
  quality of their own.
- the mono sum is clean, with no comb filtering: summing L and R gives a
  single, slightly forward-oriented pickup, in which a rear source sits about
  5 dB below the same source in front.

Only the first-order horizontal channels are used: height and everything above
first order are discarded. There is no orientation control — the front is the
+X axis, so use the [Rotate](#sp4t-rotate) module before this one to aim the
field.

The wide-band +90° phase shift that UHJ is built on is made of allpass
filters, which add no latency of their own (an FIR Hilbert transformer would
need a delay of thousands of samples to reach down to 20 Hz). Their group
delay is frequency-dependent instead: negligible above a few hundred hertz,
longest at the bottom of the band. Both outputs go through filters of the same
kind, so L and R stay aligned with each other and the mono sum is unaffected;
mixing these outputs with an unprocessed copy of the same material — Stereo
Out fed from the same signal, say — may colour the low end.

A fixed make-up gain of 2 is applied to both channels, which brings a front
source within half a decibel of what Stereo Out decodes the same source to.
Being identical on L and R, it leaves the UHJ relationship between them
untouched.

- **Width**: 4 HP
- **In**: AMB — **Out**: L, R

### SP4T Channels Out

```{image} images/modules/SP4TChannelsOut.svg
:alt: SP4T Channels Out
:class: module-panel
```

*Decodes the sound field to a layout of up to 16 output channels.*

A multi-channel decoder: it turns the sound field into one signal per channel
of a layout, with optional LFE channels. Each channel has its own jack,
numbered 1 to 16, and all of them are on the **POLY** output as well, in the
same order — that one carries as many channels as the highest one in use.

A channel is a *direction*, not necessarily a speaker. Feeding a real or
virtual rig is the usual use, and many of the layout presets are named after
such rigs, but what each decoding method actually builds differs: AllRAD and EPAD
pan the field onto the directions, while SAD points a virtual microphone along
each of them. The channels are what they are made to drive.

The channel directions are typed in, not modulated: there is no CV over them.
To move a sound, modulate the sources going into an encoder
([Source4 In](#sp4t-source4-in)) or turn the whole field with
[Rotate](#sp4t-rotate). What this module gives instead is precision, through
the table of angles on the panel and a set of common layouts.

**LAYOUT** names the layout and the buttons on either side step
through the presets; clicking the name opens the list, which is also in the
right-click menu. Editing anything afterwards shows "Custom".

| Preset | Channels | |
|---|---|---|
| Stereo, LCR, Quad | 2, 3, 4 | the usual front pairs and a quadraphonic square |
| 5.0, 5.1, 7.0, 7.1 | 5 to 8 | SMPTE channel order: L R C LFE Ls Rs … |
| 5.1.4, 7.1.4 | 10, 12 | the same, with four height channels at 45° |
| Hexagon, Octagon, Ring 12, Ring 16 | 6 to 16 | horizontal rings, first channel dead ahead |
| Tetrahedron, Octahedron, Cube, Icosahedron | 4, 6, 8, 12 | regular solids, for full-sphere rigs |
| T-design 1 to 5 | 2, 4, 6, 12, 12 | spherical t-designs, every degree that fits in 16 channels |

A spherical *t*-design spreads its directions so evenly that a decoder built
on it is exact up to ambisonic order ⌊t/2⌋; degree 5 is the highest whose
design fits in 16 channels, so these stop at 2nd order. Degree 1 is the
degenerate pair, one channel ahead and one behind.

Some presets hold the same layout under two names, which is the point of
having both: the tetrahedron *is* the degree-2 design, and the degree-4 and
degree-5 designs are the same twelve directions. **LAYOUT** keeps the name you
picked until you change the layout. The octahedron and the degree-3 design are
the same six directions but hand them to the channels in a different order, so
those stay two layouts.

The table below it holds the 16 channels, four at a time; **PAGE** turns to the
next four and the LEDs between the buttons show where you are. Each row is one
channel:

- **AZ** and **ELEV** are the direction of the channel, in degrees — azimuth
  positive to the left, elevation positive upwards. Type a value and press
  Enter. Setting one of the two on an unused channel sets the other to zero, so
  that a row is never half filled in.
- **LFE** turns the channel into an LFE feed, which has no direction of its
  own. Click it, or press Space once it has the focus. Turning it off again
  brings back the angles the channel had.
- Emptying a field, or "Clear channel" in the row's right-click menu, gives the
  channel up: it then shows `---` and stays silent, even when channels above it
  are in use.

Tab and Shift-Tab walk along the angle fields, skipping the LFE rows and
turning the page at either end. Out-of-range values are clamped and anything
unparsable goes back to what it was.

The screen above shows where the channels point, each labelled with its
number; the LFE channels are listed in the corner instead, having no direction.
The view modes of the right-click menu are the same as on the encoders.

The decoder is designed with the
[Spatial Audio Framework](https://github.com/leomccormack/Spatial_Audio_Framework),
as SPARTA's AmbiDEC is, but it runs in the time domain and so adds no latency.
Three methods are on the right-click menu:

- **AllRAD** (the default) maps the field onto a dense virtual array and pans
  that onto the channels of the layout with VBAP. The most forgiving of an
  irregular or incomplete layout, and the one to keep unless there is a reason
  not to.
- **EPAD** preserves the energy of the field, which suits a regular layout.
- **SAD** is a sampling decoder — one virtual microphone per channel. The
  cheapest and the bluntest.

**Max-rE weighting** (on by default) trades a little localisation sharpness
for a steadier image away from the sweet spot. With dual-band decoding on it
becomes one switch per band: the textbook arrangement weights the high band
only, the ear taking its direction from energy up there and from amplitude
further down.

The decoding order is capped by what the layout can carry: a horizontal ring
needs 2N+1 channels for order N, a full-sphere layout (N+1)². A 3rd-order signal
on a quad is decoded at 1st order, and the extra channels are simply not used.

**Dual-band decoding** (off by default) splits the signal at the **crossover
frequency** (200 to 1600 Hz, 800 by default) and decodes each half its own way:
the low band with amplitude matching, which is what the ear uses for direction
down there, the high band with energy matching. Each band also gets its own
order and its own max-rE switch, so the low one can be left broad and the high
one sharp. The split is a 4th-order Linkwitz-Riley crossover with no latency of
its own; expect the per-channel level of the low band to sit well below the
high one, the low band being spread over more channels on purpose.

**LFE feed** decides what the LFE channels carry: silence by default, or the
omni (W) channel through a 120 Hz low-pass. Every LFE channel carries the same
signal.

Levels follow the usual convention: a source at 5 V comes out below 5 V on
every channel, whatever the layout and order.

For a rig of more than 16 channels, or for bass management and distance
compensation, decode outside of VCV Rack with dedicated tools instead.

- **Width**: 14 HP
- **In**: AMB — **Out**: 1 to 16, POLY
- **Buttons**: previous / next layout, previous / next table page

## Processors

Modules that act on the sound field as a whole (input: an ambisonic signal ---
output: the processed ambisonic signal).

### SP4T Rotate

```{image} images/modules/SP4TRotate.svg
:alt: SP4T Rotate
:class: module-panel
```

*Rotates the whole sound field.*

An exact, lossless rotation of the 3D sound field encoded in the input ambisonic
signal, around the three axes:

| Angle | Axis | What it does | Range |
|---|---|---|---|
| **Yaw** | Z (vertical) | turns the scene left/right | ±180° |
| **Pitch** | Y (left–right) | tilts the scene up/down | ±90° |
| **Roll** | X (front–back) | tilts the scene sideways | ±180° |

The three CV inputs are normalled from left to right, so one CV can drive all
three angles, each through its own attenuverter and `+`/`-` direction switch.

The screen shows the original X,Y,Z axes (grey) and the rotated X',Y',Z' axes
(orange).

- **Width**: 8 HP
- **In**: AMB — **Out**: AMB
- **Knobs**: YAW, PITCH, ROLL, each with a CV input, a trim and a direction
  switch

### SP4T Flip

```{image} images/modules/SP4TFlip.svg
:alt: SP4T Flip
:class: module-panel
```

*Mirrors the sound field across one or more axes.*

Each button flips the field across the X (front–rear), Y (left–right) or Z
(top–bottom) axis. The **DRY/WET** knob blends the result with the original
field using either an equal-power or a through-zero crossfade, whichever the
first switch selects.

The second switch selects the behaviour of the buttons and CV inputs: MOM
(momentary / gate) or LATCH (latching / trigger, the default).

- **Width**: 6 HP
- **In**: AMB, one gate/trigger CV per axis — **Out**: AMB
- **Buttons**: X, Y, Z
- **Knob**: DRY/WET, with a CV input and an attenuverter

### SP4T Blur Sharp

```{image} images/modules/SP4TBlurSharp.svg
:alt: SP4T Blur Sharp
:class: module-panel
```

*Blurs or sharpens the sound field.*

Changes how sharply the field is drawn without changing the ambisonic order (the
output signal has as many channels as the input signal).

**AMOUNT** controls blur vs. sharpen: left blurs, right sharpens and the middle
passes the signal through untouched.

Blurring reduces the apparent spatial resolution of the sound field and makes it
more uniform (full left = omni-directional) while sharpening does the opposite
and narrows the spatial image (full right = almost only directional). This
module is similar to a DJ-style filter (low-pass vs. high-pass), but instead of
filtering the frequencies of an audio signal, it filters the directions of an
ambisonic-encoded sound field.

Blurring is achieved by fading the higher-order ambisonic (HOA) channels until
only the first W channel (omni-directional) is left. To prevent the output
signal from getting too quiet, the gains of the lower-order channels are
increased as the higher orders fade. A trade-off is made between full
compensation (preserving the energy of the sound field) and no compensation at
all, which also avoids the output signal getting too hot and clipping.

The **SLOPE** control is used only when blurring; it sets how wide the fade
between HOA channels is, from nearly step-like muting to a soft progression. It
also has no effect on a 1st-order ambisonic signal.

- **Width**: 4 HP
- **In**: AMB, AMOUNT CV, SLOPE CV — **Out**: AMB
- **Knobs**: AMOUNT (−100% blur to +100% sharpen, default 0), with a CV input
  and an attenuverter; SLOPE (0.25 to the order of the input, default 1), with
  a unipolar CV input

## Utilities

Utility modules to mix whole sound fields, and to split, merge or process
(groups of) ambisonic channels independently of each other. Some modules accept
any polyphonic cable (not necessarily carrying an ambisonic signal).

### SP4T Mix3

```{image} images/modules/SP4TMix3.svg
:alt: SP4T Mix3
:class: module-panel
```

*Ambisonic mixer for up to three input signals.*

Sums up to three sound fields into one. Ambisonic signals are linear, so mixing
them is a channel-by-channel weighted sum: what comes out is the sound field
holding all the sources of the three inputs, at their own positions.

One column per input, each with its **level** knob, a **CV** input and its trim
(the CV attenuator above the jack). The gain of an input is

    level x clamp(trim x CV / 10 V, 0, 1)

The level knob tops out at unity — there is no boost — so the CV can only bring
an input down from where the knob sits, and three inputs at full level cannot
push the sum beyond the sum of what came in. The CV input is unipolar (0 to
10 V) and reads 10 V when nothing is patched, which is why the trim on its own
also works as a second attenuator. A level is a plain scalar on every channel
of its input, so it changes the loudness of that field and never its spatial
image; it cannot go negative, which would flip the polarity of a whole field.

Inputs of different orders mix freely: the output carries the highest order of
the patched inputs, and the channels a lower-order input does not have count as
zero. An unpatched input is skipped altogether.

The screen is a hint rather than a meter: one bar per ambisonic channel and per
input, in the colour of the input, on a 48 dB scale with a faint line every
12 dB. Ticks and the numbers 0 to 3 under them mark where each ambisonic order
starts, and the channels an input does not carry are drawn as flat dashes, so
the order of each input can be read at a glance. The right-click menu picks how
the bars are grouped:

- **Group by ambisonic channel** (the default) gives each ambisonic channel a
  group of three thin bars, with the level of the output laid over the group as
  a thin tick.
- **Group by ambisonic input** gives each input a row of 16 bars, introduced by
  its number.

- **Width**: 8 HP
- **In**: AMB 1-3, LEVEL CV 1-3 — **Out**: AMB
- **Knobs**: LEVEL per input (0 to 1x, default 1x), each with a CV input and a
  trim (0 to 100%, default 100%)

### SP4T Gain

```{image} images/modules/SP4TGain.svg
:alt: SP4T Gain
:class: module-panel
```

*A gain per ambisonic channel.*

The 16 ambisonic channels (up to 3rd order) are laid out as a grid from top-left to
bottom-right (ACN order), each with its own gain.

This module is mostly for convenience and for experimentation. Changing single
channels is not a spatially meaningful operation in general.

- **Width**: 10 HP
- **In**: AMB, one CV per channel — **Out**: AMB
- **Knobs**: 16 gains (0–2x). Each CV input is normalled to 5 V, so the gain is
  the knob's own with nothing patched

### SP4T Mute

```{image} images/modules/SP4TMute.svg
:alt: SP4T Mute
:class: module-panel
```

*A mute per ambisonic channel.*

The same grid as [Gain](#sp4t-gain), with one button per channel instead (muting
fades very quickly, avoiding clicks).

The switch selects the behaviour of the buttons and CV inputs: MOM
(momentary / gate) or LATCH (latching / trigger, the default).

This module is mostly for convenience and for experimentation. Changing single
channels is not a spatially meaningful operation in general.

- **Width**: 10 HP
- **In**: AMB, one gate/trigger CV per channel — **Out**: AMB
- **Buttons**: 16 mutes

### SP4T Split Insert

```{image} images/modules/SP4TSplitInsert.svg
:alt: SP4T Split Insert
:class: module-panel
```

*Two effect inserts, one for the omni part of the field and one for the rest.*

The input is split into two components: the omni-directional component
(single-channel) goes through the top send and return, every directional channel
through the bottom ones (polyphonic cables), and the two are merged back into
the output. Patch an effect into either loop to process one without touching the
other (e.g., reverb on the omni part only). A send with nothing patched into its
return passes straight through.

Each loop has a gain before and after the effect, and a delay compensation in
samples: set it to the latency of the effect in that loop, and both the dry
signal and the other loop are held back to match.

- **Width**: 10 HP
- **In**: AMB, two returns — **Out**: AMB, two sends
- **Knobs** per loop: PRE gain (0–2x, with a CV input), POST gain (0–2x),
  DELAY (0–4096 samples)
- **Knob**: DRY/WET, with a CV input and an attenuverter

### SP4T Split

```{image} images/modules/SP4TSplit.svg
:alt: SP4T Split
:class: module-panel
```

*Splits an ambisonic signal into its separate channels.*

Equivalent to VCV's [Split](https://library.vcvrack.com/Fundamental/Split), with
a panel laid out for ambisonics: the 16 ACN channels as a grid, from top-left to
bottom-right. The output channels that are not covered by the input signal
(order 1 or 2) stay at 0 V.

- **Width**: 6 HP
- **In**: AMB — **Out**: 16 single channels

### SP4T Merge

```{image} images/modules/SP4TMerge.svg
:alt: SP4T Merge
:class: module-panel
```

*Builds an ambisonic signal from separate channels.*

The counterpart of [Split](#sp4t-split), equivalent to VCV's
[Merge](https://library.vcvrack.com/Fundamental/Merge) with an ambisonic panel.
The switch sets the order of the output (1, 2, 3). Inputs with nothing patched
are 0 V and inputs beyond the chosen order are ignored.

- **Width**: 6 HP
- **In**: 16 single channels — **Out**: AMB

### SP4T Smear

```{image} images/modules/SP4TSmear.svg
:alt: SP4T Smear
:class: module-panel
```

*Decorrelates the channels of a polyphonic signal.*

Makes the channels less alike without changing what each of them sounds like. On
an ambisonic signal it widens and blurs the image. It also works on any
polyphonic cable.

Two algorithms are available via the right-click menu:

- **Velvet noise** (default): spreads each channel over a few tens of
milliseconds through a sparse pattern of its own (no latency).
- **Time-frequency**: takes the channels apart band by band instead, which is
finer and lets transients pass untouched (~35 ms latency, more CPU-hungry).

For the velvet noise algorithm, a frequency threshold can be set via the
right-click menu: the bass frequencies below that threshold are left
untouched.

- **Width**: 4 HP
- **In**: polyphonic, AMOUNT CV — **Out**: polyphonic
- **Knob**: AMOUNT (0–1, default 0.5), with a CV input and a trim
