# Ambisonics

```{image} images/AmbisonicLogo.svg
:width: 150px
:align: center
:alt: Ambisonics logo
```

## What is ambisonics?

Ambisonics is a full-sphere, speaker-independent spatial audio format.

It was proposed several decades ago, although it was mostly overlooked until
recently. Today, it is primarily used in VR audio applications.

### Generalization of mid/side encoding

If you are already familiar with Mid/Side (M/S) stereo processing, First-Order
Ambisonics (FOA) is the exact same concept generalized to 3D space:

- The "Mid" becomes an omnidirectional component (W) capturing overall sound
  pressure.
- The "Sides" become three bidirectional (figure-of-eight) components along each
  axis: *front-rear* (X), *left-right* (Y), and *top-bottom* (Z).

See [below](ambisonics-interpret) for more details.

(ambisonics-order)=
### Spherical harmonics and ambisonic order

Ambisonics can go beyond the three primary X, Y, Z axes mentioned above to
encode a full-sphere sound field centered at a listening point, capturing finer
spatial detail through Higher-Order Ambisonics (HOA).

The core idea of ambisonics is to represent the sound field using **spherical
harmonics expansion** ({numref}`fig:sh`): each ambisonic channel carries a given
harmonic function sampled at the audio rate, which represents either the acoustic
pressure or one of its spatial derivatives at the listening point.

```{figure} ../res/misc/spherical_functions.svg
:scale: 100 %
:name: fig:sh
:alt: spherical harmonics
:class: custom-figure-spacing

Representation of spherical harmonic functions up to order 3. You can think these
function shapes as (virtual) microphone polar patterns, where the bright vs. dark
colors represent positive vs. inverted polarity.
```

As illustrated in {numref}`fig:sh`, spherical harmonics are organized as
hierarchical **orders**:

- 0th order: represents the overall sound pressure (omnidirectional)
- 1th order: represents the pressure gradients along the primary X,Y,Z axes
- 2nd and higher orders: represent increasingly complex and detailed directional patterns 

Since we cannot use an infinite number of audio channels to carry an ambisonic
signal, the spherical harmonics expansion has to be truncated at a given order
`N` that corresponds to a fixed number `(N+1)²` of ambisonic channels:

- 1st order: 4 channels
- 2nd order: 9 channels
- 3rd order: 16 channels
- and so on...

Increasing the order `N` increases the level of details of the sound field
carried by the ambisonic signal but also drastically increases the number of its
channels, which involves a tradeoff between spatial "sharpness" and
computational load.

```{note}
Ambisonics' core principle of representing any complex 3D sound field using spherical harmonics
expansion is very similar to the principle of representing any complex audio waveform as an
expansion of simple sine waves (Fourier series).
```

(ambisonics-speaker-agnostic)=
### Scene-based, speaker-agnostic encoding

Ambisonics is a **scene-based** encoding technique, which greatly differs from
the usual **channel-based** encoding (1 channel = 1 speaker) and which also
contrasts with **object-based** immersive techniques such as Dolby Atmos (i.e.,
each object carries its own audio stream plus positional metadata).

Ambisonic channels aren't intended for direct playback; instead, they need to be
decoded into a channel-based format first (just like Mid/Side encoded stereo
channels need to be decoded back to Left/Right channels). This constraint is
also one of the greatest advantages of the ambisonic format: its representation
of the 3D sound field is **speaker-independent**, i.e., the same ambisonic
signal can be decoded into any channel-based format such as L/R stereo speakers,
binaural, quadraphonic or 5.1 surround systems, dome of speakers, etc.

Likewise, the audio signals captured by an array of microphones placed at a
listening point do not directly translate into an ambisonic signal. Those raw
audio signals first need to go through an ambisonic encoder.

(ambisonics-vs-3d)=
### Ambisonics vs. real "3D" spatial audio

It is worth noting that standard ambisonics does not natively integrate the
concept of distance to a sound source. Instead, it assumes that the sound is
coming from all directions to the listening point as plane waves. This makes the
maths simpler: a full-sphere sound field can then be represented simply by real
spherical harmonics expansion. A sphere is 3D geometric object but spherical
harmonics functions are defined on the *surface* of the sphere; it might
therefore be more accurate to define ambisonics as a "2.5D" technique rather
than a full 3D spatial audio technique.

A basic encoder converting point sources into an ambisonic signal takes only the
**directions** (azimuth, elevation) to these sources. It is common, however,
that encoders integrate ambisonics with other kinds of pre-processing algorithms
to simulate things like proximity effect, wall reflections, doppler effect for
moving sources, etc.

## Ambisonics format standards

Ambisonics can be used as an open standard to pass an encoded 3D sound field
through multiple processing units and software.

There has been multiple proposed conventions, which differ in how the channels
are ordered and how the channel signal gains are normalized relatively to each
other.

This "SP4T" VCV Rack plugin uses the **AmbiX** (Ambisonic Exchange) convention,
which is the most recent and common one (it is also the one that best supports
high-order ambisonic signals). AmbiX uses **ACN** (Ambisonic Channel Numbering)
and **SN3D** normalization.

(ambisonics-interpret)=
## How to interpret the ambisonic channels?

A First-Order Ambisonic (FOA) signal has 4 channels W,X,Y,Z -- i.e., the so-called
*B-format* obtained after encoding raw microphone signals (*A-format*).

The table below follows the ACN channel-ordering convention.

| Channel (ACN) | Order | Axis | Direction | Plot |
|---|---|---|---|---|
| 0 | 0th | (W) | isotropic / omnidirectional | <img src="../res/misc/b_format_W.svg" width="50px"> |
| 1 | 1st | Y   | left–right | <img src="../res/misc/b_format_Y.svg" width="50px"> |
| 2 | 1st | Z   | top–bottom | <img src="../res/misc/b_format_Z.svg" width="30px"> |
| 3 | 1st | X   | front–rear | <img src="../res/misc/b_format_X.svg" width="50px"> |

In idealized settings, the first channel corresponds to the sound that would be
picked up by an omnidirectional microphone, while the other three channels
relate to what would be picked up by figure-8 microphones oriented along the
X,Y,Z axes.

The first two ACN channels W,Y are thus equivalent — up to some gain
normalization — to the familiar mid/side representation of a stereo signal.

Subsequent channels, if any, represent Higher-Order Ambisonics (HOA). They are
harder to interpret, since they correspond to more complex harmonics, most of
which contain information across more than one axis ({numref}`fig:sh`).

## Advantages and limitations

Ambisonics has multiple advantages that offer real opportunities for sound
design and audio synthesis:

- It is a relatively simple and non-proprietary technique.
- It is speaker-agnostic: the same encoded signal can be decoded in many
  different ways, e.g., to binaural or to any speaker layout, whether standard
  or arbitrary (see [above](ambisonics-speaker-agnostic)).
- The whole sound field can be processed in simple and efficient ways, like
  exact (lossless) field rotation or mirroring. It is an advanced panning tool.
- Applications go well beyond panning: for example, the omni vs. directional
  components of the sound field can be easily decoupled and processed
  independently (just like Mid/Side processing).

It also has a few limitations:

- It is directional, far-field only (see [above](ambisonics-vs-3d)).
- The spherical harmonic expansion is truncated at a given order (see
  [above](ambisonics-order)), which sets the finite spatial resolution and
  sharpness of the encoded sound field. Lower orders are subject to spatial
  blur.

## Why ambisonics in modular synthesis / VCV Rack?

The point of ambisonics in a modular context goes well beyond immersive-audio
playback. Because each ambisonic channel is just an ordinary audio signal, the
whole sound field becomes something that can be patched, modulated, processed
and routed like anything else on the rack (note: ambisonic channels do not carry
"speaker" signals, and some operations stay spatially meaningful only when
they are applied consistently across the whole channel set).

There is no obligation to represent a realistic scene: the ambisonic channels
can just as well carry different sound *textures* mapped across a sphere,
turning spatial operations into a powerful sound-design tool. And why not try
using it with CV signals? There's no limitation, really.

Modular synthesis has the right workflow for exploring the full range of
ambisonic possibilities. VCV Rack's polyphonic cables also make it very
convenient to carry ambisonic signals between modules (up to 3rd order = 16
channels, hopefully more in the future?).

This VCV Rack plugin aims to provide a comprehensive set of modules for
typical ambisonic operations such as **encoding** one or more sources into a 3D
sound field — or directly **synthesizing** the sound field — **rotating,
flipping, decoupling and processing** that field, and finally **decoding** it
to a mono, stereo or binaural signal or to any array of speakers. In the spirit
of modular synthesis, almost anything can be controlled and CV modulated!
