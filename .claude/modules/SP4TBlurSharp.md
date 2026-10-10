# SP4TBlurSharp Module

## Description

Ambisonic Blur/Sharpen

Either blurs or sharpens an ambisonic signal. Controls the apparent spatial
resolution of the sound field without changing the ambisonic order (order of
input = order of output).

Note: blur compensates for the energy it takes out of the higher degrees,
though not in full (see below); sharpen does not compensate at all.

## Specifications

- Width: 4 HP
- 1 ambisonic jack input 
- 1 ambisonic jack output 
- 2 Knobs
  - blur/sharpen amount (-1 to +1, default 0.0)
- 1 small but regular knob: slope (0.25 to L ambisonic order)
- 1 small knob (trim): CV attenuverter for amount
- 2 CV jack inputs:
  - amount (bipolar)
  - slope (unipolar)

## Panel design

The design of the panel is done in the SP4TBlurSharp SVG files.

## Implementation

The Ambisonic order of the input signal is given by the number of channels of
the polyphonic cable (4, 9 or 16).

Controls:

- **Amount** a ∈ [−1, +1]
  - −1 (full left): 100% blur.
  - 0 (middle): the signal passes unchanged.
  - +1 (full right): 100% sharpen.
- **Slope** τ ∈ [0.25, L]
  - Width of the fade between degrees on the blur side. Small values give nearly
    step-like muting, large values a soft progression.
  - Default: τ = 1. At L = 1, τ has no audible effect.
  - Sharpening uses a fixed ramp and ignores τ. The Slope knob can be greyed out
    when Amount > 0.

Principle:

The module applies one real gain g_l per degree l = 0 … L, shared by all 2l + 1
channels of that degree. With ACN channel ordering, channel n belongs to degree
l = ⌊√n⌋:

  out[n] = g_⌊√n⌋ · in[n],  n = 0 … (L + 1)² − 1

This makes the process:

- a linear, rotation-invariant filter on the sphere that acts on the whole sound field (direct and diffuse alike);
- independent of any downstream decoder, though it stacks with any per-degree
  weighting the decoder applies (e.g. max-rE);

### Blur side (a < 0)

Set α = −a ∈ (0, 1]. The gains are *nearly* energy-preserving: the shape is,
the level it is applied at is held back (step 4).

1. **Moving cutoff:**
   x = L − α · (L + τ − 1)
2. **Soft mute weight** for each degree l = 0 … L:
   u_l = clip((x − l) / τ + 1, 0, 1)
   w_l = u_l² · (3 − 2u_l)  (smoothstep)
3. **Energy normalization:**
   n = √( (L + 1)² / Σ_k (2k + 1) · w_k² )
4. **Softened normalization:**
   b = B ^ ( ln n / ln (L + 1) ),  B = √2 (+3 dB)
   g_l = w_l · b

Behavior:

- At α → 0 every gain is 1.
- At α = 1 only W remains. Exact energy preservation (step 3 alone) would give
  it a gain of L + 1: +6 dB at L = 1, +9.5 dB at L = 2, +12 dB at L = 3. That
  is correct for a recorded, diffuse sound field, but it clips readily on the
  synthetic material a modular patch carries, where the energy taken out of the
  higher degrees was never there to begin with.
- Step 4 is why: n spans 1 … L + 1, and it is remapped onto 1 … B, the same
  ceiling whatever the order. Only the common factor changes, so the balance
  between the degrees — the blur itself — is untouched; the compensation is
  partial, monotonic in α, and still 1 at α = 0.
- No setting of the module, either side, can then make any channel louder than
  +3 dB. A signal at the 5 V audio standard peaks at 7.1 V, inside Rack's ±10 V.
- W is never muted (w_0 = 1 throughout), so step 3 never divides by zero, and
  n ≥ 1 always, so step 4 never attenuates.
- The highest degrees fade first, and the surviving degrees rise together.
- B is the one number to turn if full blur should sit louder or quieter.

The weights are inspired by Carpentier's ambisonic blur (AES Convention 142,
2017), but they are a simplified version, not his exact curves.

See https://www.sekisushai.net/ambitools/docs/blur.html

### Sharp side (a > 0)

Set s = s_max · a, s_max = 0.7 (see below). High-pass ramp over degree, without
energy normalization:

  g_l = (1 − s) + s · l / L

Behavior:

- At s → 0 every gain is 1.
- At s = 1 the gains are l / L, so W is removed completely (a true high-pass).
- The maximum gain is always 1, on the top degree, so the sharp side can't clip
  or raise the noise floor of the highest order. It only shifts the balance
  between orders.
- Overall level drops as s rises, mostly through W, as expected from a high-pass.
- For L = 3, s = 1, the −3 dB width of a single source narrows from 47.6° to
  42.3°, while the highest side lobe rises from −12 dB to −7.7 dB.
  - The sweet spot is roughly s ≈ 0.3–0.6.
  - To keep full right within that range, s is scaled as s = s_max · a with
    s_max = 0.7, which is what the module implements. This also keeps W present,
    so full right is still undoable and the level does not collapse.
- The process can be undone (by dividing by the gains) while s < 1. At s = 1 it can't, because W is gone.

### Example gains, L = 3, τ = 1

As implemented, i.e. with the softened normalization (B = √2) and s_max = 0.7.
The two right-hand columns are the same gains before those two are applied, for
comparison: the blur rows differ by one common factor per row, the sharpen rows
reach further.

| Amount | g₀ | g₁ | g₂ | g₃ | exact / s_max = 1 |
|---|---|---|---|---|---|
| −100% | 1.41 | 0 | 0 | 0 | 4.00, 0, 0, 0 |
| −75% | 1.23 | 1.03 | 0 | 0 | 2.26, 1.91, 0, 0 |
| −50% | 1.15 | 1.15 | 0.57 | 0 | 1.75, 1.75, 0.87, 0 |
| −25% | 1.07 | 1.07 | 1.07 | 0.17 | 1.32, 1.32, 1.32, 0.21 |
| 0 | 1 | 1 | 1 | 1 | 1, 1, 1, 1 |
| +25% | 0.82 | 0.88 | 0.94 | 1 | 0.75, 0.83, 0.92, 1 |
| +50% | 0.65 | 0.77 | 0.88 | 1 | 0.50, 0.67, 0.83, 1 |
| +100% | 0.30 | 0.53 | 0.77 | 1 | 0, 0.33, 0.67, 1 |

### Practical notes

- **Smoothing:** ramp the gains over a few milliseconds whenever Amount or Slope
  changes, to avoid zipper noise, especially on W and degree 1.
- **Continuity:** both sides give unity gains at the center, so crossing zero is
  seamless.
- **Headroom:** no setting can make any channel louder than +3 dB (full blur,
  any order), and the sharpen side never goes above unity. That ceiling is the
  point of the softened normalization: exact energy preservation would reach
  +12 dB on W at L = 3, which clips synthetic material.
- **Level:** full blur is therefore quieter than an exactly energy-preserving
  blur would be, and full sharpen is quieter still, as a high-pass should be. A
  separate makeup gain control would let users choose level-neutral behavior
  (not on the panel, kept for later).
