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
