# Build platforms — v0.2.10

English is canonical. Ukrainian mirror: `docs/uk/BUILD_PLATFORMS.md`.

OpenAstroSuite is no longer tied to Raspberry Pi. A Windows x64 PC, Linux x86_64 PC, or Linux ARM64/Raspberry Pi can all be the **observatory node** when the hardware is attached directly to that computer. The same GUI can run locally or connect to another node over OAL HTTP/WebSocket.

## Supported first-class targets

| Target | GUI | Node | Native drivers | INDI compatibility |
|---|---:|---:|---|---:|
| Windows x64 / MSVC 2022 | yes | yes | QHY, Canon EDSDK, ZWO ASI/EAF, Gemini, Sky-Watcher | optional |
| Linux x86_64 | yes | yes | QHY, Canon libgphoto2, ZWO ASI/EAF, Gemini, Sky-Watcher | optional |
| Linux ARM64 / Raspberry Pi | yes | yes | same as Linux | optional |
| Linux headless | no | yes | selectable | optional |

Native OAL remains the preferred hardware path. INDI is deliberately easy to enable for equipment without a native OAL driver.

## Presets

`CMakePresets.json` contains portable presets and **must not contain developer-specific SDK paths**:

- `windows-core-release` — Qt/OpenCV + serial native drivers, no vendor camera SDKs.
- `windows-native-release` — all native drivers, INDI off.
- `windows-observatory-release` — all native drivers + INDI compatibility.
- `linux-native-release` — all native drivers, INDI off.
- `linux-observatory-release` — all native drivers + INDI compatibility.
- `linux-node-release` — headless Linux node.
- `rpi4-native-release` / `rpi4-observatory-release` — ARM64/RPi aliases of the Linux model.
- `node-sim-release` — no physical hardware dependencies.

Copy `CMakeUserPresets.example.json` to `CMakeUserPresets.json` and edit only that local file. It is ignored by Git.

## Windows x64

Recommended toolchain:

- Visual Studio 2022 MSVC x64 compiler;
- Qt 6 MSVC2022 x64;
- CMake + Ninja;
- OpenCV built for compatible MSVC ABI;
- Windows/x64 vendor SDKs for the devices that are enabled.

Do not mix MinGW libraries with an MSVC build.

Example local setup:

```powershell
Copy-Item CMakeUserPresets.example.json CMakeUserPresets.json
# Edit SDK paths in my-windows-observatory.
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
```

For a dependency-light compile first:

```powershell
cmake --preset windows-core-release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.0/msvc2022_64" `
  -DOpenCV_DIR="C:/opencv/opencv/build/x64/vc16/lib"
cmake --build --preset windows-core-release --parallel
```

### Native Canon on Windows

`OAS_CANON_TRANSPORT=AUTO` resolves to **EDSDK** on Windows. Provide:

```text
CANON_EDSDK_INCLUDE_DIR = directory containing EDSDK.h
CANON_EDSDK_LIBRARY     = EDSDK import library (.lib)
CANON_EDSDK_RUNTIME_DIR = directory containing EDSDK.dll and companion runtime DLLs
```

The EDSDK transport preserves the original EOS file and publishes the EDSDK thumbnail as an OAL preview frame. Hardware qualification is still required per camera model/firmware.

### Native QHY / ZWO on Windows

Use the Windows x64 SDK artifacts, e.g. `.lib` for link and `.dll` for runtime. A Linux `.so` cannot be linked by MSVC.

## Linux x86_64

Debian/Ubuntu baseline:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  libopencv-dev libgphoto2-dev libjpeg-dev
```

Install the x86_64 QHY and ZWO SDKs separately, then put their exact paths in `CMakeUserPresets.json` or pass them on the command line.

```bash
cmake --preset my-linux-observatory
cmake --build --preset my-linux-observatory -j$(nproc)
sudo cmake --install build/linux-observatory
```

Use `file` before linking a vendor library:

```bash
file /opt/qhy/lib/libqhy.so
file /opt/zwo/asi/lib/libASICamera2.so
```

On x86_64 they must report x86-64 ELF objects. On ARM64/RPi they must report AArch64/ARM64.

### Native Canon on Linux

`OAS_CANON_TRANSPORT=AUTO` resolves to **GPHOTO2** on Linux. This is a native OAL driver using libgphoto2 only as the low-level USB/PTP access layer. `OAS_CANON_TRANSPORT=EDSDK` is also supported when a Canon Linux EDSDK matching the machine is installed.

## Raspberry Pi / Linux ARM64

The RPi presets now use the same build model as desktop Linux. The only difference is the architecture of Qt/OpenCV/vendor SDK binaries and deployment service configuration.

```bash
cmake --preset rpi4-observatory-release \
  -DQHYCCD_INCLUDE_DIR=/opt/qhy/include \
  -DQHYCCD_LIBRARY=/opt/qhy/lib/libqhy.so \
  -DZWO_ASI_INCLUDE_DIR=/opt/zwo/asi/include \
  -DZWO_ASI_LIBRARY=/opt/zwo/asi/lib/libASICamera2.so \
  -DZWO_EAF_INCLUDE_DIR=/opt/zwo/eaf/include \
  -DZWO_EAF_LIBRARY=/opt/zwo/eaf/lib/libEAFFocuser.so
cmake --build --preset rpi4-observatory-release -j$(nproc)
```

## Directly attached hardware

Windows/Linux/RPi all support this topology:

```text
QHY / ZWO / Canon / Gemini / Sky-Watcher
                 │ USB / serial
                 ▼
          openastrolink-node
             ┌────┴─────┐
       local GUI    remote GUI
                       + Stellarium
```

RPi is therefore an edge deployment option, not a mandatory proxy.

## Packaging

### Windows

After a successful build:

```powershell
./scripts/package_windows.ps1 \
  -BuildDir build/windows-observatory \
  -QtBin C:/Qt/6.10.0/msvc2022_64/bin \
  -OpenCvBin C:/opencv/opencv/build/x64/vc16/bin \
  -VendorRuntimeDirs @("C:/SDK/QHY/bin","C:/SDK/ZWO/ASI/bin","C:/SDK/ZWO/EAF/bin","C:/SDK/Canon/EDSDK/Dll") \
  -Zip
```

The script runs `cmake --install`, `windeployqt`, and copies explicitly supplied vendor runtime DLLs. Vendor DLL redistribution remains subject to each vendor's license.

### Linux

```bash
./scripts/package_linux.sh build/linux-observatory dist/linux-observatory
```

The Linux package intentionally does not silently redistribute vendor SDK libraries. Install them on the destination host or bundle them only when their licenses permit it.

## Linux device permissions

Serial users normally need `dialout`:

```bash
sudo usermod -aG dialout "$USER"
```

Install QHY/ZWO vendor udev rules. Ensure desktop photo software does not claim a Canon EOS USB/PTP session before OAL does.

## Runtime driver locations

The node scans `OAL_DRIVER_PATH`, `drivers/` beside the executable, and standard installation directories. Normal installation layout:

```text
Windows package:
  bin/OpenAstroSuite.exe
  bin/openastrolink-node.exe
  lib/openastrolink/drivers/*.dll

Linux:
  /usr/local/bin/OpenAstroSuite
  /usr/local/bin/openastrolink-node
  /usr/local/lib/openastrolink/drivers/*.so
```

`build_features.json` is generated at configure time and installed with the package so support logs can state exactly which hardware transports were compiled in.
