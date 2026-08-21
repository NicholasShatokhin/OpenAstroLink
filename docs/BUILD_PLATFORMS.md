# Build platforms — v0.2.9

## Raspberry Pi 4 — primary observatory deployment

64-bit Raspberry Pi OS / Debian-family is the target. Native QHY, Canon EOS, Gemini and Sky-Watcher are built as ABI-v2 native plugins in the RPi native preset; INDI remains optional compatibility support for other equipment.

```bash
cmake -S . -B build-rpi -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOAS_BUILD_GUI=ON \
  -DOAS_BUILD_NODE=ON \
  -DOAS_BUILD_NATIVE_REFERENCE_DRIVERS=ON \
  -DOAS_ENABLE_QHY=ON \
  -DOAS_ENABLE_INDI=ON \
  -DOAS_ENABLE_GPHOTO2=OFF
cmake --build build-rpi -j$(nproc)
```

`OAS_ENABLE_QHY=ON` now builds `oal_driver_qhy`; QHYCCD SDK is not linked into `oas_core`.

QHY requires an ARM64-compatible QHYCCD SDK/udev installation visible to CMake. INDI compatibility requires a separately running `indiserver` and the exact hardware driver executables.

For an architecture-only test without vendor SDKs:

```bash
cmake -S . -B build-sim -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DOAS_ENABLE_QHY=OFF -DOAS_ENABLE_INDI=OFF
cmake --build build-sim -j$(nproc)
```

This still builds the native `oal.simulated` ABI-v2 reference plugin.

## Windows / MSVC 2022

Use a Qt 6 MSVC kit and OpenCV built for a compatible MSVC ABI. Example:

```powershell
cmake -S . -B build/vs2022 `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.0/msvc2022_64" `
  -DOpenCV_DIR="C:/opencv/opencv/build/x64/vc16/lib"
cmake --build build/vs2022 --config Release
```

The native reference plugin builds without Qt/OpenCV dependencies. Native QHY additionally requires a Windows QHYCCD SDK library compatible with the chosen toolchain.

## Linux desktop

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Enable optional compatibility/hardware integrations only when their dependencies exist:

```bash
-DOAS_ENABLE_QHY=ON
-DOAS_ENABLE_INDI=ON
-DOAS_ENABLE_GPHOTO2=ON
```

## Runtime driver locations

The node scans native ABI-v2 plugins/manifests from:

- `OAL_DRIVER_PATH`;
- `drivers/` beside the executable;
- standard OpenAstroLink library install directories.

On RPi/Linux installation the recommended location is:

```text
/usr/local/lib/openastrolink/drivers
```


## Native Canon EOS

On Raspberry Pi/Linux enable `OAS_ENABLE_NATIVE_CANON=ON` (already enabled by `rpi4-native-release` and `rpi4-observatory-release`). The build requires `libgphoto2-dev` and `libjpeg` development files. This builds the ABI-v2 `oal_driver_canon` plugin; the legacy in-core `canon-gphoto2` compatibility backend remains independently controlled by `OAS_ENABLE_GPHOTO2`.

## ZWO SDK options (v0.2.9)

Native ZWO support is optional per build. `rpi4-native-release` and `rpi4-observatory-release` enable ASI/EAF native drivers; provide `ZWO_ASI_ROOT` / exact ASI include+library paths and `ZWO_EAF_ROOT` / exact EAF include+library paths. General observatory builds may keep INDI enabled in parallel for non-native equipment.
