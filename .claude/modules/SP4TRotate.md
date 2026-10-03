# SP4TRotate Module

## Description

Ambisonic rotator.

The module takes an ambisonic input signal (polyphonic cable) and return the
rotated sound field as an output ambisonic signal.

Rotation is defined by the three angles "yaw", "pitch", "roll" rotating the
ambisonic sound field along the Z,Y,X axes, according to the standard ambisonics
coordinate convention (see the table below).

| Angle | Axis | What it does | Range |
|---|---|---|---|
| **Yaw** | **Z** (vertical axis) | Turns the scene left/right | −180° to +180° |
| **Pitch** | **Y** (left–right axis) | Tilts the scene up/down | −90° to +90° |
| **Roll** | **X** (front–back axis) | Tilts the scene sideways | −180° to +180° |

For convenience, each of the angles can be flipped using the +/- switches.

## Specifications

- Width: 8 HP
- 3 Knob
  - Yaw angle (-180 to 180 degrees, default 0 degrees)
  - Pitch angle (-90 to 90 degrees, default 0 degrees)
  - Roll angle (-180 to 180 degrees, default 0 degrees)
- 3 small knob (trim): CV attenuverters for each of the angles
- 3 CV jack inputs:
  - CV controls for each of the angles (bipolar)
- 1 AMB Ambisonic jack input (polyphonic) 
- 1 AMB Ambisonic jack output (polyphonic) 
- 1 display

## Panel design

The design of the panel is done in the SP4TRotate SVG files.

## Implementation

Look at the SAF example `rotator`.

The Ambisonic order of the input signal is given by the number of channels of
the polyphonic cable (4, 9 or 16).

The angle switches apply to both angle knob and CV controls.

Implemented in `src/AmbiRotator.cpp`. The sound field is rotated by
Rz(yaw) Ry(pitch) Rx(roll), with right-handed rotations in the ambiX frame: a
positive angle turns the field anticlockwise seen from the positive end of its
axis, so a yaw of +90 takes a source in front to azimuth +90 (toward the left,
as the azimuth convention of the plugin), a positive pitch takes it downward and
a positive roll takes the left side upward.

SAF's `yawPitchRoll2Rzyx()` builds the rotation of the *listener* instead, which
is what `rotator` wants for head tracking: it turns the scene the opposite way.
Its transpose is the rotation of the scene, and is exactly the product above.

Yaw and roll wrap to [-180, 180] and pitch is clamped to [-90, 90], so a CV
sweeping past a full turn keeps spinning the field instead of sticking at the
end of the knob range. Wrapping is seamless, an angle and the same angle plus a
full turn being the same rotation.

The spherical harmonic rotation matrix (SAF's `getSHrotMtxReal()`) is recomputed
once every 32 samples and its entries are linearly interpolated in between,
which avoids zipper noise when the angles are modulated; `rotator` crossfades
the output of the previous and the new matrix over a frame instead. The matrix
only mixes channels of the same order, so the scaling between N3D and SN3D
cancels out and the same matrix applies to an ambiX signal.

`AmbiRotator::rotatedAxes()` gives the screen the rotated axes from the same
code path that builds the audio matrix, so the two cannot disagree.

Two paths skip the matrix multiply. A rotation has no memory, so when the AMB
input or the AMB output is unplugged the work stops at once, with no tail to let
ring out; the interpolation is reset so that reconnecting starts from the angles
in force rather than gliding from a stale matrix. And when the angles are all
zero the input is routed straight to the output, which covers the default state
and a module patched in ready to be modulated later. `AmbiRotator::isIdentity()`
gates that one: it stays false until the interpolation has reached the identity,
so returning the angles to zero is not cut short, and the glide lands exactly on
its target rather than wherever adding the steps one by one ended up, which makes
the copy return exactly what the multiply would. The control rate keeps running
in both cases, so the screen still follows the knobs with nothing patched.

The display shows the 3D X,Y,Z original axes in grey (use the same settings than
the `AmbisonicDisplay::drawSphere3D`, without showing the sphere great circles
here), as well as the rotated X',Y',Z' axes using the `nvgRGB(0xff, 0xb4, 0x54)`
color.
