# SP4TXY-MSOut Module

## Description

Decodes the sound field to stereo with a pair of virtual coincident microphones
(X/Y or M/S).

The module renders an Ambisonic input signal (polyphonic cable) to stereo as if
it had been recorded with a pair of coincident microphones placed at the centre
of the scene, in the horizontal plane. In X/Y mode, two identical microphones
are angled apart by SPREAD. In M/S mode, a forward-facing mid microphone is
combined with a sideways (left-facing) figure-8, and SPREAD sets their balance,
i.e., the stereo width. The PATTERN knob sweeps continuously from omni through
cardioid and hypercardioid to figure-8, so all the classic coincident techniques
(X/Y, Blumlein, M/S and their variants) are available from the same controls.

The module accepts any Ambisonic order but uses only the first-order horizontal
channels. This is exact, not a shortcut: a first-order microphone only picks up
the first-order part of the sound field. There is no orientation control: use
the Rotate module before this one to aim or tilt the pair. For a
loudspeaker-panning rendering, see Stereo Out; for headphones, see Ears Out. For
beams sharper than any real microphone (higher orders), see the beamformer
module (planned).

Naming: C++ `SP4TXYMSOut`, slug `SP4T-XY-MS-Out`, panel title
"SP4T · XY-MS OUT". Everywhere else (labels, screen, docs) the techniques are
written "X/Y" and "M/S".

## Specifications

- Width: 8 HP
- 2 knobs
  - PATTERN (0 to 1, default 0.5 = cardioid): weight of the figure-8 component
    of the microphone pattern, from 0 (omni) to 1 (figure-8). In M/S mode it is
    the pattern of the mid microphone only (the side microphone is always a
    figure-8). The tooltip shows the value followed by the pattern name when it
    matches one of the named patterns (see table below), e.g., "0.5 (cardioid)".
  - SPREAD (0 to 90 degrees, default 45 degrees)
    - X/Y mode: angle between each microphone and the front (X) axis
    - M/S mode: mid/side balance; 0 = mid only (mono), 45 = equal mid and side
      (classic M/S), 90 = side only
- 2 small knobs (trim): CV attenuverters for PATTERN and SPREAD
- 1 switch (2 positions): mode, "X/Y" (default) or "M/S"
- 2 CV jack inputs:
  - PATTERN
  - SPREAD
- 1 AMB Ambisonic jack input (polyphonic)
- 2 audio jack outputs:
  - L (left)
  - R (right)
- 4 LEDs in the static microphone drawing (one per capsule): X/Y left and right,
  M/S mid and side. Only the pair of the active mode lights up.
- Right-click menu options:
  - Diffuse-field level compensation (boolean, default: off)

### Named patterns (PATTERN values)

| Pattern       | PATTERN | Front/rear pickup                   |
|---------------|---------|-------------------------------------|
| Omni          | 0       | equal all around                    |
| Subcardioid   | 0.3     | front-weighted, no null             |
| Cardioid      | 0.5     | null at the rear                    |
| Supercardioid | 0.634   | nulls at ±125°, small inverted rear lobe |
| Hypercardioid | 0.75    | nulls at ±110°, larger inverted rear lobe |
| Figure-8      | 1       | nulls at the sides, rear lobe inverted |

### Usual techniques (for the module documentation)

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

General notes:

- All these techniques are coincident: L and R differ only in level and
  polarity, never in time, so there is no comb filtering in mono. Mono issues
  come only from inverted lobes (supercardioid, hypercardioid, figure-8), which
  partly cancel and make some sources or reverb sound phasey.
- In M/S mode, SPREAD never changes what reaches the mono sum (always the mid
  microphone, scaled down as SPREAD goes up), so the width can be adjusted
  without affecting mono compatibility. This is not the case in X/Y mode.
- Wider SPREAD and more directional patterns give a wider image with more room
  sound, but more inverted content.
- The virtual microphones have ideal, frequency-independent patterns (no
  off-axis colouration). With a recording from a real first-order microphone,
  the patterns are less accurate at high frequencies.
- For separate M/S processing downstream, use M/S mode at SPREAD 45° and any
  M/S encoder after the L/R outputs (L+R and L−R give back the mid and side
  signals at equal level).

## Panel design

The design of the panel is done in the SP4TXY-MSOut SVG files (prototype drawn
by hand). From top to bottom:

- Title.
- Dynamic screen (polar plot of the microphone patterns, see below), taking the
  width of the module minus the usual margins.
- Static microphone drawing below the screen, on the panel background rather
  than on a dark rectangle as the speaker and head drawings of Stereo Out and
  Ears Out are (those sit inside the screen of their module; here the screen
  above holds the patterns). Drawn in the ink the Flip icons use (#5c6370 on
  the light panel, #9da5ae on the dark one), 0.25 mm strokes, filled with the
  panel background so that the bodies read as crossing behind one another. Two
  subplots side by side, X/Y on the left (capsules at 5.79 and 8.19 mm) and
  M/S on the right (16.1 and 18.5 mm), each holding two microphones drawn as a
  capsule (circle) and a body (rounded rectangle). The two capsules of a pair
  overlap like binoculars, drawn as one outline (no arc inside the overlap):
  - X/Y: both bodies rotated by ±45°, crossing behind the capsules (left
    microphone on the left, pointing front-left).
  - M/S: mid upright (pointing front), side lying sideways pointing left, body
    to the right. Mid and side bodies have the same length, chosen so that the
    pair has the same height as the X/Y pair.
  - No label under the subplots: the screen names the active mode above each
    of them instead (see below).
  - One SP4TSignalLight at the centre of each capsule, over a dot of the ink
    of the drawing, as the speakers of Stereo Out have.
  - Reference artwork: `mic2-out-display-binoculars-c.svg` (170 x 95 units,
    LED centres at (36, 30), (49, 30), (109.5, 30) and (122.5, 30)), to be
    scaled to the panel.
- Mode switch (2 positions, labeled "X/Y" and "M/S"), close to the drawing.
- PATTERN and SPREAD columns, as in Stereo In: main knob (large) with arc and
  label above, trim, CV input, linked by thin lines.
- AMB input.
- L and R outputs side by side on a rounded rectangle area, as in Stereo Out.

### Screen

Raw microphone patterns in the horizontal plane, in both modes (no L/R result
view):

- Canvas: same settings as the horizontal (XY) plane of the default "XY/Z
  projections" view of Stereo In and Source4 In (`sp4t::AmbisonicDisplay`):
  outline circle, X axis up and Y axis left drawn as diameters with arrows and
  names at their positive ends, one graticule circle, same colors
  (`ScreenStyle.hpp`) and strokes. No Z axis. Front is up and left is left. The
  outline circle is gain 1, the graticule circle gain 0.5. All the text of the
  screen is at the label size of the Rotate screen (16 reference units; both
  panels give their screen the same width, so the two match).
- The circle takes all the room left by the X axis name above it, the Y axis
  name on its left and the two texts in the bottom corners (below), which are
  laid out for the longest string each can hold so that the plot does not
  resize as the controls are turned.
- X/Y mode: the two microphone patterns, left in Ayu blue (#59C2FF, as the "L"
  source of Stereo In) and right in Ayu red (#F07178, as "R").
- M/S mode: the mid pattern in Ayu orange (#FFB454, as the "M" source of Stereo
  In) and the side figure-8 in Ayu purple (#D2A6FF), each scaled by its SPREAD
  gain (cos and sin of SPREAD), so that the balance shows as the side lobes
  growing against the mid.
- Fixed scale (gain 1 = outline circle), so that level changes are visible.
- Inverted-polarity lobes dashed and drawn in the same color, slightly
  fainter. nanovg has no dash support, so the dashes are stroked one by one
  along the lobe.
- The name of the active mode ("X/Y" or "M/S") in the bottom left corner and
  the pattern in the bottom right corner, both in the Ayu orange the Rotate and
  Source4 In screens give to what their module is set to (the greys are for the
  canvas: axes, graticules, their names). The pattern is abbreviated (OMNI,
  SUB, CARD, SUPR, HYPR, FIG8) so that it can be drawn at the label size, and
  prefixed with "~" when the value does not match that named pattern exactly;
  the full name is what the knob tooltip shows.
- In the module browser (no module), draw the default setting (X/Y cardioid at
  45°).

## Implementation

Notation: W, Y, X are the input channels 0, 1 and 3 (ACN/SN3D), d the PATTERN
value (0..1), a = 1 − d, β the SPREAD angle.

- X/Y mode:
  - L = a·W + d·(cos β·X + sin β·Y)
  - R = a·W + d·(cos β·X − sin β·Y)
- M/S mode, with M = a·W + d·X and S = Y:
  - L = cos β·M + sin β·S
  - R = cos β·M − sin β·S

Both are a 2 x 3 gain matrix on (W, Y, X): implement them as a small reusable
kernel `sp4t::AmbiMicPairDecoder` (`src/AmbiMicPairDecoder.hpp/.cpp`) with the
same structure as `sp4t::AmbiStereoDecoder`: setters for mode, pattern and
spread (clamped), gains recomputed once every 32 samples and linearly
interpolated in between (no zipper noise when modulated, e.g., the sign change
of the Y gain), no allocation after construction. Higher-order input channels
are ignored; output silence when the input has fewer than 4 channels.

Controls, as in Stereo Out: updated every 16 samples; CV value = knob + trim ×
CV / 10 V × full range (1 for PATTERN, 90° for SPREAD), clamped to the knob
range.

Diffuse-field level compensation (menu option): multiplies both outputs by
1 / sqrt(P), P being the diffuse-field power of one output relative to an omni
microphone (SN3D: W has power 1, X and Y 1/3 each, uncorrelated):

- X/Y: P = a² + d²/3
- M/S: P = cos²β·(a² + d²/3) + sin²β/3

So loudness stays roughly constant when turning PATTERN (up to +4.8 dB for a
figure-8). Off by default: the plain microphone behaviour is the reference.

LEDs: same response as Stereo Out (peak since the last light update, full scale
at 5 V, cube-root curve, `setBrightnessSmooth`, 256-sample divider). In X/Y mode
they show L and R. In M/S mode they show the mid and side signals after the
SPREAD balance, which are derived from the outputs: (L + R) / 2 and (L − R) / 2.
The LEDs of the inactive mode stay off.

Save the menu option in `dataToJson()` / `dataFromJson()`; the mode is a panel
switch, hence a regular parameter.

### Spatial Audio Framework

SAF provides axisymmetric beam weights (`beamWeightsCardioid2Spherical()`,
`beamWeightsHypercardioid2Spherical()`, `beamWeightsMaxEV()`) and their
rotation to a direction (`rotateAxisCoeffsReal()`), as used in its `beamformer`
example. They only cover the discrete pattern families, work in N3D, and at
first order reduce to the closed-form gains above, so the kernel does not need
them. They are the natural basis for the planned beamformer module, and can
serve as a cross-check in a test: SAF's order-1 cardioid and hypercardioid,
rotated to ±β, converted to SN3D and normalized to unit on-axis gain, must
match the X/Y gains for d = 0.5 and d = 0.75 (and SAF's order-1 max-EV is close
to the supercardioid).
