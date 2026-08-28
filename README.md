# OpenAstroSuite / OpenAstroLink


**Current release: v0.2.10.19 — server-first startup + live native catalogue + corrected direct mount control**

This release brings HTTP/WebSocket control up before slow hardware enumeration, fixes the native-device combobox catalogue, keeps `synscan-wifi` as direct UDP/11880 mount control, and makes `oal.eqdrive` a dual-protocol native driver: EQMOD-compatible Sky-Watcher Motor Controller transport first, official EQDrive ASTEP as a fallback. Classic ASCOM remains available and the existing `oal.skywatcher` / EqMount driver is retained.

English is the canonical project language. Ukrainian mirrors are provided in `README_UA.md` and `docs/uk/`.

OpenAstroLink (OAL) is a local-first observatory control stack. The reference path is **native OAL drivers**; INDI, ASCOM Alpaca and LX200 remain optional compatibility layers for equipment that does not yet have a native OAL driver.


## v0.2.10.19 HIL startup/catalogue/mount correction

- **Server-first node startup:** driver libraries are loaded, HTTP/WebSocket listeners start, and hardware enumeration runs on a background thread. Slow Gemini reset recovery or EQDrive/QHY discovery no longer prevents the GUI from connecting to the node.
- **Live native catalogue fix:** native device records are no longer incorrectly filtered by a driver-only `native` flag. A Gemini/QHY/native mount discovered after startup is propagated in node state and appears in the remote GUI combobox without restarting the GUI.
- **Asynchronous serial selector:** applying a Gemini/Sky-Watcher/EQDrive COM override queues discovery instead of blocking the GUI. If the selected native device is already cached, no redundant serial reopen/reset is performed.
- **Direct SynScan/EQDrive Wi-Fi:** `synscan-wifi` continues to talk directly to the mount adapter over UDP 11880, while `synscan-app` remains the separate SynScan Pro/App compatibility backend on UDP 11881.
- **Motor Controller semantics corrected:** running/GOTO/initialization/direction bits and GOTO motion-mode encoding now follow the Sky-Watcher/EQMOD convention. Direct GOTO uses the proven `G -> H -> M -> J` sequence and verifies encoder movement before reporting success.
- **Native EQDrive dual protocol:** `oal.eqdrive` first probes the EQMOD-compatible Sky-Watcher Motor Controller command set used by the proven EQMOD path, then falls back to official EQDrive ASTEP (`St`, `Pos`, `Cg`, `Speed`, `Goto`) where exposed by firmware. A busy COM port exits discovery immediately rather than fighting EQMOD/ASCOM.
- **Safety retained:** direct Wi-Fi and native EQDrive celestial GOTO still require Sync and remain HIL-limited to small test moves until axis direction/pier geometry is qualified.

## Current hardware coverage

Native OAL driver code exists for:

- QHY cameras (`oal.qhy`, QHYCCD SDK);
- Canon EOS (`oal.canon`: Canon EDSDK on Windows, libgphoto2/PTP on Linux);
- ZWO ASI cameras (`oal.zwo.asi`, ASI SDK);
- ZWO EAF focusers (`oal.zwo.eaf`, EAF SDK);
- Gemini EAF (`oal.gemini`, direct serial protocol);
- Sky-Watcher/SynScan mounts (`oal.skywatcher`, existing direct serial/EqMount path);
- EQDrive controllers (`oal.eqdrive`, experimental dual-protocol direct serial path: EQMOD-compatible Motor Controller first, official ASTEP fallback).

These drivers are implemented but still require hardware-in-the-loop qualification on the actual host OS, CPU architecture and device/firmware before being labelled production-ready.


## Mount interoperability added in v0.2.10.17

- **Native EQDrive:** `oal.eqdrive` is separate from and does not replace `oal.skywatcher`. Discovery is read-only; the initial RA/Dec model is a conservative sync-anchor model and requires one Sync before native celestial GOTO. See `docs/EQDRIVE.md`.
- **Classic ASCOM (Windows):** backend `ascom-classic` uses `oas-ascom-host.exe` and the installed ASCOM Platform/registered Telescope driver, with GUI **ASCOM Chooser...** and **ASCOM Properties...** controls. This is the compatibility path for EQMOD-style use. See `docs/ASCOM_CLASSIC.md`.
- **SynScan network:** `synscan-wifi` talks directly to the mount/EQDrive Wi-Fi adapter with the Motor Controller protocol on UDP 11880 and does not need SynScan Pro. `synscan-app` talks to a running SynScan App/Pro on UDP 11881. See `docs/SYNSCAN_NETWORK.md`.
- **Gemini serial selection:** choosing a COM port in **Native serial discovery** now persists the override and migrates an old native Gemini backend binding to the newly discovered device on that port. CLI `--gemini-port` still wins for the current process.

## HIL reliability fixes in this package

- **QHY single-frame lifecycle:** every successful/failed single-frame cycle is explicitly terminated with `CancelQHYCCDExposingAndReadout`; a readout watchdog cancels a stuck SDK call, actual exposure/gain/offset are read back from the SDK, and frame min/max/mean are logged. Native QHY discovery is not invoked while a QHY handle is active.
- **Scoped native rediscovery:** reconnect retries refresh only the missing native driver. An unplugged Gemini no longer causes `ScanQHYCCD()` or repeated EQDrive/Sky-Watcher probes against a COM port currently owned by EQMOD/Classic ASCOM.
- **Gemini COM migration:** stale port-based bindings can migrate after unique automatic rediscovery; changing the serial selector refreshes only `oal.gemini` and updates the persisted native binding.
- **SynScan network UX:** `synscan-wifi` auto-discovers the direct adapter on UDP 11880 (including common 192.168.4.1/192.168.0.1 AP addresses); `synscan-app` independently discovers the phone/PC running SynScan Pro on UDP 11881. TCP 11882 is documented only as another server exported by SynScan App/Pro, not as direct mount Wi-Fi.
- **Mount direction diagnostics:** OpenAstroLink does not guess/invert RA or DEC. Before GOTO it logs current RA/Dec, target, tracking, pier side and shortest coordinate deltas; Stellarium coordinates are logged as decoded/forwarded unchanged.
- **EQDrive serial diagnostics:** focused native discovery now tries all four DTR/RTS states and logs read-only ASTEP replies (`FWs`, `FW`, `FWx`, `St`, `Pos`, `Cg`) before exposing a mount only when the motor section is actually usable.

## Runtime model

```text
QHY / Canon / ZWO / Gemini / Sky-Watcher / EQDrive
                    │ USB / serial
                    ▼
             openastrolink-node
               ┌────┴─────┐
          local GUI   remote GUI
                         │
                     Stellarium

Optional compatibility path:
INDI / Classic ASCOM / ASCOM Alpaca / SynScan network / LX200
```

The node owns hardware and long-running operations. Closing a GUI does not stop the node or disconnect devices.

---

# Build quick start

## CMake requirement and preset compatibility

This release intentionally uses **CMake preset schema v2** so it works with CMake 3.20+ instead of requiring CMake 3.25+ merely to parse the presets.

Check first:

```bash
cmake --version
```

Required:

```text
CMake >= 3.20
```

If an older checkout contains a `CMakeUserPresets.json` with:

```json
"version": 6
```

an older Linux CMake can fail with:

```text
Unrecognized "version" field
```

For v0.2.10.11 recreate the user preset from the supplied example:

```bash
rm -f CMakeUserPresets.json
cp CMakeUserPresets.example.json CMakeUserPresets.json
```

Then edit only the local SDK paths.

Preflight helpers:

```bash
./scripts/check_build_environment.sh
```

or on Windows PowerShell:

```powershell
.\scripts\check_build_environment.ps1
```

---

# Windows x64 build

## Supported toolchain

Use one ABI consistently:

```text
MSVC 2022 x64
+ Ninja
+ Qt 6 MSVC2022_64
+ MSVC-compatible OpenCV
+ Windows x64 vendor import libraries/DLLs
```

Do **not** combine MinGW/Strawberry GCC with Qt `msvc2022_64` or vendor `.lib` files.

The build helper attempts to load `vcvars64.bat` automatically, so a normal PowerShell window is sufficient in most installations.

## Required base software

Install:

- Visual Studio 2022 C++ x64 toolchain;
- Qt 6.x `msvc2022_64` with Core, GUI, Widgets, Network, SerialPort, WebSockets, HttpServer and Concurrent;
- OpenCV 4 built for the MSVC ABI;
- Ninja (Qt's `C:\Qt\Tools\Ninja` is acceptable).

## Vendor SDKs

### QHY

Install the **Windows x64 QHYCCD SDK**. Example layout used by the current development machine:

```text
C:/Program Files/QHYCCD/AllInOne/sdk/include/qhyccd.h
C:/Program Files/QHYCCD/AllInOne/sdk/x64/qhyccd.lib
C:/Program Files/QHYCCD/AllInOne/sdk/x64/*.dll
```

OAL isolates QHY headers from the MSVC CRT/STL include search path because some QHY SDK packages ship compatibility headers named `stdint.h`/`stdint_windows.h`.

### ZWO ASI

Download/extract the Windows x64 ASI SDK and point the preset to:

```text
ASICamera2.h
ASICamera2.lib
ASICamera2.dll (runtime)
```

### ZWO EAF

Download/extract the Windows x64 EAF SDK and point the preset to:

```text
EAF_focuser.h
EAF_focuser.lib (or the import library name supplied by the SDK)
matching runtime DLL
```

### Canon EDSDK

Canon EDSDK is optional. If it is not installed, use the default example preset `my-windows-observatory`, which contains:

```json
"OAS_ENABLE_NATIVE_CANON": "OFF"
```

After installing EDSDK, use `my-windows-observatory-edsdk` and set the EDSDK header, import library and runtime directory.

## Configure local paths

```powershell
Copy-Item CMakeUserPresets.example.json CMakeUserPresets.json
```

Edit `CMakeUserPresets.json`. It is intentionally ignored by Git.

Then build:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

Equivalent manual commands from an x64 MSVC developer environment:

```powershell
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
```

To enable Canon EDSDK:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory-edsdk -Clean
```

## Windows packaging

After a successful build:

```powershell
.\scripts\package_windows.ps1 `
  -BuildDir build/windows-observatory `
  -QtBin C:/Qt/6.10.0/msvc2022_64/bin `
  -OpenCvBin C:/opencv/opencv/build/x64/vc16/bin `
  -VendorRuntimeDirs @(
    "C:/Program Files/QHYCCD/AllInOne/sdk/x64",
    "C:/SDK/ZWO/ASI SDK/lib/x64",
    "C:/SDK/ZWO/EAF/lib/Windows/x64/Release"
  ) `
  -Zip
```

Add the Canon EDSDK runtime directory to `VendorRuntimeDirs` when Canon is enabled and the vendor license permits packaging those binaries.

---

# Native Linux build (x86_64 or ARM64/RPi)

The same node and GUI can own hardware directly on a normal Linux workstation, mini-PC or Raspberry Pi.

WSL is useful for compilation/testing, but direct observatory hardware access is better on native Linux unless USB/serial passthrough has been configured explicitly.

## Base dependencies (Debian/Ubuntu family)

Package names vary slightly by distribution/release. Typical dependencies are:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  libopencv-dev libgphoto2-dev libjpeg-dev
```

If your distribution splits additional Qt modules, install the corresponding Qt 6 development package. GUI builds also require Qt Widgets (normally included by `qt6-base-dev`).

Check:

```bash
cmake --version
./scripts/check_build_environment.sh
```

## Canon on Linux

The default Linux native preset uses **libgphoto2**, therefore no Canon EDSDK path is required:

```text
Canon EOS → USB/PTP → libgphoto2 → oal.canon → OAL ABI v2
```

Verify:

```bash
pkg-config --modversion libgphoto2
pkg-config --cflags --libs libgphoto2
```

Canon EDSDK can be selected explicitly later, but it is not required for the normal Linux build.

## QHY Linux SDK

Use the SDK that matches the host CPU architecture. A recommended local staging layout is:

```text
/opt/openastrolink-sdk/qhy/include/qhyccd.h
/opt/openastrolink-sdk/qhy/lib/libqhy.so
```

Example staging after extracting the vendor SDK:

```bash
sudo mkdir -p /opt/openastrolink-sdk/qhy/include /opt/openastrolink-sdk/qhy/lib
sudo cp /path/to/qhy/include/*.h /opt/openastrolink-sdk/qhy/include/
sudo cp /path/to/qhy/lib/libqhy.so* /opt/openastrolink-sdk/qhy/lib/
```

If only a versioned library is present, create a development symlink, for example:

```bash
cd /opt/openastrolink-sdk/qhy/lib
sudo ln -sf libqhy.so.0.1.8 libqhy.so
```

Check architecture before linking:

```bash
uname -m
file /opt/openastrolink-sdk/qhy/lib/libqhy.so
```

For WSL-only compile tests you may point `CMakeUserPresets.json` directly at `/mnt/c/...`, but the `.so` must still be Linux x86_64 if `uname -m` is `x86_64`.

For the currently used QHY SDK folders, a WSL-only example is:

```json
"QHYCCD_INCLUDE_DIR": "/mnt/c/workspace/astro/QHYCCD_Linux",
"QHYCCD_LIBRARY": "/mnt/c/workspace/astro/qhysdk/lib/libqhy.so.0.1.8"
```

Before using it:

```bash
uname -m
file /mnt/c/workspace/astro/qhysdk/lib/libqhy.so.0.1.8
```

## ZWO ASI Linux SDK

Recommended staging:

```text
/opt/openastrolink-sdk/zwo/asi/include/ASICamera2.h
/opt/openastrolink-sdk/zwo/asi/lib/libASICamera2.so
```

Verify architecture with `file` before building.

## ZWO EAF Linux SDK

Recommended staging:

```text
/opt/openastrolink-sdk/zwo/eaf/include/EAF_focuser.h
/opt/openastrolink-sdk/zwo/eaf/lib/libEAFFocuser.so
```

Again, the library architecture must match `uname -m`.

## USB/serial permissions

For serial devices:

```bash
sudo usermod -aG dialout "$USER"
```

Log out/in afterwards. Install the vendor-provided QHY/ZWO udev rules where required. Canon desktop photo-management services may need to be disabled from auto-grabbing the camera during observatory operation.

## Linux user preset and build

```bash
cp CMakeUserPresets.example.json CMakeUserPresets.json
```

Edit the `/opt/openastrolink-sdk/...` paths if your SDKs live elsewhere.

Then:

```bash
./scripts/build_linux.sh my-linux-observatory
```

Equivalent manual commands:

```bash
cmake --preset my-linux-observatory
cmake --build --preset my-linux-observatory -j"$(nproc)"
```

Headless system build:

```bash
cmake --preset linux-node-release
cmake --build --preset linux-node-release -j"$(nproc)"
```

Install:

```bash
sudo cmake --install build/linux-observatory
sudo ldconfig
```

Package without redistributing vendor libraries automatically:

```bash
./scripts/package_linux.sh build/linux-observatory
```

---

# INDI compatibility

INDI remains intentionally easy to enable for equipment not yet covered by a native OAL driver. It does not define OAL semantics and is not required by QHY, Canon, ZWO, Gemini or Sky-Watcher native paths.

Presets:

```text
*-native-release       native OAL only
*-observatory-release  native OAL + INDI compatibility client enabled
```

A running `indiserver` is only needed when you actually use an INDI device.

---

# First hardware validation

Do not start with a full unattended session. Qualify one layer at a time:

1. build and package cleanly;
2. run `oal-hardware-probe`;
3. mount: status → small GOTO → abort → tracking → sync → guide pulse;
4. focuser: position → small relative/absolute moves → limits → reconnect;
5. camera: short exposures → settings → cancel → repeated capture → reconnect;
6. ASTAP on known/real sky frames;
7. autofocus on real optics;
8. closed-loop pointing and polar alignment;
9. guiding and long DSO sequences / planetary video.

See `docs/VALIDATION.md` and `docs/OAL_SPECIFICATION.md`.

# Current limits

The project is suitable for continued supervised hardware qualification, not yet an unattended Internet-facing observatory. Major remaining work includes reliable replayable WebSocket events, complete RFC 9457 HTTP error semantics, idempotency, durable FITS/RAW/SER data-plane/storage, production guiding, durable session recovery, safety/weather/roof policy, TLS/auth/scopes/audit, sandboxed out-of-process driver hosting and a conformance suite.

See `docs/STATUS.md` and `docs/ROADMAP_P0_P1_IMPLEMENTATION.md`.


## v0.2.10.11 — Gemini EAF Windows reset-aware serial discovery

Windows HIL with a Gemini EAF on a CH340 USB-serial adapter showed that the controller can require about two seconds after `COMx` is opened before `:02#` returns `EOK#`. The native `oal.gemini` driver now keeps the COM port quiet for the configured post-open settle interval (`openSettleMs`, currently 2200 ms) before the first `:02#` probe. If that probe still fails, it keeps the same serial session open for the additional recovery window (`resetRecoveryMs`, currently 1200 ms) and retries once. This applies both to `--gemini-port COMx` and automatic serial-port scanning. Reopening between attempts is deliberately avoided because it can retrigger the USB-UART reset.

## v0.2.10.11 Windows HIL runtime/discovery notes

This hotfix addresses three issues found during the first Windows hardware-in-the-loop run:

1. Native device discovery is now cached. Normal `/api/v1/node/backends`, `/api/v1/state`, and GUI startup reads do **not** rescan USB/serial hardware. A full rescan is performed once before the node reports ready and thereafter only by the explicit **Refresh native device discovery** action (`POST /api/v1/drivers/refresh`). This prevents serial probing from causing the remote GUI's 3 s metadata timeout.
2. ZWO ASI/EAF manifests now conform to the ABI-v2 manifest contract (`schema` plus array-valued `permissions`).
3. On Windows, `QHYCCD_RUNTIME_DIR`, `ZWO_ASI_RUNTIME_DIR`, `ZWO_EAF_RUNTIME_DIR`, and `CANON_EDSDK_RUNTIME_DIR` are now active build-tree staging inputs, not packaging-only hints. DLLs from these directories are copied beside the executables and native drivers at configure time, so the development build can run without manually extending `PATH`.

When vendor SDK paths change, rerun CMake configure so runtime DLL staging is refreshed.

If you copied an older `CMakeUserPresets.json`, make sure its top-level preset schema is `"version": 2` for compatibility with CMake 3.20+ and older WSL distributions. The repository's `CMakeUserPresets.example.json` is the canonical template.

## v0.2.10.11 — Windows CRT runtime isolation

Vendor SDK runtime folders must **not** be copied wholesale into the application directory. Some QHY Windows SDK packages contain legacy `msvcr90.dll` / `msvcp90.dll`; app-local copies can conflict with the MSVC 2022 runtime and produce `R6034` before the node reaches normal startup.

v0.2.10.11 stages vendor/device DLLs but filters Microsoft CRT/UCRT redistributables. Install Microsoft redistributables normally instead of copying them from SDK folders.

After upgrading from v0.2.10.4, make one clean Windows build:

```bat
rmdir /s /q build\windows-observatory
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
powershell -ExecutionPolicy Bypass -File scripts\diagnose_windows_runtime.ps1 -BuildDir build/windows-observatory
```

### v0.2.10.11 Gemini manifest timing hotfix

Gemini/CH340 HIL showed that the driver manifest was overriding the C++ reset-settle default with `openSettleMs=150`. Runtime defaults are now synchronized at 2200 ms quiet-open settle plus a 1200 ms same-handle recovery retry. The hardware probe logs the effective timing values.


### v0.2.10.11 — graceful node shutdown on Windows

Windows HIL exposed a shutdown race when `openastrolink-node` was stopped with `Ctrl+C`: worker-thread queued wakeups could arrive while `QEventDispatcherWin32` was already tearing down its hidden message window, producing `QEventDispatcherWin32::wakeUp: Failed to post a message (Invalid window handle.)` and in some runs leaving the process stuck.

The node now installs a minimal console control handler. The handler never calls Qt; it only records the interrupt. A 50 ms Qt timer on the main thread converts the first `Ctrl+C`/`Ctrl+Break` into `QCoreApplication::quit()`. Cleanup is connected to `aboutToQuit`, so listeners, operation workers, device connections, native driver worker threads and serial sessions are stopped while the Qt event dispatcher is still valid. A second console interrupt deliberately falls through to the Windows default handler as a force-termination escape hatch.

Expected shutdown log:

```text
Console interrupt received; starting graceful shutdown. Press Ctrl+C again to force termination.
OpenAstroLink node shutdown: stopping listeners and active work
OpenAstroLink node shutdown complete
```

Windows HIL status: native Gemini EAF discovery/connection, direct focuser motion and autofocus-driven motion have now been confirmed on real hardware. Autofocus convergence/repeatability on a real optical target remains a separate validation item.


## v0.2.10.17 — urban-resilient adaptive plate solving

For light-polluted sites and mounts that cannot support a single long solver exposure, the node now provides an adaptive solve operation instead of relying on `capture once -> ASTAP once`. The default policy starts with one short exposure, then escalates to registered stacks of short exposures. Large-scale sky gradients are removed before solving, solver-frame star count is measured before expensive retries, and the last attempt always invokes the selected solver even if the local quality gate is pessimistic.

The GUI exposes **Adaptive urban capture + solve**. Its defaults are 2x2 solver binning, three registered frames for the middle attempt, five for the final attempt, a 3 s maximum single-frame exposure, and a 20-star local quality target. The base exposure and gain come from the normal Capture controls. When a mount is connected, its current RA/Dec is preferred as the ASTAP hint and the search radius expands 5° -> 10° -> configured maximum across retries.

REST clients can start the same node-local operation with `POST /api/v1/solve/adaptive`. The operation returns per-attempt diagnostics (`detectedStars`, background/noise statistics, registered frame count, effective stacked exposure, search radius and solver message) plus the `solverFrameId` used by ASTAP. The solver frame remains available through the normal frame preview endpoint for diagnosis.

### v0.2.10.17 HIL discovery/state fixes

- Persisted native USB devices are re-discovered before each auto-connect retry, so a QHY camera that appears after node startup can recover without restarting the node.
- Native Gemini moves are asynchronous at the driver boundary; `focuser.status` remains callable during motion and reports live position/`moving`. Autofocus now explicitly waits for the focuser to become idle before each exposure.
- The GUI polls focuser status during manual moves.
- Native Sky-Watcher discovery now logs each serial probe and the exact SynScan `KO -> O#` handshake failure/success, with a same-session retry for slow USB hand controllers. The current RA/DEC native mount device still targets the SynScan hand-controller protocol; direct USB/EQDIR motor-controller transport remains a separate incomplete path.
