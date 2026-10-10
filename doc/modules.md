# SP4T Modules

The modules are grouped by what they do to a signal: encoded into a sound
field, decoded back for listening, processed as a whole, or worked on channel
by channel.

Every jack labelled **AMB** carries a whole ambisonic signal over a polyphonic
cable, with as many channels as the order it was encoded at. A knob with a trim
knob and a jack next to it takes CV: the attenuated CV is added to the knob.

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

Places a source in the 3D space with the **AZ** (azimuth) and **ELEV**
(elevation) knobs. The source is mono when only the left input is patched, and
stereo when both are: **SPREAD** then sets the angle between each of the two
sources and the direction the knobs point at. The screen shows where the
sources sit.

- **In**: L (mono), R — **Out**: AMB
- **Knobs**: AZ (±180°), ELEV (±90°), SPREAD (0–90°, default 45°), each with a
  CV input and a trim
- **Switches**: ambisonic order of the output (1, 2, 3); azimuth direction —
  `+` follows the ambisonic convention, where a positive angle turns the source
  to the left, `-` flips it so that turning the knob right moves the source
  right
- **Right-click**: view mode of the screen

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

One column per source, each with its own **AZ** and **ELEV** knobs and CV
inputs. The **OMNI** input adds a source with no direction of its own, spread
evenly in every direction.

Each CV row is normalled from left to right: a jack with nothing patched takes
the signal of the nearest patched jack on its left, so one CV can steer several
sources, each through its own knob. The triangles between the jacks show it.

- **In**: 4 sources, OMNI — **Out**: AMB
- **Knobs**: AZ (±180°) and ELEV (±90°) per source, each with a CV input
- **Switch**: ambisonic order of the output (1, 2, 3)
- **Right-click**: view mode of the screen, flip azimuth sign

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

Two virtual speakers in front of the listener, in the horizontal plane.
**SPREAD** is the angle of each from the centre: 0° makes both outputs the
same (mono), 90° is hard left/right.

Cheap on CPU, and basic as decoders go. For headphones, use Ears Out instead.

- **In**: AMB, SPREAD CV — **Out**: L, R
- **Knob**: SPREAD (0–90°, default 45°), with a CV input and a trim
- **Right-click**: crude mode — almost free, using only the W and Y channels
  (plain mid/side) and ignoring the rest

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
head tracking of its own: put Rotate before it to turn the scene.

- **In**: AMB — **Out**: L, R
- **Right-click**: decoding method (least-squares by default, plus
  diffuse-field EQ, spatial resampling, time-alignment and magnitude
  least-squares), max-rE weighting, diffuse-field covariance matching,
  truncation EQ, and the latency of the filters

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

An exact, lossless rotation of everything encoded in the input, around the
three axes:

| Angle | Axis | What it does | Range |
|---|---|---|---|
| **Yaw** | Z (vertical) | turns the scene left/right | ±180° |
| **Pitch** | Y (left–right) | tilts the scene up/down | ±90° |
| **Roll** | X (front–back) | tilts the scene sideways | ±180° |

The three CV inputs are normalled from left to right, so one CV can drive all
three angles, each through its own attenuverter and `+`/`-` direction switch.

- **In**: AMB — **Out**: AMB
- **Knobs**: YAW, PITCH, ROLL, each with a CV input, a trim and a direction
  switch
- **Screen**: the rotated axes

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
(top–bottom) axis. Flips combine, and **DRY/WET** blends the result with the
original field — through an equal-power or a through-zero crossfade, whichever
the switch selects.

- **In**: AMB, one gate/trigger CV per axis — **Out**: AMB
- **Buttons**: X, Y, Z
- **Knob**: DRY/WET, with a CV input and an attenuverter
- **Switches**: crossfade curve; whether the buttons and CV inputs are
  momentary (gate) or latching (trigger, the default)

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

The 16 ACN channels are laid out as a grid, read from top-left to
bottom-right, each with its own gain and LED. Mostly for convenience and for
experimenting: changing single channels is not a spatially meaningful operation
in general.

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

The same grid as Gain, with a button per channel instead. Muting fades rather
than cuts, so it does not click.

- **In**: AMB, one gate/trigger CV per channel — **Out**: AMB
- **Buttons**: 16 mutes
- **Switch**: whether the buttons and CV inputs are momentary (gate) or
  latching (trigger, the default)

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

The input is split in two: the omni-directional channel goes through the top
send and return, every directional channel through the bottom ones, and the two
are merged back into the output. Patch an effect into either loop to process
one without touching the other — reverb on the omni part only, say. A send with
nothing patched into its return passes straight through.

Each loop has a gain before and after the effect, and a delay compensation in
samples: set it to the latency of the effect in that loop, and both the dry
signal and the other loop are held back to match.

- **In**: AMB, two returns — **Out**: AMB, two sends
- **Knobs** per loop: PRE gain (0–2x, with a CV input), POST gain (0–2x),
  DELAY (0–4096 samples)
- **Knob**: DRY/WET, with a CV input and an attenuverter
- **Right-click**: the latency the inserts introduce

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

What VCV's [Split](https://library.vcvrack.com/Fundamental/Split) does, with a
panel laid out for ambisonics: the 16 ACN channels as a grid, from top-left to
bottom-right. Channels the input does not carry stay at 0 V.

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

The counterpart of Split, and VCV's
[Merge](https://library.vcvrack.com/Fundamental/Merge) with an ambisonic panel.
The switch sets the order of the output; inputs with nothing patched are 0 V,
and inputs beyond the chosen order are ignored.

- **In**: 16 single channels — **Out**: AMB
- **Switch**: ambisonic order of the output (1, 2, 3)

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

Randomised time-frequency delays and cascaded all-pass filters make the
channels less alike without changing what each of them sounds like. On an
ambisonic signal it widens and blurs the image; it works on any polyphonic
cable.

- **In**: polyphonic, AMOUNT CV — **Out**: polyphonic
- **Knob**: AMOUNT (0–1, default 0.5), with a CV input and a trim
- **Right-click**: bypass transients (sends them straight through instead of
  smearing them), compensate level, and the latency it introduces
