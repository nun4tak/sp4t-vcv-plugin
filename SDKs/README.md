# SDKs

Third-party SDKs and dependencies vendored under this directory.

## Spatial Audio Framework

[`Spatial_Audio_Framework`](Spatial_Audio_Framework) is vendored as a git
submodule (https://github.com/leomccormack/Spatial_Audio_Framework).

### Why it's patched

SAF's `framework/CMakeLists.txt` always builds a universal `arm64;x86_64`
binary on macOS, and (when `SAF_ENABLE_SIMD` is enabled) applies a single
`-march=native` to both architecture slices. `-march=native` targets only the
host CPU, so it breaks compilation of whichever slice isn't the host
architecture. Also, SAF's SIMD fallback path only has x86 (SSE/AVX)
intrinsics implemented — there is no NEON equivalent — so `SAF_ENABLE_SIMD`
can never be valid while compiling the arm64 slice, regardless of
`-march`.

[`spatial-audio-framework-universal-simd.patch`](spatial-audio-framework-universal-simd.patch)
fixes this by passing per-architecture `-march` baselines (via
`-Xarch_arm64`/`-Xarch_x86_64`) and only defining `SAF_ENABLE_SIMD` for the
x86_64 slice; the arm64 slice already gets vectorised linear algebra from
Apple Accelerate regardless. It also scopes these flags to the `saf` target
(`PRIVATE`) instead of leaking them globally or into consumers of the
library.

### How to apply it

If the submodule is updated to a newer upstream commit (e.g. via `git submodule
update --remote`), try re-apply the patch:

```sh
cd SDKs/Spatial_Audio_Framework
patch -p1 < ../spatial-audio-framework-universal-simd.patch
```

## HIIR

[`hiir`](hiir) is [HIIR](http://ldesoras.free.fr/prod.html) 1.30 by Laurent de
Soras, an oversampling and Hilbert transform library, under the WTFPL
(`hiir/license.txt`). Copied in rather than added as a submodule: upstream is
distributed as a zip. These files come from the header-only fork at
<https://github.com/unevens/hiir>, which is the same 1.30 with the
`PolyphaseIir2Designer` implementation moved into its header.

`sp4t::UhjEncoder` uses `hiir::PhaseHalfPiFpu` for the pair of allpass chains
that give it its 90° phase shift, so only the files that one needs are here,
not the whole library:

    def.h  fnc.h  fnc.hpp  StageProcFpu.h  StageProcFpu.hpp
    PhaseHalfPiFpu.h  PhaseHalfPiFpuTpl.h  PhaseHalfPiFpuTpl.hpp
    PolyphaseIir2Designer.h

The SIMD implementations are left out: `PhaseHalfPiFpu` is 16 allpass sections
per sample and the plugin runs three of them.

Its headers include each other as `"hiir/<file>"`, so `CMakeLists.txt` puts
this directory (not `SDKs/hiir`) on the plugin's include path, and the plugin's
own sources include it the same way.

### Local change

`PhaseHalfPiFpuTpl.hpp` calls `assert()` without including `<cassert>`, which
does not compile on its own. The include was added, marked with a comment in
the file.
