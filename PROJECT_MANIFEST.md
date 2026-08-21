# Project manifest — OpenAstroSuite / OpenAstroLink v0.2.9

English is the canonical documentation language. `PROJECT_MANIFEST_UA.md` is the Ukrainian mirror.

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostics: `oal-hardware-probe`
- Core library: `oas_core`
- Protocol / native driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.9-zwo-stellarium-dual-optics`
- Language: C++20
- UI: Qt 6.4+
- Image processing: OpenCV 4
- REST: Qt HTTP Server
- Events: Qt WebSockets
- Async execution: Qt Concurrent + `OperationManager`
- Native driver ABI: C ABI v2

## Native OAL drivers

- `oal.qhy` — QHY cameras through QHYCCD SDK.
- `oal.canon` — Canon EOS through USB/PTP/libgphoto2 transport.
- `oal.zwo.asi` — ZWO ASI cameras through ZWO ASI SDK.
- `oal.zwo.eaf` — ZWO EAF focusers through ZWO EAF SDK.
- `oal.gemini` — Gemini EAF native serial path.
- `oal.skywatcher` — Sky-Watcher SynScan native mount path.
- `oal.simulated` — reference ABI-v2 simulated devices.

## Compatibility adapters

INDI, ASCOM Alpaca, LX200, OpenCV/UVC and OAL remote-device adapters remain available. Native OAL is preferred but INDI is intentionally easy to compile and enable for broad third-party equipment coverage.

## Multi-camera roles

The observatory core has independent `main` and `guide` camera roles. Their operation resources are `camera` and `camera.guide`. Both can be connected concurrently, including two devices from the same multi-device native driver.

## Optical profiles

`TelescopeProfile` stores the main optical train and a separate guide optical train: aperture, effective focal length, camera pixel/sensor geometry, derived f-ratio and image scale, plus main optical design and optional central obstruction.

## Stellarium

`StellariumTelescopeServer` implements the external Stellarium Telescope Control TCP bridge for mount position and GOTO. Default TCP port: `10000`. Full camera/focuser/workflow control remains OAL-native.

## Documentation policy

Canonical: `README.md`, `PROJECT_MANIFEST.md`, `docs/*.md`.
Ukrainian mirrors: `README_UA.md`, `PROJECT_MANIFEST_UA.md`, `docs/uk/*.md`.
Machine-readable API/ABI identifiers remain English.
