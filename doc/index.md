# Nun4tak SP4T

A [VCV Rack](https://vcvrack.com/) plugin providing
[ambisonic](https://en.wikipedia.org/wiki/Ambisonics) 3D spatial audio modules.

Ambisonic input/output signals use the ACN channel ordering and SN3D
normalization conventions (ambiX), unless otherwise specified. They are carried
between modules by polyphonic cables: 4 channels for 1st order, 9 for 2nd, 16
for 3rd.

```{toctree}
:maxdepth: 3

ambisonics
modules
```

## Code repository

The source lives at
[github.com/nun4tak/sp4t-vcv-plugin](https://github.com/nun4tak/sp4t-vcv-plugin),
under the GNU General Public License v3. Bugs, questions and ideas are welcome
in the [issue
tracker](https://github.com/nun4tak/sp4t-vcv-plugin/issues).

## Acknowledgement

Most of the ambisonic DSP in these modules relies on the [Spatial Audio
Framework](https://github.com/leomccormack/Spatial_Audio_Framework) (dual ISC /
GNU GPLv2 licence), which also powers the
[SPARTA](https://leomccormack.github.io/sparta-site/) audio plugins. Many thanks to
its developers for making it publicly available!
