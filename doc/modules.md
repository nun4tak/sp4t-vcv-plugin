# SP4T Modules

The modules are grouped by what they do to a signal: encoded into a sound
field, decoded back for listening, processed as a whole, or worked on channel
by channel.

Every jack labelled **AMB** carries a whole ambisonic signal over a polyphonic
cable with as many channels as the order it was encoded at (1st-order = 4, 2nd
order = 9, 3rd order = 16).

## Encoders

Turn ordinary audio into an ambisonic sound field.

### SP4T Stereo In

```{image} images/modules/SP4TStereoIn.svg
:alt: SP4T Stereo In
:class: only-light module-panel
```

```{image} images/modules/SP4TStereoIn-dark.svg
:alt: SP4T Stereo In
:class: only-dark module-panel
```

*Ambisonic encoder for one panned mono or stereo source.*

Places either a mono point source or a pair of L/R stereo point sources in the
3D space with the **AZ** (azimuth) and **ELEV** (elevation) knobs. The source is
mono when only the left input is patched, and stereo when both are. In the
latter case, **SPREAD** sets the angle between each of the two L/R sources and
the "mono" direction that the knobs point at.

The switch next to the **AMB** output selects the order (1, 2, 3) of the encoded
ambisonic signal. Increasing the order encodes the sound field at a higher
spatial resolution, which results in more channels (4, 9, 16) carried by the
output polyphonic cable.

The switch below the **AZ** knob allows to flip the sign of the azimuth angle
for both the knob and input CV: `+` follows the ambisonic convention (a
positive angle turns the source to the left) while `-` may be more intuitive
(turning the knob right moves the source to the right).

The screen shows the position(s) - direction(s) - of the source(s). Different
view modes are available via the right-click menu.

- **Width**: 10 HP
- **In**: L (mono), R — **Out**: AMB
- **Knobs**: AZ (±180°), ELEV (±90°), SPREAD (0–90°, default 45°), each with a
  CV input and a trim

### SP4T Source4 In

```{image} images/modules/SP4TSource4In.svg
:alt: SP4T Source4 In
:class: only-light module-panel
```

```{image} images/modules/SP4TSource4In-dark.svg
:alt: SP4T Source4 In
:class: only-dark module-panel
```

*Ambisonic encoder for up to four point sources.*

One column per source (input at the bottom), each with its own **AZ** and
**ELEV** knobs and CV inputs. In addition, the **OMNI** input can be used to add
a omni-directional source (sound coming from everywhere in the 3D space).

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

The screen shows the positions - directions - of the sources. Different
view modes are available via the right-click menu.

- **Width**: 10 HP
- **In**: 4 sources, OMNI — **Out**: AMB
- **Knobs**: AZ (±180°) and ELEV (±90°) per source, each with a CV input

## Decoders

Turn a sound field back into something to listen to.

### SP4T Stereo Out

```{image} images/modules/SP4TStereoOut.svg
:alt: SP4T Stereo Out
:class: only-light module-panel
```

```{image} images/modules/SP4TStereoOut-dark.svg
:alt: SP4T Stereo Out
:class: only-dark module-panel
```

*Decodes the sound field to a pair of stereo speakers.*

The pair of virtual speakers are positioned in front of the listener, in the
horizontal plane. **SPREAD** corresponds to the angle between each speaker and
the X (front-rear) axis: 0° makes both outputs the same (mono), 90° is hard
left/right.

This is a basic decoder that is very cheap on CPU. Although it poorly capture
the whole 3D sound field, it may still be interesting for some sound design
experiments. For even cheaper computation and less accuracy, the "crude mode"
(available via the right-click menu) uses only the first two W,Y ambisonic
channels (i.e., mid/side decoding) and completely ignores the rest. For
headphones, you may want to use the [Ears Out](#sp4t-ears-out) module instead
(binaural decoding, more CPU hungry).

- **Width**: 4 HP
- **In**: AMB, SPREAD CV — **Out**: L, R
- **Knob**: SPREAD (0–90°, default 45°), with a CV input and a trim

### SP4T Ears Out

```{image} images/modules/SP4TEarsOut.svg
:alt: SP4T Ears Out
:class: only-light module-panel
```

```{image} images/modules/SP4TEarsOut-dark.svg
:alt: SP4T Ears Out
:class: only-dark module-panel
```

*Decodes the sound field to headphones (binaural).*

A binaural decoder, for listening to the whole field over headphones. It has no
head tracking of its own but you can use the [Rotate](#sp4t-rotate) module
before it to turn the scene.

Multiple decoding options are available via the right-click menu. Note that some
algorithms are included here for completeness but may not yield good results
and/or have more latency.

- **Width**: 4 HP
- **In**: AMB — **Out**: L, R

## Processors

Act on the sound field as a whole, keeping it a sound field.

### SP4T Rotate

```{image} images/modules/SP4TRotate.svg
:alt: SP4T Rotate
:class: only-light module-panel
```

```{image} images/modules/SP4TRotate-dark.svg
:alt: SP4T Rotate
:class: only-dark module-panel
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
:class: only-light module-panel
```

```{image} images/modules/SP4TFlip-dark.svg
:alt: SP4T Flip
:class: only-dark module-panel
```

*Mirrors the sound field across one or more axes.*

Each button flips the field across the X (front–rear), Y (left–right) or Z
(top–bottom) axis. The **DRY/WET** knob blends the result with the original
field using either an equal-power or a through-zero crossfade, whichever the
first switch selects.

The second switch selects the behavior of the buttons and CV inputs: MOM
(momentary / gate) or LATCH (latching / trigger, the default).

- **Width**: 6 HP
- **In**: AMB, one gate/trigger CV per axis — **Out**: AMB
- **Buttons**: X, Y, Z
- **Knob**: DRY/WET, with a CV input and an attenuverter

### SP4T Blur Sharp

```{image} images/modules/SP4TBlurSharp.svg
:alt: SP4T Blur Sharp
:class: only-light module-panel
```

```{image} images/modules/SP4TBlurSharp-dark.svg
:alt: SP4T Blur Sharp
:class: only-dark module-panel
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
filtering the frequencies of an audio signal, it consists here in "directional"
filtering of an ambisonic encoded sound field.

Blurring is achieved by fading the higher order ambisonic (HOA) channels, until
only the first W channel (omni-directional) is left. To prevent the output
signal getting too quiet, the gains of the lower order channels are increased as
the higher orders fade. A trade-off is made between full compensation
(preserving the energy of the sound field) and no compensation at all to also
avoid the output signal getting too hot and clipping.

The **SLOPE** control is used only when blurring ; it sets how wide the fade
between HOA channels is, from nearly step-like muting to a soft progression. It
has also no effect on a 1st-order ambisonic signal.

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
:class: only-light module-panel
```

```{image} images/modules/SP4TGain-dark.svg
:alt: SP4T Gain
:class: only-dark module-panel
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
:class: only-light module-panel
```

```{image} images/modules/SP4TMute-dark.svg
:alt: SP4T Mute
:class: only-dark module-panel
```

*A mute per ambisonic channel.*

The same grid as [Gain](#sp4t-gain), with one button per channel instead (muting
fades very quickly, avoiding clicks).

The switch selects the behavior of the buttons and CV inputs: MOM
(momentary / gate) or LATCH (latching / trigger, the default).

This module is mostly for convenience and for experimentation. Changing single
channels is not a spatially meaningful operation in general.

- **Width**: 10 HP
- **In**: AMB, one gate/trigger CV per channel — **Out**: AMB
- **Buttons**: 16 mutes

### SP4T Split Insert

```{image} images/modules/SP4TSplitInsert.svg
:alt: SP4T Split Insert
:class: only-light module-panel
```

```{image} images/modules/SP4TSplitInsert-dark.svg
:alt: SP4T Split Insert
:class: only-dark module-panel
```

*Two effect inserts, one for the omni part of the field and one for the rest.*

The input is split in two components: the omni-directional component
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
:class: only-light module-panel
```

```{image} images/modules/SP4TSplit-dark.svg
:alt: SP4T Split
:class: only-dark module-panel
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
:class: only-light module-panel
```

```{image} images/modules/SP4TMerge-dark.svg
:alt: SP4T Merge
:class: only-dark module-panel
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
:class: only-light module-panel
```

```{image} images/modules/SP4TSmear-dark.svg
:alt: SP4T Smear
:class: only-dark module-panel
```

*Decorrelates the channels of a polyphonic signal.*

Makes the channels less alike without changing what each of them sounds like. On
an ambisonic signal it widens and blurs the image. It also works on any
polyphonic cable.

Two algorithms are available via the right-click menu:

- **Velvet noise** (default): spreads each channel over a few tens of
milliseconds through a sparse pattern of its own (no latency).
- **Time-frequency**: takes the channels apart band by band instead, which is
finer and lets transients pass untouched (~35 ms latency, more CPU hungry).

For the velvet noise algorithm, a frequency threshold can be set via the
right-click menu: the bass frequencies below that threshold are not affected by
this effect.

- **Width**: 4 HP
- **In**: polyphonic, AMOUNT CV — **Out**: polyphonic
- **Knob**: AMOUNT (0–1, default 0.5), with a CV input and a trim
