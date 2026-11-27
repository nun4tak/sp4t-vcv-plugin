# SP4T Modules

The modules are grouped by what they do to a signal: encode it into a sound
field, decode it back for listening, process it as a whole, or work on it
channel by channel.

Every jack labelled **AMB** carries a whole ambisonic signal over a polyphonic
cable with as many channels as the order it was encoded at (1st order = 4, 2nd
order = 9, 3rd order = 16).

## Encoders

Turn ordinary audio into an ambisonic sound field.

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

Turn a sound field back into something to listen to.

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

## Processors

Act on the sound field as a whole, keeping it a sound field.

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

Work on the channels a sound field is made of, or on any polyphonic cable.

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
