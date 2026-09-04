## Current qualification — v0.2.10.50

| Target | Build status | Notes |
|---|---|---|
| Windows x64 / MSVC 2022 + Ninja | ✅ confirmed | Full observatory build succeeded after deterministic `cl.exe` selection. |
| Linux x86_64 | ✅ confirmed | Native build succeeded with per-user Qt bootstrap on Ubuntu 22.04/Jammy. |
| Linux ARM64 / Raspberry Pi 4 | ✅ confirmed | WSL/Linux cross-build reached 100% for node, probe and native vendor drivers including QHY 26.06.04. |
| Linux ARM64 / Raspberry Pi 5 | 🟡 ABI-supported | Same generic aarch64 target; physical Pi 5 runtime/HIL still pending. |
| macOS Apple Silicon / Intel | 🟡 configured | Presets/vendor bootstrap implemented; physical Mac build/sign/package qualification pending. |

Native drivers are the default everywhere. INDI is opt-in only. Raw `cmake --preset` stays side-effect-free; platform build wrappers may search/bootstrap redistributable dependencies. Canon EDSDK remains manual-download/local-discovery only.

## Windows compiler selection — build-fix20

Native Windows presets use the Ninja generator with `CMAKE_CXX_COMPILER=cl.exe`. This keeps the proven MSVC/Ninja build path while preventing Strawberry/MinGW `c++.exe` from being selected, and it does not require CMake to discover a registered Visual Studio instance. Raw `cmake --preset my-windows-observatory` must run in an x64 MSVC Developer Command Prompt; `scripts/build_windows.ps1` loads `vcvars64` automatically. Native Windows build directories use `*-msvc-ninja` names to avoid stale GNU-Ninja and failed VS-generator cache collisions. Windows-hosted Raspberry Pi cross presets remain GNU/Ninja because they intentionally target Linux ARM.

# Build platforms — v0.2.10.50

English is canonical; Ukrainian mirror: `docs/uk/BUILD_PLATFORMS.md`.

## 1. Supported host roles

The observatory host may be:

- Windows x64 desktop/mini-PC;
- Linux x86_64 desktop/mini-PC;
- Linux ARM64 / Raspberry Pi 4 or Raspberry Pi 5;
- macOS Apple Silicon / Intel;
- headless Linux or macOS node.

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


## Native dependency auto-bootstrap (build-fix18)

The recommended native build wrappers now resolve dependencies before invoking CMake. They search existing installations first and only bootstrap missing redistributable components. Raw `cmake --preset ...` remains side-effect-free.

Linux/WSL:

```bash
./scripts/build_linux.sh my-linux-observatory
```

The first run can install missing Debian/Ubuntu build packages, install a per-user Qt through `aqtinstall` when the distro Qt is older than 6.4, install/find OpenCV, and stage QHY/ZWO SDKs. The resolved paths are saved under `.oal/native-deps-linux-<arch>.env`. Use `--no-auto-deps` to disable this behavior.

macOS:

```bash
./scripts/build_macos.sh my-macos-arm64-observatory
```

Homebrew is used for build tools/OpenCV when available; Qt is searched first and otherwise installed into the per-user OpenAstroLink cache. QHY and ZWO SDKs are staged in the same cache.

Windows:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

The wrapper searches installed Qt/OpenCV/vendor SDKs, bootstraps Qt through `aqtinstall`, can build/install OpenCV through a per-user vcpkg checkout, downloads QHY from the official QHY SDK repository, and auto-discovers local ZWO/Canon SDKs. Use `-NoAutoDeps` for a strictly manual environment.

Canon EDSDK is intentionally **search-only** on every platform: OpenAstroLink does not automatically download or redistribute Canon's license-gated SDK. ZWO Linux/macOS blobs may be staged from the repository's pinned SDK mirror; Windows ZWO is searched locally first and, when missing, fetched from ZWO's official developer product-download endpoint; that endpoint tracks the current vendor release rather than a pinned version.

Managed native cache roots are also searched directly by CMake after bootstrap, so subsequent manual `cmake --preset ...` invocations can reuse them.

## 3. Presets

Repository presets:

```text
windows-core-release
windows-native-release
windows-observatory-release
linux-native-release
linux-observatory-release
linux-node-release
macos-core-release
macos-native-release
macos-observatory-release
macos-node-release
macos-arm64-observatory-release
macos-x86_64-observatory-release
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
  build-essential cmake pkg-config \
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

Linux presets use `Unix Makefiles`, so Ninja is no longer a hard dependency. Windows MSVC presets continue to use Ninja.

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

## 7. macOS (Apple Silicon and Intel)

macOS is a first-class source/build target beginning with build-fix3. The native path uses Apple Clang, Qt >= 6.4, OpenCV, Canon EDSDK and the vendor macOS ZWO libraries. QHY remains optional until a matching macOS SDK library is supplied. Classic ASCOM is Windows-only and is disabled.

Repository presets:

```text
macos-core-release
macos-native-release
macos-observatory-release
macos-node-release
macos-arm64-observatory-release
macos-x86_64-observatory-release
```

Typical Homebrew bootstrap:

```bash
brew install cmake qt opencv
```

The current Homebrew `qt` meta-formula includes the Qt modules used by OAL; if installing split Qt formulae instead, ensure Core/Gui/Network/Widgets/SerialPort/WebSockets/HttpServer/Concurrent are all present.

The local example expects the same sibling SDK tree used by the Windows/Linux development checkout:

```text
workspace/astro/oal
workspace/astro/zwo/asi
workspace/astro/zwo/eaf
workspace/astro/edsdk/EDSDKv132021M
```

ZWO ASI SDK layouts use `mac_arm64` for Apple Silicon and `mac`/`mac_x64` for Intel depending on SDK release; EAF uses `mac_arm64` and `mac_x64`. CMake searches all of those suffixes. Canon `AUTO` resolves to EDSDK on macOS.

Build:

```bash
./scripts/build_macos.sh my-macos-arm64-observatory
# Intel:
./scripts/build_macos.sh my-macos-x86_64-observatory
```

macOS build support is source/configuration-qualified only until it is compiled and HIL-tested on physical Mac hardware. Vendor dylib signing/notarization and `.app` packaging remain separate qualification work.

## 8. INDI

`OAS_ENABLE_INDI=ON` enables the built-in compatibility client. It does not link native drivers through INDI. A separate `indiserver` is only required when using an actual INDI-only device.

## 9. Runtime search paths

Windows packages should place required DLLs beside the executables/package runtime path. Linux either installs vendor runtime libraries system-wide according to the vendor instructions or configures the dynamic loader appropriately. `package_linux.sh` intentionally does not redistribute vendor `.so` files automatically because licensing differs by vendor.

## 10. Raspberry Pi native and cross-compilation

### Automatic cross-sysroot bootstrap (build-fix4)

The cross compiler alone is not enough: CMake must see ARM target headers/libraries (Qt, OpenCV, libc, libusb, JPEG) while Qt's `moc`/`rcc` tools must remain runnable on the x86_64 host. `scripts/bootstrap_rpi_cross.sh` now prepares both sides explicitly.

On Debian/Ubuntu/WSL the default, fully local path creates a Debian 12/Bookworm target root with `debootstrap` + `qemu-user-static`, installs the ARM development packages, fixes absolute symlinks, determines the target Qt version, and bootstraps matching host Qt tools from the open-source Qt source archive when needed:

On Ubuntu 22.04 the distro `debian-archive-keyring` can be too old for current Bookworm signatures. build-fix6 keeps verification enabled: it obtains the official Debian 12 archive/release/security keys over HTTPS, verifies pinned fingerprints, caches an OAL-local keyring under `~/.cache/openastrolink/`, and passes that keyring to `debootstrap --keyring`.

```bash
./scripts/bootstrap_rpi_cross.sh arm64
```

To mirror the exact userspace of a physical Raspberry Pi instead:

```bash
./scripts/bootstrap_rpi_cross.sh arm64 --from-pi pi@openastrolink.local
```

The default locations are user-local and do not need to be hard-coded into the repository:

```text
~/.local/share/openastrolink/sysroots/rpi4-arm64
~/.local/share/openastrolink/host-qt/<target-Qt-version>
~/.local/share/openastrolink/sdk/qhy-arm64
```

If a sibling `../QHYCCD_Linux_New` checkout exists, the bootstrap also extracts the newest matching ARM archive, verifies the ELF architecture with `file`, and stages only headers/shared libraries for cross-linking. It does **not** install ARM QHY binaries into the x86_64 host filesystem.

The bootstrap writes `.oal/rpi-cross-arm64.env`, which `scripts/build_rpi_cross.sh` consumes automatically. Therefore the normal loop becomes:

```bash
./scripts/bootstrap_rpi_cross.sh arm64
./scripts/build_rpi_cross.sh arm64
```

Or as one explicit command:

```bash
./scripts/build_rpi_cross.sh arm64 --bootstrap
```

The stock repository cross preset still keeps vendor SDKs disabled. A local `CMakeUserPresets.json` may enable the confirmed ARM64 Canon EDSDK and ZWO `armv8` libraries and the staged QHY SDK.


### Native ARM64 on Raspberry Pi 4/5

On 64-bit Raspberry Pi OS use:

```bash
cmake --preset rpi4-node-release
cmake --build --preset rpi4-node-release -j"$(nproc)"
```

Use `rpi4-observatory-release` instead when the local GUI is required. These presets enforce an ARM64 target and fail instead of silently producing an x86_64 binary when run on the wrong host. For 32-bit Raspberry Pi OS use `rpi-armhf-node-release`.

### Cross-build from Linux/WSL

Installed cross compilers are supported through repository toolchain files:

```text
cmake/toolchains/linux-aarch64-gcc.cmake   -> aarch64-linux-gnu-g++
cmake/toolchains/linux-armhf-gcc.cmake     -> arm-linux-gnueabihf-g++
```

A compiler alone is not sufficient for OpenAstroLink: Qt6/OpenCV and any enabled hardware SDK must also exist for the target architecture. Provide a Raspberry Pi sysroot containing the target development files:

```bash
./scripts/build_rpi_cross.sh arm64 /opt/openastrolink-sysroots/rpi4-arm64
# or
./scripts/build_rpi_cross.sh armhf /opt/openastrolink-sysroots/rpi-armhf
```

The stock cross presets intentionally disable QHY, ZWO, Canon and INDI. This prevents an x86_64 host `.so` from being accidentally linked into an ARM target. After the core ARM node configures successfully, create a local user preset that inherits the cross preset and enables each vendor driver only with matching ARM libraries.

Qt cross builds may also require host-side Qt tools (`moc`, `rcc`, `uic`). When the target Qt package requests them, set `OAS_QT_HOST_PATH` (forwarded to Qt as `QT_HOST_PATH`) to the compatible host Qt installation while keeping `Qt6_DIR`/target packages in the sysroot.

### Cross-build from native Windows

The repository also supports the Arm GNU Toolchain layouts already used by the project:

```text
cmake/toolchains/windows-to-linux-aarch64-gcc.cmake
cmake/toolchains/windows-to-linux-armhf-gcc.cmake
```

Use:

```powershell
.\scripts\build_rpi_cross.ps1 -Arch arm64 -Sysroot C:\toolchains\sysroots\rpi4-arm64 -QtHostPath C:\Qt\6.4.2\msvc2022_64
```

The default roots match the existing `C:/toolchains/gcc-arm-10.3-2021.07-...` installations and can be overridden with `OAS_CROSS_TOOLCHAIN_ROOT` and `OAS_CROSS_TRIPLE`. Automatic sysroot creation is currently implemented on Linux/WSL; native-Windows cross builds consume an already prepared/copied sysroot and an explicit matching Qt host-tools path.

### Toolchains intentionally not used for OAL application builds

`win_mingw_x86/x64` are not production OAL Windows presets because the current Windows dependency stack is MSVC ABI (`Qt msvc2022_64` plus MSVC vendor import libraries). `arm-none-eabi`/bare-metal toolchains target firmware without a Linux userspace and therefore cannot build the Qt/OpenCV OpenAstroLink application or node.



> **Linux baseline:** the system Qt must be **6.4 or newer**. Ubuntu 22.04/Jammy ships Qt 6.2.4 and has no `qt6-httpserver-dev`, so it is not a supported system-Qt build host. Use Ubuntu 24.04+ or Debian/Raspberry Pi OS 12 Bookworm+, or provide a custom Qt >= 6.4 through `CMAKE_PREFIX_PATH`/`Qt6_DIR`.


### v0.2.10.49 build-fix8 — target Qt/OpenCV discovery in cross sysroots

The ARM cross toolchains now pin the Debian multiarch (`aarch64-linux-gnu` / `arm-linux-gnueabihf`) and automatically seed `Qt6_DIR` and `OpenCV_DIR` from target configs inside `OAS_CROSS_SYSROOT`. Host Qt (`OAS_QT_HOST_PATH`) remains host-tools-only (`moc/rcc/uic`); host and target Qt are never mixed. The bootstrap environment record also persists target package paths for repeatable builds.


<!-- build-fix8-target-package-discovery -->
### build-fix8: target package discovery

After the ARM sysroot bootstrap, CMake uses target `Qt6Config.cmake` and `OpenCVConfig.cmake` from the Debian multiarch directory, while `OAS_QT_HOST_PATH` is used only for host `moc/rcc/uic`. For ARM64 the standard paths are `usr/lib/aarch64-linux-gnu/cmake/Qt6` and `usr/lib/aarch64-linux-gnu/cmake/opencv4` inside the sysroot.


### v0.2.10.49 build-fix9 — QHY staging validation

`bootstrap_rpi_cross.sh` no longer advertises a full vendor environment after a failed QHY stage. When QHY is enabled, staging must finish with a real `include/qhyccd.h` and `lib/libqhy.so` whose ELF architecture matches the target. Use `--no-qhy` for a deliberately QHY-free environment. `build_rpi_cross.sh` reads `OAS_QHY_SDK` from the bootstrap env record and forwards the exact include/library paths to CMake.

### v0.2.10.49 build-fix10 — QHY ARM archive naming is not ABI

Do not infer the QHY target ABI from `armv8` in the archive name. The legacy public `QHYCCD_Linux_New` v2.0.11 `armv8` package physically contains a 32-bit ARM/EABI shared library. OAL therefore validates every QHY candidate with `file(1)` and uses this package only for ARMHF. ARM64 requires a genuine QHY `Arm_64`/`AARCH64` SDK archive.

For the current ARM64 build, use `my-rpi4-cross-arm64` (QHY OFF) until such an SDK is staged. Once obtained:

```bash
./scripts/stage_qhy_cross_sdk.sh arm64 /path/to/current-qhy-arm64-sdk.tgz \
    "$HOME/.local/share/openastrolink/sdk/qhy-arm64"
./scripts/build_rpi_cross.sh arm64 --preset my-rpi4-cross-arm64-full
```

For the legacy cloned package, ARMHF is valid:

```bash
./scripts/stage_qhy_cross_sdk.sh armhf ../QHYCCD_Linux_New \
    "$HOME/.local/share/openastrolink/sdk/qhy-armhf"
```


### v0.2.10.49 build-fix12 — Canon Linux headers + Qt HttpServer 6.4 compatibility

The first physical AArch64 compilation reached the source layer and exposed two version/SDK portability details. Canon EDSDK 13.20.x Linux headers still contain the MSVC token `__int64` and use `WCHAR`; OAL now supplies translation-unit-local Linux compatibility definitions before including `EDSDK.h`, leaving the vendor SDK untouched. Qt HttpServer changed `QAbstractHttpServer::bind(QTcpServer*)` from `void` in Qt 6.4-6.7 to `bool` in Qt 6.8+, so the node now compiles against both supported API shapes.


### v0.2.10.49 build-fix13 — AArch64 transitive sysroot linker closure

The ARM64 cross-link closure is now physically confirmed. The earlier final-link failure was caused by Bookworm BLAS/LAPACK alternatives living in `usr/lib/<multiarch>/blas` and `.../lapack` plus missing target-only transitive search paths. The corrected policy adds target-sysroot `-rpath-link`/`-L` paths, installs and validates BLAS/LAPACK, normalizes sysroot symlinks with the required privileges, and resolves all transitive `DT_NEEDED` libraries from the Bookworm target rather than the Jammy host. The resulting build reaches 100% for `openastrolink-node` and `oal-hardware-probe` with Canon EDSDK, ZWO ASI/EAF, QHY, Gemini, SkyWatcher and EQDrive enabled.

#### Debian BLAS/LAPACK alternatives in cross sysroots (build-fix15)

On Bookworm, `libblas.so.3` and `liblapack.so.3` may be exposed through Debian alternatives while the concrete reference libraries live below `usr/lib/<multiarch>/blas` and `usr/lib/<multiarch>/lapack`. A foreign GNU linker should not rely on host resolution of those absolute alternatives links. The OAL bootstrap therefore normalizes sysroot links as root, validates the concrete target libraries with `file`, and the CMake cross-link policy supplies both numerical subdirectories via link-time-only `-rpath-link`/`-L`. OAL also links the target LAPACK and BLAS runtime libraries explicitly after OpenCV so OpenCV/Armadillo/ARPACK/SuperLU numerical symbols are closed deterministically.


### build-fix17 — native OAL is the default; INDI is opt-in

`OAS_ENABLE_INDI` defaults to `OFF` at the CMake option level and in all normal observatory/node presets. Use the explicit `*-indi-release` presets or pass `-DOAS_ENABLE_INDI=ON` only when an INDI-only device is required. Native OAL drivers are never routed through INDI. The Linux/macOS vendor bootstrap may use a pinned `indi-3rdparty` revision solely as a reproducible mirror of ZWO SDK blobs; this does not install, enable or link the INDI compatibility backend.

### Ubuntu 22.04 / Jammy Qt note

Jammy provides Qt 6.2.4 only. Installing more Jammy `qt6-*`/`libqt6*-dev` packages does not satisfy OpenAstroLink's Qt >= 6.4 + Qt HttpServer requirement. Use:

```bash
./scripts/build_linux.sh my-linux-observatory --bootstrap-deps
```

The wrapper installs a complete per-user Qt through `aqtinstall` under `~/.local/share/openastrolink/qt` and does not require the Qt GUI installer or a Qt account. Do not run the Qt GUI installer with `sudo`. The Linux wrapper also removes a stale CMake cache automatically when a checkout has moved between WSL (`/mnt/c/...`) and native Linux (`~/...`).
