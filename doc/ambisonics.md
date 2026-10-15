# Ambisonics

```{image} images/AmbisonicLogo.svg
:width: 150px
:align: center
:alt: Ambisonics logo
```

## What is ambisonics?

Ambisonics encodes a full-sphere sound field using spherical harmonics expansion
({numref}`fig:sh`): it is a multi-channel audio encoding technique where each
channel carries a given harmonic function sampled at the audio rate (see
[below](ambisonics-interpret) for more details on how to interpret those
signals). This *scene-based* encoding contrasts with the usual *channel-based*
encoding (1 channel = 1 speaker) and *object-based* techniques such as Dolby
Atmos, where each object carries its own audio stream plus positional metadata.

```{figure} ../res/misc/spherical_functions.svg
:scale: 100 %
:name: fig:sh
:alt: spherical harmonics

Representation of spherical harmonic functions up to order 3.
```

Despite being less popular, ambisonics has multiple advantages that offer real
opportunities for sound design and audio synthesis:

- It is a relatively simple and non-proprietary technique.
- It is speaker-agnostic: the same encoded signal can be decoded in many
  different ways, e.g., to binaural or to any speaker layout, whether standard
  or arbitrary.
- The whole sound field can be processed in simple and efficient ways, like
  exact (lossless) field rotation or mirroring. It is an advanced panning tool.
- Applications go well beyond panning: for example, the omni vs. directional
  components of the sound field can be easily decoupled and processed
  independently.

It also has a few limitations:

- It is directional, far-field only.
- The spherical harmonic expansion is truncated at a given order, which sets
  the finite spatial resolution and sharpness of the encoded sound field. Lower
  orders are subject to spatial blur.

To each order *N* corresponds a fixed number *(N+1)²* of ambisonic audio
channels: 1st order = 4, 2nd order = 9, 3rd order = 16 ({numref}`fig:sh`).

### Ambisonics vs. 3D spatial audio

The 1st limitation above deserves a bit more explanation. Standard ambisonics
indeed does not natively integrate the concept of distance to a sound source.
Instead, it assumes that the sound is coming from all directions to the center
of the field as plane waves. This makes the maths simpler: a full-sphere sound
field can then be represented simply by real spherical harmonics expansion. A
sphere is 3D geometric object but spherical harmonics functions are defined on
the *surface* of the sphere; it may therefore be more accurate to define
ambisonics as a "2.5D" technique rather than a full 3D spatial audio technique.

A basic encoder converting point sources into an ambisonic signal takes only
the *directions* (azimuth, elevation) to these sources. It is common, however,
that encoders integrate ambisonics with other kinds of pre-processing algorithms
to simulate things like proximity effect, wall reflections, doppler effect for
moving sources, etc.

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

(ambisonics-interpret)=
## How to interpret the ambisonic channels?

A First-Order Ambisonic (FOA) signal has 4 channels W,X,Y,Z (the so-called
*B-format*). The table below follows the ACN channel-ordering convention.

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
normalization — to the familiar "mid-side" representation of a stereo signal.
Ambisonics may indeed be viewed as a generalization of the "mid-side" concept to
3D spatial audio!

Subsequent channels, if any, represent Higher-Order Ambisonics (HOA). They are
harder to interpret, since they correspond to more complex harmonics, most of
which contain information across more than one axis ({numref}`fig:sh`).
