# SP4TSmear Module

## Description

Decorrelator.

A simple multi-channel (I/O polyphonic cables) signal decorrelator.

Two engines, selected from the right-click menu:

- velvet noise (default): each channel is convolved with a sparse sequence of
  +1/-1 impulses of its own, a thousand or so per second over a few tens of
  milliseconds. No latency, and a few dozen additions per sample per channel.
  Below the bass split frequency (right-click menu, 200 Hz by default) the
  signal passes through untouched, since smearing the low end is what makes a
  decorrelator sound loose.
- time-frequency: randomised delays and cascaded all-pass filters in an afSTFT
  filterbank (SAF). Finer, with per-band control and a transient ducker, at the
  cost of a 1664-sample latency and of running all 16 channels whatever the
  input carries.

## Specifications

- Width: 4 HP
- 1 Knob
  - decorrelation amount (0 to 1, default 0.5)
- 1 small knob (trim): CV attenuator for amount
- 1 CV jack inputs:
  - amount (unipolar)
- 1 jack input (polyphonic) 
- 1 jack output (polyphonic) 
- Right-click menu options:
  - decorrelator: velvet noise (default) or time-frequency
  - bass split: off, 100, 200 (default), 400 or 800 Hz; velvet noise only
  - bypass transients: the transients are sent straight to the output instead
    of through the decorrelator, which smears them; time-frequency only,
    disabled by default
  - compensate level, disabled by default

## Panel design

The design of the panel is done in the SP4TSmear SVG files.

## Implementation

Look at the SAF example `decorrelator`.

The decorrelated signal is crossfaded with the input at constant power (cosine /
sine), not linearly: the two are largely incoherent, so a linear mix would dip
by 3 dB halfway through the travel.

The transients are bypassed with SAF's transient ducker: it splits the
time-frequency frame in two, only the residual is decorrelated and the transient
part is added back untouched. The level compensation applies to the decorrelated
part alone, since the bypassed transients still sum coherently.
