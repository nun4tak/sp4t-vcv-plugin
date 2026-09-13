# Nun4tak-SP4T

VCV Rack plugin providing Ambisonics 3D spatial audio modules.

## Requirements

- [VCV Rack SDK](https://vcvrack.com/Rack#sdk) 2.6.6, unpacked as
  `SDKs/Rack-SDK` (not vendored in this repo)
- CMake 3.19 or newer and a C/C++ toolchain
- Optional: [pixi](https://pixi.sh), which provides CMake and the task shortcuts
  below

Download the SDK for your platform, replacing `mac-arm64` with `mac-x64`,
`lin-x64` or `win-x64` as needed:

```sh
curl -L -o Rack-SDK.zip https://vcvrack.com/downloads/Rack-SDK-2.6.6-mac-arm64.zip
unzip -d SDKs Rack-SDK.zip
```

## Configure, build, install

```sh
pixi run configure   # cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
pixi run build       # cmake --build build
pixi run install     # cmake --install build
pixi run dist        # cmake --build build --target plugin_dist
pixi run clean       # cmake -E rm -rf build
```

`build` and `dist` configure first, and `install` builds first, so
`pixi run install` is enough from a clean checkout. Without pixi, run the CMake
commands shown in the comments directly.

`build` produces a ready-to-load plugin directory at `build/Nun4tak/`. `install`
copies it into the Rack user plugin folder, which defaults to
`~/Library/Application Support/Rack2/plugins-mac-arm64` on macOS,
`$XDG_DATA_HOME/Rack2/plugins-lin-x64` on Linux and
`%LOCALAPPDATA%/Rack2/plugins-win-x64` on Windows. Restart Rack afterwards.
`dist` writes a distributable `.vcvplugin` archive to `build/dist/`.

If the SDK lives elsewhere, pass `-DRACK_SDK_DIR=<path>` when configuring. To
install to a different folder, pass `-DCMAKE_INSTALL_PREFIX=<path>`. For a debug
build, configure with `-DCMAKE_BUILD_TYPE=Debug`.
