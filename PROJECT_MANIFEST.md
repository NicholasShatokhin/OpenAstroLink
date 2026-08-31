# Project manifest — OpenAstroSuite / OpenAstroLink v0.2.10.38

## v0.2.10.38

- Package: `0.2.10.38-mount-site-shared-park`
- Core version: `0.2.10.38`
- Native GOTO safety uses true sky separation; raw transport retains a separate 180° mechanical cap.
- Classic ASCOM site/time is verified and synchronized before GOTO when the driver permits site writes; EQMOD gets an actionable `Allow Site Writes` diagnostic otherwise.
- Classic ASCOM adds driver Az/Alt/LST telemetry and an explicit EQMOD `equOther(0)` → topocentric/JNow compatibility assumption.
- Persistent park calibration is unified at the OAL control surface: native stores Home+Park, Classic ASCOM calls standard `SetPark`.
- Remote profile supports preferred `maxGotoSkyDeltaDeg` plus the legacy `maxGotoAxisDeltaDeg` alias.

## Package hotfix v0.2.10.36.1

- Package: `0.2.10.36.1-gui-qtimer-msvc-hotfix`
- Core version remains: `0.2.10.36`
- Fix: remove the invalid explicit Qt 6.10 `QTimer::timeout()` signal invocation after starting the mount clock timer.

English is the canonical documentation language. `PROJECT_MANIFEST_UA.md` is the Ukrainian mirror.

- Package: `0.2.10.36-mount-diagnostics-ser-overlays`
- Core version: `0.2.10.36`
- Main HIL focus: mount coordinate diagnostics, ASCOM telemetry/site-time, sidereal/lunar/solar tracking, SER and Live View measurement overlays

## v0.2.10.36

- Raw pre-debayer SER recording for Live View.
- Optional Mil-Dot and angular measurement overlays.
- Sidereal/Lunar/Solar tracking rates across native EQDrive/direct SynScan and Classic ASCOM where supported.
- Configurable native raw-axis GOTO qualification limit.
- Full mount coordinate/site/time/LST/backend diagnostic logging and near-pole Sync warning.
- Classic ASCOM live coordinate validity for Stellarium plus explicit site/time synchronization.
- Remote Live View stale-preview race mitigation.


## Previous package history

# Project manifest — OpenAstroSuite / OpenAstroLink v0.2.10.35.1 hotfix package

English is the canonical documentation language. `PROJECT_MANIFEST_UA.md` is the Ukrainian mirror.

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostics: `oal-hardware-probe`
- Core library: `oas_core`
- Protocol / native driver framework: `OpenAstroLink (OAL)`
- Package: `0.2.10.35.1-zwo-msvc-hotfix`
- Core version: `0.2.10.35-focus-debayer-coordinates`
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

## v0.2.10.35 focus preview, optional debayer, synchronized coordinates

- Operational autofocus preview in the main camera pane and manual focuser jog controls.
- More stable Scene autofocus metric and coarse/fine peak selection.
- Preview-only optional debayer with Auto CFA metadata plus explicit RGGB/BGGR/GRBG/GBRG; QHY uses `CAM_COLOR`, ZWO uses `BayerPattern`; science RAW/FITS is untouched.
- Synchronized J2000/JNow/Az-Alt/Galactic target fields and a Polaris preset.
- Saturation is an image-quality warning, never a camera/transport error.

## v0.2.10.34 QHY live-stream and health stability

- Native QHY continuous SDK streaming transport for Live/Finder.
- Automatic restore to QHY single-frame mode when Live View ends.
- Operation-aware native health scheduling; QHY idle health uses a three-strike probe instead of `GetQHYCCDChipInfo`.
- Cached native camera sensor geometry to avoid post-readout vendor capability calls.
- Daylight-safe Live/Finder defaults and saturated/dark-frame diagnostics.
- Remote capture transport preserves `saveRaw` / `savePath`, so QHY FITS science spooling works through the remote GUI as well as in-process.

## v0.2.10.33 live/finder and scene autofocus

- Continuous node-side Live View operation with camera resource locking and remote frame delivery.
- Live / Finder GUI with auto-stretch, crosshair, bright-region locator and finder-alignment wizard.
- Scene autofocus with configurable exposure/gain; star autofocus rejects fields without enough stars.
- Preview cache for remote live-frame fetches.
- Static `site/` foundation for `openastro.link` (English canonical + Ukrainian mirror).
- Canon repeated-still Live View is blocked until EDSDK EVF is implemented.

## v0.2.10.32 hardware HIL safety corrections

- Background liveness probes + hot-remove state cleanup for active QHY, Gemini and EQDrive devices.
- Core FITS science spool for user captures when a native camera does not provide an original file path (QHY).
- EQDrive instant ABORT with stop verification; explicit disconnect stops motion first.
- Mechanical Park requires an explicitly calibrated current-axis target; unpark during parking aborts physical motion.
- Safer Sync UX, plate-solve Sync helper, axis mapping reversal controls, and temporary 15° sky-GOTO qualification envelope.

## v0.2.10.32 adaptive solver stability / histogram

- Software fallback binning for solver frames when hardware binning is unavailable.
- Bounded adaptive capture phase and Canon inter-frame settling.
- Histogram/quality-driven next-exposure selection (gain/ISO remains explicit).
- Optional GUI preview histogram with clipping statistics and next-exposure suggestion/auto-apply.
- Reduced full-state broadcasts during operation progress to protect HTTP/WS responsiveness.

## v0.2.10.29 Canon hot-plug settle/retry recovery

- EOS 550D HIL confirmed that EDSDK `camera-added` may precede visibility in `EdsGetCameraList()` by more than the original short settle window.
- `ApplicationController` now debounces the callback and retries only `oal.canon` at approximately 0.7 / 2.2 / 4.5 / 8 seconds.
- The last retry may hard-reload only the idle Canon EDSDK driver after a zero-device scan; QHY and serial drivers are not probed by this recovery path.
- Focused hard-recovery requests retain their driver scope when queued behind an already-running native discovery.
- Includes the v0.2.10.27 Qt 6.10/MSVC capture-result build correction.

## v0.2.10.26 Canon automatic rediscovery + ISO gain
- EDSDK `camera-added` now becomes an OAL `device.discoveryHint`, consumed by `ApplicationController` as an asynchronous driver-scoped `oal.canon` rediscovery.
- Windows Canon OAL `gain` is implemented as ISO using the camera-advertised `kEdsPropID_ISOSpeed` descriptor.
- `CameraFrame` carries `scienceFilePath`; exposure operation results and the GUI surface the original CR2/CR3 path.
- Operational Canon preview remains the input to autofocus/plate solve; original RAW remains the science artifact.

## v0.2.10.25 Canon EOS 550D HIL follow-up

- Pending EDSDK camera-added events receive a bounded enumeration settle/retry window on explicit Refresh and are not cleared until a camera is actually enumerable.
- Explicit all-device Refresh remains vendor-neutral and can hard-reload inactive Canon/EDSDK after a zero-device scan; no periodic Canon probing was introduced.
- EOS 550D long exposure now prefers held `Completely_NonAF` shutter while Tv=Bulb; `BulbStart/BulbEnd` is compatibility fallback because the 550D returned EDSDK `0x60` (`INVALID_PARAMETER`) for `BulbStart`.
- Successful RAW transfer no longer fails solely because `EdsDownloadThumbnail()` is empty. JPEG originals are decoded directly; CR2 preview falls back to its largest embedded decodable JPEG while the original RAW remains untouched.
- Device UI wording now clearly separates selected serial-driver rediscovery from global USB/serial native discovery.

## v0.2.10.22 build correction

- Fixed the Windows GUI link failure caused by a declared-but-undefined `MainWindow::refreshFocuserStatus()`.
- Added a GUI declaration/definition link-contract regression check.
- Replaced the ignored `QtConcurrent::run()` future in `OperationManager` with `QThreadPool::start()` to eliminate C4858.


## Native OAL drivers

- `oal.qhy` — QHY cameras through QHYCCD SDK; Windows vendor-header isolation included.
- `oal.canon` — Canon EOS through Canon EDSDK on Windows or USB/PTP/libgphoto2 on Linux.
- `oal.zwo.asi` — ZWO ASI cameras through ZWO ASI SDK.
- `oal.zwo.eaf` — ZWO EAF focusers through ZWO EAF SDK.
- `oal.gemini` — Gemini EAF native serial path.
- `oal.skywatcher` — existing Sky-Watcher SynScan / EqMount native mount path.
- `oal.eqdrive` — dual-protocol EQDrive serial path (EQMOD-compatible Motor Controller first, official ASTEP fallback); raw-axis interface consumed by the OAL Core mount-geometry model.
- `oal.simulated` — reference ABI-v2 simulated devices.

Windows HIL has confirmed native Gemini discovery/motion, native EQDrive discovery/motion, direct SynScan/EQDrive Wi-Fi motion, and Classic ASCOM/EQMOD operation. QHY discovery/capture is partially qualified; explicit Refresh can now hard-reload the inactive QHY driver DLL/SDK after a zero-device scan, while runtime hot-plug/repeated-capture stability remains under HIL.

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

## v0.2.10.21 mount geometry and QHY explicit recovery

- Added `MountGeometryType`/`MountGeometryModel` to OAL Core with GEM, fork-equatorial, Alt-Az, Alt-Az+derotator, equatorial-platform and custom two-axis profiles.
- Native EQDrive and direct SynScan/EQDrive Wi-Fi now consume the shared Core geometry model instead of treating celestial RA/DEC as motor coordinates.
- Mechanical Home/Park is stored separately from celestial coordinates; default Park is Axis1=90°, Axis2=0°. Current axes can be saved as a custom Park from the Mount GUI.
- GEM geometry uses UTC/site longitude to derive local sidereal time and hour angle; pier branch and axis signs are installation configuration. Automatic meridian-flip planning remains experimental/off by default.
- Mount status/API/state now expose geometry type and raw mechanical Axis1/Axis2 when available.
- Classic ASCOM async slews emit periodic RA/DEC/pier/tracking samples for EQMOD geometry diagnostics.
- Explicit all-native Refresh can hard unload/reload an inactive ABI-v2 QHY driver DLL, releasing/reloading its QHYCCD dependency before a second scan. This is never performed while a QHY camera handle is active and never on a periodic timer.
- Documentation: `docs/MOUNT_GEOMETRY.md` and Ukrainian mirror.

## v0.2.10.17 adaptive urban plate solving

- Node-owned `solver.adaptive` operation captures short exposures, evaluates solver-frame quality, registers drift between frames, stacks accepted frames, removes large-scale sky gradients and retries the selected plate solver.
- Camera-side binning is carried in `CameraFrame`; ASTAP FOV derivation is now binning-aware.
- When available, current mount RA/Dec is used as the default solve hint and the search radius expands across attempts.
- REST endpoint: `POST /api/v1/solve/adaptive`; remote GUI exposes **Adaptive urban capture + solve**.
- The final prepared solver frame and per-attempt diagnostics are retained for troubleshooting.


## v0.2.10.17 mount interoperability

- Native ABI-v2 driver `oal.eqdrive` keeps `oal.skywatcher` intact and now probes the EQMOD-compatible Sky-Watcher Motor Controller transport first, with the published EQDrive ASTEP motor-control section as a fallback.
- Native EQDrive/direct Wi-Fi expose raw mechanical axes to OAL Core. `MountGeometryModel` performs J2000/JNow ↔ hour-angle/Alt-Az ↔ mechanical-axis conversion according to the selected GEM/fork/Alt-Az profile; one known-position Sync establishes installation encoder offsets/signs.
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


### Coordinate-frame policy

- OAL canonical equatorial coordinates are **J2000**.
- Mount GOTO/Sync and session targets accept explicit `coordinateFrame: J2000|JNOW`; omitted frame means J2000.
- JNow/of-date support currently applies precession between J2000 and mean equator/equinox of date; nutation, annual aberration, atmospheric refraction and a full topocentric apparent-place model are not yet part of this conversion.
- Classic ASCOM reads `EquatorialSystem` and converts at the compatibility boundary; Stellarium is treated as a J2000 catalogue interface.

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

## v0.2.10.20 manual discovery / J2000 / live native catalogue

- Backend catalogues are carried in node state and remote GUI comboboxes update after explicit discovery. One vendor-neutral scan runs after the server is online; there is no periodic vendor polling. Hardware connected later is enumerated only by **Refresh native device discovery** / `POST /api/v1/drivers/refresh`.
- `synscan-wifi` is direct Motor Controller UDP/11880 to the mount/EQDrive Wi-Fi adapter; `synscan-app` remains UDP/11881 to SynScan Pro.
- `oal.eqdrive` is dual-protocol: EQMOD-compatible Sky-Watcher Motor Controller serial transport is tried first and official EQDrive ASTEP is retained as a fallback; existing `oal.skywatcher` is retained. Direct `synscan-wifi` uses the same Motor Controller command set over UDP 11880.
- Native EQDrive GOTO requires Sync and is HIL safety-limited; park/pier/pulse-guide remain unqualified.

### v0.2.10.29 hot-plug capture fix
Canon hard-recovery must initialize EDSDK on the long-lived application Qt event-loop thread. Do not move the Canon `restartDriver()` call back into the transient native-discovery worker: doing so allows enumeration and commands but loses EOS object-transfer callbacks after hot-plug.

## v0.2.10.32 hardware-control finish

- Canon EDSDK hot-remove: per-camera `kEdsStateEvent_Shutdown`, immediate `DEVICE_DISCONNECTED`, pending exposure wake/cancel, controller-side removal of the active native binding while preserving saved auto-connect for the next hot-add.
- Native long GOTO: removed the temporary 15-degree HIL envelope. Raw/native GOTO uses a 180-degree shortest-axis envelope; automatic meridian flip remains disabled and long slews remain supervised HIL work.
- Manual mount control: OAL/REST/remote-controller two-axis manual-slew contract with hand-controller rate levels 1..9; GUI press-and-hold 3x3 pad; native EQDrive, native SynScan, direct SynScan/EQDrive Wi-Fi and Classic ASCOM MoveAxis implementations.
- Stellarium: immediate position packet on client connection plus 500 ms live J2000 position stream. Raw EQDrive requires one Sync before sky coordinates are valid.
