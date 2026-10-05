# SP4TSmear Module

## Description

Decorrelator.

A simple multi-channel (I/O polyphonic cables) signal decorrelator based on
randomised time-frequency delays and cascaded all-pass filters.

## Specifications

- Width: 4 HP
- 1 Knob
  - decorrelation amount (0 to 1, default 0.5)
- 1 small knob (trim): CV attenuator for amount
- 1 CV jack inputs:
  - amount (unipolar)
- 1 jack input (polyphonic) 
- 1 jack output (polyphonic) 
- 2 right-click menu options, both boolean and disabled by default:
  - bypass transients: the transients are sent straight to the output instead
    of through the decorrelator, which smears them
  - compensate level

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
