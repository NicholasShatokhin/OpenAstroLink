# Project manifest — OpenAstroSuite / OpenAstroLink v0.2.10.19

English is the canonical documentation language. `PROJECT_MANIFEST_UA.md` is the Ukrainian mirror.

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostics: `oal-hardware-probe`
- Core library: `oas_core`
- Protocol / native driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.10.19-hil-start-catalog-mount-fix`
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
- `oal.skywatcher` — existing Sky-Watcher SynScan / EqMount native mount path.
- `oal.eqdrive` — experimental dual-protocol EQDrive serial path (EQMOD-compatible Motor Controller first, official ASTEP fallback); sync-anchor celestial model in this release.
- `oal.simulated` — reference ABI-v2 simulated devices.

Gemini EAF has passed basic Windows HIL for discovery, connection and motion; the other physical native drivers remain HIL-pending or only partially qualified on their actual device/firmware/host combinations.

## Compatibility adapters

Classic ASCOM (Windows helper/Chooser), SynScan App network, SynScan serial-protocol-over-TCP, INDI, ASCOM Alpaca, LX200, OpenCV/UVC and OAL remote-device adapters remain available. Native OAL is preferred; INDI remains intentionally easy to enable for equipment not yet covered natively.

## Current implemented foundations

- Local/remote GUI with node-owned hardware.
- Per-device disconnect and state snapshot/reconnect.
- Operation manager with queue/run/success/fail/cancel, progress and resource locks.
- Async mount slew, main/guide exposure, autofocus and urban-resilient adaptive plate solving.
- Main + guide camera roles and optical profiles.
- Native OAL ABI-v2 registry/capabilities/events/cancellation/frame publication.
- Stellarium mount position/GOTO TCP bridge.
- Cross-platform presets/scripts for Windows/Linux/RPi.

## v0.2.10.17 adaptive urban plate solving

- Node-owned `solver.adaptive` operation captures short exposures, evaluates solver-frame quality, registers drift between frames, stacks accepted frames, removes large-scale sky gradients and retries the selected plate solver.
- Camera-side binning is carried in `CameraFrame`; ASTAP FOV derivation is now binning-aware.
- When available, current mount RA/Dec is used as the default solve hint and the search radius expands across attempts.
- REST endpoint: `POST /api/v1/solve/adaptive`; remote GUI exposes **Adaptive urban capture + solve**.
- The final prepared solver frame and per-attempt diagnostics are retained for troubleshooting.


## v0.2.10.17 mount interoperability

- Native ABI-v2 driver `oal.eqdrive` keeps `oal.skywatcher` intact and now probes the EQMOD-compatible Sky-Watcher Motor Controller transport first, with the published EQDrive ASTEP motor-control section as a fallback.
- Native EQDrive celestial coordinates use `sync-anchor-v2`; one known-position Sync is required before RA/Dec GOTO. `coordinateValid=false` prevents unsynced coordinates from being used as ASTAP hints or published to Stellarium.
- Windows `ascom-classic` backend runs COM automation in the separate native `oas-ascom-host.exe`, with ASCOM Chooser and driver setup UI available in the local GUI.
- `synscan-wifi` talks directly to the mount/EQDrive Wi-Fi adapter using the Motor Controller protocol over UDP 11880; `synscan-app` implements the SynScan App Protocol to a running SynScan Pro host over UDP 11881.
- Native serial discovery includes EQDrive and persisted serial-port migration; selecting a new Gemini COM port updates the persisted native backend binding after successful rediscovery.
- Documentation: `docs/EQDRIVE.md`, `docs/ASCOM_CLASSIC.md`, `docs/SYNSCAN_NETWORK.md` and Ukrainian mirrors.


## v0.2.10.17 HIL camera/mount/network reliability

- QHY single-frame readout follows the vendor Exp/Get/Cancel lifecycle, has a cancellation watchdog, SDK control readback, and frame statistics logging.
- Native reconnect discovery is filtered per missing driver so active QHY sessions and ASCOM-owned mount COM ports are not disturbed by an unrelated missing device.
- SynScan endpoints are per-backend: direct `synscan-wifi` auto-discovers the adapter on UDP 11880, while `synscan-app` auto-discovers the SynScan Pro host on UDP 11881; TCP 11882 is documented as an app-host compatibility service only.
- Classic ASCOM GOTO logs coordinate/pier-side preflight data; no RA/DEC sign inversion is introduced.
- Focused EQDrive diagnostics cover all DTR/RTS combinations and multiple read-only ASTEP identity/motor queries.
- Gemini stale COM bindings can auto-migrate after unique rediscovery, while explicit COM selection refreshes only the Gemini driver.

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


## Windows HIL update v0.2.10.11

- Gemini EAF: native discovery, connection, direct motion and autofocus-driven motion are confirmed on real Windows/USB-serial hardware.
- Node shutdown: `Ctrl+C` is converted into a main-thread graceful shutdown before the Qt event dispatcher is destroyed; a second `Ctrl+C` remains a force-termination escape hatch.

## v0.2.10.17 HIL status/recovery
- graceful Ctrl+C shutdown is HIL-confirmed;
- Stellarium bridge accepts GOTO and forwards RA/Dec into the node;
- QHY auto-connect retries now refresh native discovery first;
- Gemini moves no longer block status until physical completion; autofocus explicitly waits for idle;
- GUI polls live focuser position/moving during manual motion;
- Sky-Watcher native discovery now emits detailed SynScan serial handshake diagnostics and same-session retry.
- Important: the native RA/DEC Sky-Watcher device currently targets the SynScan hand-controller protocol. Direct USB/EQDIR motor-controller transport is not yet exposed as a full mount backend.

## v0.2.10.19 live native catalogue / direct Wi-Fi

- Backend catalogues are carried in node state and remote GUI comboboxes update after native hot-plug/discovery.
- `synscan-wifi` is direct Motor Controller UDP/11880 to the mount/EQDrive Wi-Fi adapter; `synscan-app` remains UDP/11881 to SynScan Pro.
- `oal.eqdrive` is dual-protocol: EQMOD-compatible Sky-Watcher Motor Controller serial transport is tried first and official EQDrive ASTEP is retained as a fallback; existing `oal.skywatcher` is retained. Direct `synscan-wifi` uses the same Motor Controller command set over UDP 11880.
- Native EQDrive GOTO requires Sync and is HIL safety-limited; park/pier/pulse-guide remain unqualified.
