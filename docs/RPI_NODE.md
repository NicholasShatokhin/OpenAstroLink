## v0.2.10.50 Raspberry Pi 4/5 ARM64 status

OpenAstroLink treats 64-bit Raspberry Pi as a generic Linux `aarch64` target. Pi 4 and Pi 5 therefore share the same OAL ABI and vendor ARM64 SDK matrix. The historical `rpi4-*` preset/sysroot names are retained for backward compatibility; they do not encode a Cortex-A72-only binary. The current WSL/Linux→ARM64 build has reached 100% for `openastrolink-node`, `oal-hardware-probe` and native QHY/Canon/ZWO/Gemini/Sky-Watcher/EQDrive drivers. Physical Pi 5 runtime/HIL and the ARM64 `OpenAstroSuite` GUI runtime are still pending.

# Raspberry Pi 4/5 observatory node — v0.2.10.50

Version 0.2.10 keeps the v0.2.2 process boundary and makes the Raspberry Pi hardware stack **native-OAL-first**: QHY, Canon EOS, Gemini EAF and Sky-Watcher are available as ABI-v2 native OAL plugins; INDI remains an optional compatibility path for additional equipment.

## Runtime model

```text
RPi4
  openastrolink-node (systemd, headless)
      ApplicationController
      algorithms + active devices
      HTTP :8080 / WebSocket :8090
           ▲
           │ localhost or LAN/VPN
           │
  OpenAstroSuite GUI (optional local monitor/keyboard)

Remote PC
  OpenAstroSuite GUI ───────────────► same node
```

The GUI is not the owner of hardware in node mode. `capture`, `solve`, `autofocus`, guiding, polar-alignment sampling/estimation and session state are invoked through the node API and execute on the Raspberry Pi. Closing the remote GUI therefore does not destroy the node process or release its devices. The GUI also performs a basic WebSocket reconnect if the event channel drops; sequence/replay recovery remains the later P0 event-hardening task.

An **Embedded core** mode is retained for development and offline desktop use, but the recommended Raspberry Pi deployment is the systemd node plus a GUI connected to `http://127.0.0.1:8080`.

## Build on Raspberry Pi OS 64-bit

Install Qt 6/OpenCV development packages available for your Raspberry Pi OS release, then configure the project. QHY additionally requires the vendor QHYCCD SDK headers/library on the Pi.

```bash
cmake -S . -B build-rpi -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOAS_BUILD_GUI=ON \
  -DOAS_BUILD_NODE=ON \
  -DOAS_ENABLE_QHY=ON \
  -DOAS_ENABLE_INDI=OFF \
  -DOAS_ENABLE_GPHOTO2=OFF
cmake --build build-rpi -j$(nproc)
```

The standard Raspberry Pi build is native-first: INDI is disabled unless explicitly requested. For the first architecture test, QHY may also be disabled and simulated devices used; then enable the native hardware backends.

The systemd user must have permission to open the telescope interfaces. On Raspberry Pi OS this normally means membership in the relevant serial/USB groups (commonly `dialout`, and depending on device/udev rules also `plugdev`/`video`) plus the QHY vendor udev rules. Verify actual permissions with `ls -l /dev/ttyUSB* /dev/ttyACM*` and the QHY SDK installation rather than assuming a group name.

## Install the service

```bash
sudo ./scripts/install_rpi_node.sh build-rpi
systemctl status openastrolink-node
journalctl -u openastrolink-node -f
```

The service template starts:

```text
openastrolink-node --http-port 8080 --ws-port 8090
```

The node listens on the LAN. The current v0.2.10 API has **no production authentication/TLS yet**, so do not expose ports 8080/8090 directly to the public Internet. Use a trusted isolated LAN or VPN until P0 security is implemented.

## Local GUI with monitor and keyboard

Run from the build tree:

```bash
./build-rpi/OpenAstroSuite
```

If installed with `scripts/install_rpi_node.sh`, the installer also installs a desktop launcher **OpenAstroSuite (Local Observatory)** when the GUI binary is present.

Choose **This computer — local OAL node**. The GUI connects to `http://127.0.0.1:8080`; the core remains the systemd service.

This is the preferred mode on a Raspberry Pi with an attached monitor because the same core remains available to remote clients and continues running if the GUI is closed.

## Remote GUI

On another computer run the same GUI and choose **Remote OAL node**, for example:

```text
http://openastrolink-rpi.local:8080
```

or launch directly:

```bash
OpenAstroSuite --node http://192.168.1.50:8080
```

The GUI discovers available camera/mount/focuser/solver backends from the node. Connecting a device from the remote GUI configures the device **on the Raspberry Pi**, not on the client computer.

## Persistent device bindings

After a successful camera, mount or focuser connection the node persists:

- backend name;
- endpoint;
- `autoConnect=true`.

At the next boot `openastrolink-node` restores those bindings. If an INDI server or USB device is not ready yet, the node remains reachable and retries missing persisted devices every 10 seconds. Use `--no-autoconnect` for maintenance/troubleshooting.

Typical Raspberry configuration for the planned hardware is now conceptually:

```text
Camera:  native:oal.qhy / native:oal.canon / native:oal.zwo.asi
Focuser: native:oal.gemini / native:oal.zwo.eaf
Mount:   native:oal.eqdrive / native:oal.skywatcher; INDI/LX200 only when explicitly needed
```

Native devices own their hardware transport and therefore do not require an endpoint text field in the GUI. Exact INDI device names must still match what compatibility drivers publish.

## v0.2.10 native-driver update

The process/node architecture is now paired with:

- ABI-v2 native driver registry and manifests;
- native `oal.simulated` reference camera/mount/focuser;
- native `oal.qhy` using the QHYCCD SDK directly below OAL;
- `AstapSolver` CLI adapter;
- INDI compatibility discovery and standard mount/focuser mappings;
- `oal-hardware-probe` reporting native drivers/devices separately from compatibility devices;
- node installer deployment of native plugin libraries/manifests.

See `docs/NATIVE_DRIVER_SDK.md` and `docs/RPI_FIRST_HARDWARE.md`. Native Gemini/mount, planetary SER, async solve, persistent DSO/planetary/mosaic scheduling and guided Polar Alignment foundations are now implemented. The nearest Beta work is HIL qualification of autofocus, auto-exposure, scheduler, mosaic and Polar Alignment. Production guiding, full unattended safety/security and mid-block durable recovery remain OAL 1.0 work.


Native Canon EOS is provided by `oal.canon`. On Raspberry Pi it links directly to libgphoto2 for USB/PTP transport; INDI is not part of this camera path.

## v0.2.10 node integrations

The node can own two cameras simultaneously (`main` and `guide`) and persists both bindings. The Stellarium bridge can be enabled with `--stellarium-port 10000` or through OAL settings/API. ZWO ASI/EAF native drivers may be loaded alongside QHY/Canon/Gemini/Sky-Watcher and optional INDI compatibility devices.

## Build/cross presets — v0.2.10.50
> build-fix7: the Bookworm signing-key fetch helper is now safe under `set -u`; the `name: unbound variable` failure is fixed.


For a native 64-bit Raspberry Pi 4/5 node:

```bash
cmake --preset rpi4-node-release
cmake --build --preset rpi4-node-release -j"$(nproc)"
```

For cross-compilation from Linux/WSL, build-fix6 can bootstrap the sysroot automatically. On Jammy it also compensates for the stale host Debian keyring by fetching and fingerprint-verifying the official Bookworm signing keys before `debootstrap`:

```bash
./scripts/bootstrap_rpi_cross.sh arm64
./scripts/build_rpi_cross.sh arm64
```

The default bootstrap creates a Debian 12/Bookworm ARM64 root using `debootstrap`/`qemu-user-static`, installs Qt 6.4.2 + HttpServer/WebSockets/SerialPort, OpenCV, libusb and JPEG development files, and prepares matching x86_64 Qt host tools. To mirror a physical Pi instead, use:

```bash
./scripts/bootstrap_rpi_cross.sh arm64 --from-pi pi@openastrolink.local
```

If `../QHYCCD_Linux_New` is present, the bootstrap stages a matching QHY ARM SDK under `~/.local/share/openastrolink/sdk/` after verifying its ELF architecture. ZWO and Canon remain supplied from their vendor multi-architecture SDK trees through local presets.

For cross-compilation from Windows with the existing Arm GNU Toolchain installation:

```powershell
.\scripts\build_rpi_cross.ps1 -Arch arm64 -Sysroot C:\path\to\rpi-arm64-sysroot
```

Cross presets build the headless node first and keep QHY/ZWO/Canon/INDI disabled until matching ARM target libraries are explicitly supplied. This is deliberate: a cross compiler cannot safely consume the x86_64 Linux SDK libraries used by a WSL workstation build.


### v0.2.10.49 build-fix8 — target Qt/OpenCV discovery in cross sysroots

The ARM cross toolchains now pin the Debian multiarch (`aarch64-linux-gnu` / `arm-linux-gnueabihf`) and automatically seed `Qt6_DIR` and `OpenCV_DIR` from target configs inside `OAS_CROSS_SYSROOT`. Host Qt (`OAS_QT_HOST_PATH`) remains host-tools-only (`moc/rcc/uic`); host and target Qt are never mixed. The bootstrap environment record also persists target package paths for repeatable builds.


<!-- build-fix8-target-package-discovery -->
### build-fix8: target package discovery

After the ARM sysroot bootstrap, CMake uses target `Qt6Config.cmake` and `OpenCVConfig.cmake` from the Debian multiarch directory, while `OAS_QT_HOST_PATH` is used only for host `moc/rcc/uic`. For ARM64 the standard paths are `usr/lib/aarch64-linux-gnu/cmake/Qt6` and `usr/lib/aarch64-linux-gnu/cmake/opencv4` inside the sysroot.


### v0.2.10.49 build-fix9 — QHY stage is part of readiness

For the full ARM64 vendor preset, QHY staging is no longer best-effort. The bootstrap exits on a missing/invalid QHY stage and only records `OAS_QHY_SDK` after `qhyccd.h`, `libqhy.so`, and target ELF architecture have been verified. A base ARM build can still opt out with `--no-qhy`.

### v0.2.10.49 build-fix10 — QHY ARM64 vs ARMHF

The legacy `QHYCCD_Linux_New` `armv8` SDK is physically a 32-bit ARM/EABI build. Use it with the ARMHF node only. A 64-bit Raspberry Pi node must use a genuine QHY `Arm_64`/`AARCH64` SDK; until that SDK is staged, build the ARM64 node with QHY disabled (`my-rpi4-cross-arm64`).

### build-fix17 — native-first INDI policy

`rpi4-observatory-release` and `rpi4-node-release` now keep INDI disabled by default. Use `rpi4-observatory-indi-release` / `rpi4-node-indi-release` only for INDI-only equipment. Native QHY/Canon/ZWO/Gemini/Sky-Watcher/EQDrive paths remain the preferred paths.
