# Project manifest — OpenAstroSuite / OpenAstroLink v0.2.10.10

English is the canonical documentation language. `PROJECT_MANIFEST_UA.md` is the Ukrainian mirror.

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostics: `oal-hardware-probe`
- Core library: `oas_core`
- Protocol / native driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.10.10-gemini-manifest-timing-fix`
- Language: C++20
- Minimum CMake: 3.20
- Preset schema: v2
- Windows toolchain: MSVC 2022 x64 + Ninja
- UI: Qt 6.4+
- Image processing: OpenCV 4
- REST: Qt HTTP Server
- Events: Qt WebSockets
- Async execution: Qt Concurrent + `OperationManager`
- Native driver ABI: C ABI v2

## Native OAL drivers

- `oal.qhy` — QHY cameras through QHYCCD SDK; Windows vendor-header isolation included.
- `oal.canon` — Canon EOS through Canon EDSDK on Windows or USB/PTP/libgphoto2 on Linux.
- `oal.zwo.asi` — ZWO ASI cameras through ZWO ASI SDK.
- `oal.zwo.eaf` — ZWO EAF focusers through ZWO EAF SDK.
- `oal.gemini` — Gemini EAF native serial path.
- `oal.skywatcher` — Sky-Watcher SynScan native mount path.
- `oal.simulated` — reference ABI-v2 simulated devices.

All physical native drivers remain HIL-pending on the actual device/firmware/host combination.

## Compatibility adapters

INDI, ASCOM Alpaca, LX200, OpenCV/UVC and OAL remote-device adapters remain available. Native OAL is preferred; INDI remains intentionally easy to enable for equipment not yet covered natively.

## Current implemented foundations

- Local/remote GUI with node-owned hardware.
- Per-device disconnect and state snapshot/reconnect.
- Operation manager with queue/run/success/fail/cancel, progress and resource locks.
- Async mount slew, main/guide exposure and autofocus.
- Main + guide camera roles and optical profiles.
- Native OAL ABI-v2 registry/capabilities/events/cancellation/frame publication.
- Stellarium mount position/GOTO TCP bridge.
- Cross-platform presets/scripts for Windows/Linux/RPi.

## Current major gaps

- Idempotency.
- Full RFC 9457 HTTP error model.
- Replayable sequenced WebSocket events.
- TLS/auth/roles/scopes/audit and observatory safety policy.
- Durable science FITS/RAW/SER data plane.
- Production guiding.
- Durable scheduler/recovery.
- Out-of-process driver sandbox/crash recovery.
- Public conformance suite.

## Documentation

Canonical: `README.md`, `PROJECT_MANIFEST.md`, `docs/*.md`.  
Ukrainian mirrors: `README_UA.md`, `PROJECT_MANIFEST_UA.md`, `docs/uk/*.md`.  
Normative implementation snapshot: `docs/OAL_SPECIFICATION.md`.  
New-chat handoff: `docs/NEW_CHAT_HANDOFF.md` and `docs/uk/NEW_CHAT_HANDOFF.md`.
