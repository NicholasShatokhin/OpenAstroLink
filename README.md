# OpenAstroSuite / OpenAstroLink

**v0.2.9 — ZWO native drivers, Stellarium bridge, dual-camera and optical-train profiles**

English is the canonical project documentation language. Ukrainian mirrors are provided in `README_UA.md` and `docs/uk/`.

OpenAstroLink (OAL) is a modern, local-first observatory control stack. Native OAL drivers are the reference hardware path; INDI, ASCOM Alpaca and LX200 remain optional compatibility layers for equipment that does not yet have a native OAL driver.

## Runtime architecture

```text
OpenAstroSuite GUI
  ├─ local GUI on the Raspberry Pi
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
      ├─ oal.canon        → USB/PTP/libgphoto2 → Canon EOS
      ├─ oal.zwo.asi      → ZWO ASI SDK → ZWO ASI camera
      ├─ oal.zwo.eaf      → ZWO EAF SDK → ZWO EAF focuser
      ├─ oal.gemini       → USB serial → Gemini EAF
      └─ oal.skywatcher   → SynScan serial → Sky-Watcher mount

Compatibility path, enabled when needed:
      INDI / ASCOM Alpaca / LX200
```

The node owns the hardware and long-running operations. Closing a GUI does not stop the node or disconnect the equipment.

## v0.2.9 additions

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

- `rpi4-native-release`: native hardware path, INDI disabled.
- `rpi4-observatory-release`: native hardware path plus INDI compatibility.
- Windows presets leave vendor SDK drivers optional unless their SDK paths are supplied.

ZWO options:

```text
OAS_ENABLE_NATIVE_ZWO_ASI
OAS_ENABLE_NATIVE_ZWO_EAF
ZWO_ASI_ROOT / ZWO_ASI_INCLUDE_DIR / ZWO_ASI_LIBRARY
ZWO_EAF_ROOT / ZWO_EAF_INCLUDE_DIR / ZWO_EAF_LIBRARY
```

## Compatibility policy

INDI is deliberately easy to enable because it provides broad equipment coverage. It is not a dependency of the native OAL drivers and does not define OAL capabilities or semantics. The goal is: native OAL where available, compatibility adapter where necessary, one observatory model above both.

## Current maturity

The architecture and source-level checks are ahead of hardware validation. Native QHY/Canon/Gemini/Sky-Watcher and the new ZWO drivers require hardware-in-the-loop qualification on the target Raspberry Pi and actual devices before being labelled production-ready. The Stellarium bridge should likewise be tested against the user's installed Stellarium version and real mount.

See `docs/STATUS.md`, `docs/VALIDATION.md`, `docs/ZWO_NATIVE.md`, `docs/STELLARIUM.md`, and `docs/OPTICAL_TRAINS_AND_DUAL_CAMERAS.md`.
