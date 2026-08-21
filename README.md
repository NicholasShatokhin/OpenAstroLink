# OpenAstroSuite / OpenAstroLink

**v0.2.10 — cross-platform desktop observatory builds (Windows x64, Linux x86_64, Linux ARM64/RPi)**

English is the canonical project documentation language. Ukrainian mirrors are provided in `README_UA.md` and `docs/uk/`.

OpenAstroLink (OAL) is a modern, local-first observatory control stack. Native OAL drivers are the reference hardware path; INDI, ASCOM Alpaca and LX200 remain optional compatibility layers for equipment that does not yet have a native OAL driver.


## v0.2.10 cross-platform deployment

A Raspberry Pi is no longer a special requirement. If the hardware is attached directly to a Windows or Linux computer, that computer can run `openastrolink-node` and own the observatory hardware. The GUI can run on the same machine or remotely.

```text
QHY / Canon / ZWO / Gemini / Sky-Watcher
                    │ USB / serial
                    ▼
             openastrolink-node
               ┌────┴─────┐
          local GUI   remote GUI + Stellarium
```

Canon transport is selected per platform: `AUTO` resolves to Canon EDSDK on Windows and libgphoto2 on Linux. INDI remains independently switchable as a compatibility layer. Portable CMake presets are kept in `CMakePresets.json`; developer SDK paths belong in ignored `CMakeUserPresets.json`.

See `docs/BUILD_PLATFORMS.md` for exact build and packaging commands.

## Runtime architecture

```text
OpenAstroSuite GUI
  ├─ local GUI on the observatory host (Windows/Linux/RPi)
  └─ remote GUI on another computer
              │
        OAL HTTP / WebSocket
              │
      openastrolink-node
              │
      AstroCore / operations
              │
      Native OAL ABI v2 registry
      ├─ oal.qhy          → QHYCCD SDK → QHY camera
      ├─ oal.canon        → Canon EDSDK (Windows) / USB/PTP-libgphoto2 (Linux) → Canon EOS
      ├─ oal.zwo.asi      → ZWO ASI SDK → ZWO ASI camera
      ├─ oal.zwo.eaf      → ZWO EAF SDK → ZWO EAF focuser
      ├─ oal.gemini       → USB serial → Gemini EAF
      └─ oal.skywatcher   → SynScan serial → Sky-Watcher mount

Compatibility path, enabled when needed:
      INDI / ASCOM Alpaca / LX200
```

The node owns the hardware and long-running operations. Closing a GUI does not stop the node or disconnect the equipment.

## v0.2.10 additions

- Native `oal.zwo.asi` camera driver with discovery, identity/capabilities, exposure, ROI, binning, gain/offset, cancellation and native frame publication.
- Native `oal.zwo.eaf` focuser driver with absolute/relative movement, halt, position/moving state, temperature, backlash/reverse/max-step capabilities.
- Two independent camera roles: `main` and `guide`. They can use different vendors/backends and can be connected simultaneously.
- Independent resource locks: `camera` and `camera.guide`, so guide acquisition does not inherently block a main-camera exposure.
- Main and guide optical-train profiles: aperture, effective focal length, pixel size, sensor dimensions and derived f-ratio / image scale.
- Main telescope optical design and optional central obstruction metadata.
- Stellarium Telescope Control TCP bridge, default port `10000`, for mount position reporting and GOTO commands.
- Stellarium bridge can be enabled locally, remotely over OAL, from node settings, or with `--stellarium-port`.
- English-first documentation policy with Ukrainian mirrors.

## Two-camera operation

A typical configuration is:

```text
Main train:  telescope → QHY / Canon EOS / ZWO ASI
Guide train: guide scope or OAG → ZWO ASI / QHY / other OAL/INDI camera
```

The device registry reports camera `role` as `main` or `guide`. Main imaging uses resource lock `camera`; guide capture uses `camera.guide`. The current guider still needs a full calibration/star-tracking state machine before it should be considered a production autoguider; v0.2.9 establishes the correct hardware/resource architecture for that work.

## Stellarium

Enable the bridge in **OAL Server → Stellarium Telescope Control bridge**, or on the node:

```bash
openastrolink-node --http-port 8080 --ws-port 8090 --stellarium-port 10000
```

In Stellarium Telescope Control configure an external telescope server at the node IP and TCP port `10000`.

The standard Stellarium telescope protocol controls the mount only: OAL maps telescope position and GOTO. Camera capture, focus, autofocus, polar alignment, guiding and observing sessions continue to use OAL. A future dedicated Stellarium OAL plug-in can expose the full observatory feature set.

## Optical profile

The profile now stores two optical trains. For the main telescope set the clear aperture/primary mirror diameter, effective focal length (including reducer/Barlow), optical design, optional central obstruction, main camera pixel size and sensor dimensions. For the guide train set guide aperture, guide focal length and guide-camera pixel/sensor data.

Derived values are calculated by the core:

```text
f-ratio = focal_length / aperture
image_scale_arcsec_per_pixel = 206.265 × pixel_size_um / focal_length_mm
```

These values are useful for solver hints, guiding, sampling diagnostics and future automatic configuration validation.

## Build presets

First-class presets now cover desktop and edge deployments:

- `windows-core-release`, `windows-native-release`, `windows-observatory-release`;
- `linux-native-release`, `linux-observatory-release`, `linux-node-release`;
- `rpi4-native-release`, `rpi4-observatory-release`;
- `node-sim-release`.

Copy `CMakeUserPresets.example.json` to `CMakeUserPresets.json` for Qt/OpenCV/QHY/ZWO/Canon SDK paths. The local file is ignored by Git.

## Compatibility policy

INDI is deliberately easy to enable because it provides broad equipment coverage. It is not a dependency of the native OAL drivers and does not define OAL capabilities or semantics. The goal is: native OAL where available, compatibility adapter where necessary, one observatory model above both.

## Current maturity

The architecture and source-level checks are ahead of hardware validation. Native QHY/Canon/Gemini/Sky-Watcher and the new ZWO drivers require hardware-in-the-loop qualification on each target host OS/architecture and the actual devices before being labelled production-ready. The Stellarium bridge should likewise be tested against the user's installed Stellarium version and real mount.

See `docs/STATUS.md`, `docs/VALIDATION.md`, `docs/ZWO_NATIVE.md`, `docs/STELLARIUM.md`, and `docs/OPTICAL_TRAINS_AND_DUAL_CAMERAS.md`.

### v0.2.10.2 QHY/MSVC header-isolation hotfix

The Windows QHY target no longer adds the QHY SDK `include` directory to MSVC's global include search path. Some QHY All-In-One SDK releases contain compatibility headers named `stdint.h` / `stdint_windows.h`; when that directory is passed through `/I`, MSVC's own `<cstdint>` can accidentally resolve to the vendor header and break `std::int64_t`, `std::chrono`, and other STL types. OAL now generates a private absolute-path wrapper for `qhyccd.h` and keeps vendor headers isolated from the CRT/STL search path.
