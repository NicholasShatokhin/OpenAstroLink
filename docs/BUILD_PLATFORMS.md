# Build platforms — v0.2.10.5

English is canonical; Ukrainian mirror: `docs/uk/BUILD_PLATFORMS.md`.

## 1. Supported host roles

The observatory host may be:

- Windows x64 desktop/mini-PC;
- Linux x86_64 desktop/mini-PC;
- Linux ARM64 / Raspberry Pi 4;
- headless Linux node.

The hardware can be attached directly to that host. An RPi is not required when a desktop host owns the devices.

## 2. CMake compatibility

Repository presets use **schema version 2** and require **CMake 3.20+**.

Why: schema v6 requires newer CMake merely to parse `CMakePresets.json`/`CMakeUserPresets.json`. On older Ubuntu/WSL releases this produced:

```text
Unrecognized "version" field
```

Always check:

```bash
cmake --version
```

If migrating from v0.2.10/v0.2.10.2, recreate `CMakeUserPresets.json` from the current example; an old local file with `"version": 6` can still break preset parsing even when the repository preset has been fixed.

```bash
rm -f CMakeUserPresets.json
cp CMakeUserPresets.example.json CMakeUserPresets.json
```

## 3. Presets

Repository presets:

```text
windows-core-release
windows-native-release
windows-observatory-release
linux-native-release
linux-observatory-release
linux-node-release
rpi4-native-release
rpi4-observatory-release
node-sim-release
```

Local examples:

```text
my-windows-observatory          # Canon disabled; no EDSDK required
my-windows-observatory-edsdk    # Canon enabled
my-linux-observatory
```

`CMakeUserPresets.json` is ignored by Git.

## 4. Windows

### Toolchain

Official path:

```text
MSVC 2022 x64 + Ninja + Qt MSVC2022_64
```

Do not mix Qt MSVC libraries with MinGW/Strawberry GCC.

Recommended command:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

The script attempts to load `vcvars64.bat` from Community/Professional/Enterprise/BuildTools if `cl.exe` is not already in PATH, and adds Qt's common Ninja directory when available.

Manual equivalent:

```powershell
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
```

The configure output must contain:

```text
The CXX compiler identification is MSVC
```

and must not contain `GNU`, `MinGW`, `Strawberry`, or `c++.exe` from a GCC distribution.

### QHY

Use Windows x64 QHY SDK headers/import library/runtime. OAL v0.2.10.2+ does not place the QHY include directory in the normal MSVC include path because QHY compatibility `stdint.h` headers can shadow the CRT/STL. The driver instead includes `qhyccd.h` through a generated absolute-path wrapper.

### ZWO

Use Windows x64 ASI and EAF import libraries and matching DLLs. Do not point MSVC at Linux `.so` files.

### Canon

The transport is controlled by `OAS_CANON_TRANSPORT` (`EDSDK`, `GPHOTO2`, or `AUTO`).


Without EDSDK:

```text
OAS_ENABLE_NATIVE_CANON=OFF
```

With EDSDK use the dedicated local preset and set `CANON_EDSDK_*` paths.

### Packaging

Use `scripts/package_windows.ps1`. It performs `cmake --install`, runs `windeployqt`, and copies OpenCV/vendor runtime DLLs from explicit directories.

## 5. Linux x86_64 / ARM64

### Base packages

Typical Debian/Ubuntu dependencies:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  libopencv-dev libgphoto2-dev libjpeg-dev
```

Qt package names vary across distributions. Install missing Qt 6 module development packages indicated by CMake.

### Vendor SDK staging

Recommended tree:

```text
/opt/openastrolink-sdk/
  qhy/
    include/qhyccd.h
    lib/libqhy.so
  zwo/
    asi/
      include/ASICamera2.h
      lib/libASICamera2.so
    eaf/
      include/EAF_focuser.h
      lib/libEAFFocuser.so
```

Verify every binary:

```bash
uname -m
file /opt/openastrolink-sdk/qhy/lib/libqhy.so
file /opt/openastrolink-sdk/zwo/asi/lib/libASICamera2.so
file /opt/openastrolink-sdk/zwo/eaf/lib/libEAFFocuser.so
```

Expected architectures must match (`x86-64` for x86_64 host, `aarch64` for 64-bit RPi).

### Canon

The transport is controlled by `OAS_CANON_TRANSPORT` (`EDSDK`, `GPHOTO2`, or `AUTO`).


Default Linux transport is libgphoto2, found by `pkg-config`; therefore no EDSDK path is required.

### Build

```bash
./scripts/check_build_environment.sh
./scripts/build_linux.sh my-linux-observatory
```

or manually:

```bash
cmake --preset my-linux-observatory
cmake --build --preset my-linux-observatory -j"$(nproc)"
```

### Install

```bash
sudo cmake --install build/linux-observatory
sudo ldconfig
```

## 6. WSL

WSL is useful as a Linux compile/test environment. Important distinctions:

- `/mnt/c/...` is a WSL path, not a native Linux workstation/RPi path;
- Linux `.so` architecture must match the WSL Linux architecture;
- Windows `.dll`/`.lib` cannot be linked into Linux targets;
- direct USB camera/serial access requires explicit WSL passthrough (for example through the Windows USB forwarding stack) and is not the recommended observatory deployment path.

When a WSL build reports `Unrecognized "version" field`, first check both `CMakePresets.json` **and** the ignored `CMakeUserPresets.json` schema versions and the installed CMake version.

## 7. INDI

`OAS_ENABLE_INDI=ON` enables the built-in compatibility client. It does not link native drivers through INDI. A separate `indiserver` is only required when using an actual INDI-only device.

## 8. Runtime search paths

Windows packages should place required DLLs beside the executables/package runtime path. Linux either installs vendor runtime libraries system-wide according to the vendor instructions or configures the dynamic loader appropriately. `package_linux.sh` intentionally does not redistribute vendor `.so` files automatically because licensing differs by vendor.
