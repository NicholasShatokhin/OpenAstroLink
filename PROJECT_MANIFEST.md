## v0.2.10.51

- Package: `0.2.10.51-sky-map-mvp`
- Core version: `0.2.10.51`
- Physical/real-host build milestones are now confirmed for Windows x64/MSVC+Ninja, native Linux x86_64 and Linux/WSL→AArch64 Raspberry Pi node/probe + native vendor-driver matrix.
- Native OAL drivers are the default; INDI remains opt-in compatibility only.
- Raspberry Pi 4 and Pi 5 share the generic Linux ARM64/aarch64 ABI target; legacy `rpi4-*` preset names remain compatible aliases in documentation/build workflows. Pi 5 physical runtime qualification remains pending.
- OpenAstroSuite Sky Map MVP: offline horizon map, search, pan/zoom, telescope/solve/FOV markers and mount/Scheduler actions through `ObservatoryController`.
- `oal.eqdrive` v0.2.10.50 removes the temporary hidden `maxNativeGotoDeg` qualification envelope after mount coordinate model v9 HIL qualification. No mount-geometry equations, signs, Home/Park conventions or transport direction logic were changed.
- Core/profile `maxGotoSkyDeltaDeg` (`maxGotoAxisDeltaDeg` legacy alias) remains the operator-controlled supervised sky-safety policy. Raw-axis requests retain the explicit `maxAxisDeltaDeg` mechanical guard.
- Immediate Beta sequence: HIL autofocus, auto-exposure, scheduler, mosaic, Polar Alignment. Smart Telescope UX is deferred to OAL 1.0.

### Linux dependency bootstrap recovery — build-fix22

- Fixed aqtinstall Linux x86_64 architecture identifier: `linux_gcc_64` (not install-directory name `gcc_64`).
- Added live `aqt list-qt` validation for the requested Qt architecture and required HttpServer/WebSockets/SerialPort/Positioning modules.
- Linux apt bootstrap now tries cached package indexes first and only runs `apt-get update` as a fallback; unrelated broken PPAs are never modified automatically.
- Added lock timeout and regression coverage.
- No runtime, mount v9 geometry, or INDI-default changes.

### Windows build-system recovery — build-fix21
- Native Windows presets use the proven **MSVC + Ninja** path with `CMAKE_CXX_COMPILER=cl.exe`; they no longer rely on CMake Visual Studio instance discovery.
- This also prevents Strawberry/MinGW `c++.exe` from poisoning Qt MSVC2022 and vendor `.lib` builds.
- Native Windows build directories use a fresh `*-msvc-ninja` suffix so both stale GNU/Ninja caches and failed VS-generator caches are ignored.
- `scripts/build_windows.ps1` loads `vcvars64` automatically and locates Ninja from `PATH` or the Visual Studio CMake-tools installation. Raw presets should be run from an x64 MSVC Developer Command Prompt.
- Raspberry Pi cross presets hosted on Windows remain GNU/Ninja and are unchanged.

# Project manifest — OpenAstroSuite / OpenAstroLink v0.2.10.51

## v0.2.10.49

- Package: `0.2.10.49-calendar-mosaic-polar-safety`
- Core version: `0.2.10.49`
- Observation scheduling is now per block: each event may have its own UTC date/time, allowing a persistent node calendar to cover months or a year.
- The node persists the calendar, armed state and next-block cursor. Completed blocks are not replayed after restart; a block interrupted by process restart starts again from that block boundary. Mid-frame/SER checkpoint resume remains OAL 1.0 work.
- Blocks support `parkAfter` and `autoUnparkBefore` for optional Park between sparse calendar events.
- Added executable `mosaic-fits` blocks with optical-profile FOV tile geometry, overlap, rotation, serpentine traversal and normal DSO solve/recenter/autofocus/capture policies per tile.
- Polar Alignment RA-offset motion can be constrained to a persisted horizontal Az/Alt safe region; the complete slew path is sampled and rejected if it leaves the allowed region.
- Mount v9 remains HIL-qualified across native EQDrive, direct SynScan/EQDrive Wi-Fi and Classic ASCOM/EQMOD.

### Build infrastructure follow-up — build-fix9

- Added explicit automatic Raspberry Pi cross-sysroot bootstrap for Debian/Ubuntu/WSL hosts: reproducible Debian 12/Bookworm target root or exact `--from-pi` mirror.
- Added matching host-Qt bootstrap for cross-build `moc`/`rcc` tools and `OAS_QT_HOST_PATH` support.
- Added architecture-verified QHY ARM SDK staging from a sibling `QHYCCD_Linux_New` checkout; ARM vendor files are never installed into the x86_64 host.
- Hardened WSL foreign-root setup: explicitly mount/register `binfmt_misc`, verify ARM execution before debootstrap stage two, resume interrupted roots, install `debian-archive-keyring`, and include Qt Positioning in target dependencies.
- build-fix6 handles Ubuntu 22.04's stale Debian keyring without disabling signature verification: the bootstrap fetches the official Debian 12 archive/release/security keys over HTTPS, verifies pinned fingerprints, builds an OAL-local keyring, and passes it to `debootstrap --keyring`.
- build-fix7 fixes a `set -u` Bash ordering bug in the Bookworm-key fetch helper (`name: unbound variable`) by separating local declarations from assignments; the bootstrap can now reach the verified-key download/import stage on Jammy/WSL.
- build-fix8 explicitly seeds target Qt/OpenCV CMake package directories from the Debian multiarch sysroot (`aarch64-linux-gnu` / `arm-linux-gnueabihf`) and persists them in the bootstrap env; host Qt remains host-tools-only for `moc/rcc/uic`.
- build-fix9 makes requested QHY ARM staging a validated success requirement, verifies the staged header/library architecture, and forwards the staged SDK from the bootstrap env record into the cross-build helper.
- Mount coordinate model v9 and all HIL-qualified geometry remain unchanged.

### Build infrastructure follow-up — build-fix18

- Native Linux/macOS/Windows build wrappers now search and bootstrap missing redistributable dependencies before CMake: Qt, OpenCV, QHY and ZWO where deterministic sources are available.
- Managed per-user Qt/vendor cache roots are auto-discovered by CMake on subsequent native configure runs.
- Ubuntu 22.04 can bootstrap a custom Qt >= 6.4 instead of falling back to its incompatible Qt 6.2.4.
- Canon EDSDK remains local-search-only; INDI remains optional/OFF by default; mount v9 is unchanged.

## v0.2.10.48

- Package: `0.2.10.48-safe-af-exposure-scheduler-lifecycle`
- Core version: `0.2.10.48`
- HIL milestone: native EQDrive, direct SynScan/EQDrive Wi-Fi and Classic ASCOM/EQMOD now point consistently on the real mount with direct-MC coordinate model v9 (`Axis1=+1`, `Axis2=-1`).
- Scene autofocus now meters and freezes a dedicated AF exposure, probes both directions from the starting focus, brackets/refines a local contrast peak, and transactionally restores the starting position on flat/failed/cancelled searches.
- Remote 16-bit preview no longer applies a second per-frame min/max stretch; display auto-stretch also no longer converts clipped/flat white frames into black previews.
- Still histogram auto-exposure has an explicit convergence/LOCKED state, retains the best exposure and only reacquires after a persistent scene change; Live/AF frames do not mutate the still controller.
- Scheduler autonomous acquisition preflights and finalizes interactive Live View before waiting for the camera lock; GUI supports block edit/delete/clear/reorder, current J2000 mount-pointing fill, Start now and future in-memory UTC start.
- Future scheduling is still non-durable across node restart; durable scheduler recovery, weather/roof safety, meridian recovery and unattended hardening remain OAL 1.0 work.

## v0.2.10.47

- Package: `0.2.10.47-planetary-ser-executor-buildfix3`
- Core version: `0.2.10.47`
- Extends `ObservationPlan` with a real mixed-mode planetary executor: GOTO, full-frame planet acquisition, planetary autofocus, fixed-size hardware ROI and finite SER runs.
- Adds compact bright-object `PlanetDetector` acquisition/tracking foundation.
- QHY and ZWO ASI native live transports accept hardware ROI; SER remains raw/pre-debayer.
- Adds `<SER basename>.roi.jsonl` provenance so moved ROI sensor regions are reproducible for calibration frames.
- Fast loop shifts hardware ROI when the planet drifts. Optional slow loop self-calibrates a 2×2 RA/DEC→pixel response and performs bounded coordinate micro-slew recentering; disabled by default pending HIL.
- DSO and planetary blocks may be mixed in one plan. Scheduler remains non-durable across restart; safety/weather/meridian and thermal in-exposure focus are OAL 1.0 work.
- Build-fix 3 upgrades direct EQDrive/SynScan-WiFi Core geometry profiles to coordinate model v9. A full recomputation of the follow-up 2026-09-02 HIL across all four physical sign mappings identifies the exact east/west mirror as an Axis2/DEC-polar-distance polarity issue: `Axis1Sign=+1`, `Axis2Sign=-1`. v7 EQMOD branch geometry and valid v7/v8 Home/Park values are preserved.

## v0.2.10.46

- Package: `0.2.10.46-observation-plan-dso-executor`
- Core version: `0.2.10.46`
- Introduces the primary `ObservationPlan` / `ObservationBlock` scheduler model; legacy `SessionTarget` remains a compatibility wrapper.
- Adds the first real node-side supervised DSO executor using ordinary async OAL operations/resource locks: `slew -> adaptive solve/recenter -> autofocus -> FITS/RAW capture`.
- Solve/recenter measures plate-solved pointing error, performs `Sync + correction slew`, retries to a configurable arcminute tolerance, and can run before the first frame or every N science frames.
- Autofocus can run before the first frame or every N science frames. DSO science captures request durable camera output (`saveRaw=true`).
- Session status now exposes block cursor, current step/operation, per-block/global frame counts and terminal failure reason. `/api/v1/sessions/current/plan` returns the loaded plan.
- `planetary-ser` is a first-class plan mode and wire model, but its autonomous executor is deliberately not enabled yet; ROI/centroid/SER-run orchestration remains the next scheduler stage.
- Scheduler remains non-durable across node restart; weather/meridian/recovery/unattended safety remain OAL 1.0 roadmap items.

## v0.2.10.45

- Package: `0.2.10.45-eqmod-gem-histogram-ser-metadata`
- Core version: `0.2.10.45`
- Direct EQDrive serial and direct SynScan Wi-Fi use coordinate model v7: EQMOD-style mechanical HA/Dec pointing states with northern Home `0°,0° = HA -6h / Dec +90° / pier west`.
- v7 removes v6 shortest-equivalent-GEM-branch selection, which HIL showed could select the wrong physical pointing state even though serial and UDP transports were mutually identical.
- Histogram exposure assistant uses fixed sensor scale, proportional/damped correction, bright-tail saturation protection and one auto-apply per actual science frame.
- SER recordings write same-basename human-readable capture metadata sidecars; Live View carries gain/offset/bin/exposure metadata into the writer.
- Reserved OAL/API/UI placeholders for filter wheel, rotator, dome/roof, weather, GPS, power, cover/calibrator and safety monitor.

## v0.2.10.44

- Package: `0.2.10.44-synscan-wifi-native-parity`
- Core version: `0.2.10.44`
- Removed the Wi-Fi-only Motor Controller axis-polarity layer introduced in v0.2.10.43.
- Native serial EQDrive and direct UDP/11880 share the same GOTO-plan function and mechanical count convention.
- Added exact Wi-Fi per-axis GOTO diagnostics for HIL comparison.

## v0.2.10.43

- Package: `0.2.10.43-synscan-wifi-polarity`
- Core version: `0.2.10.43`
- `synscan-wifi` has transport-specific polarity for the HIL EQDrive Wi-Fi profile: Axis1 `+1`, Axis2 `-1`; generic Sky-Watcher UDP/11880 remains `+1/+1`.
- Native serial EQDrive geometry v6 is unchanged.
- UDP shutdown no longer polls datagrams after socket close.


## v0.2.10.42

- Package: `0.2.10.42-direct-mc-polar-frame`
- Core version: `0.2.10.42`
- Native serial EQDrive and direct UDP/11880 capture controller startup counts as the session Home/Park reference; the physical start pose is `Axis1=0°, Axis2=0°` with no guessed 90° DEC offset.
- Direct-MC coordinate model ABI v6 uses the polar telescope-direction-vector transform and selects the shortest equivalent GEM branch.
- Direct-MC auto-Home restores a valid sky model on connect, so normal startup does not require a manual Polaris Sync.
- Stellarium position streaming uses live axis counts during slew.
- High-level SynScan hand-controller/App backends remain RA/DEC-native; only direct `synscan-wifi` shares the direct-MC model.
- Classic ASCOM default remains `EQMOD.Telescope`, with EQMOD site authoritative when available.

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
- `oal.canon` — Canon EOS through Canon EDSDK on Windows/macOS or selectable EDSDK/USB-PTP-libgphoto2 on Linux.
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
- Cross-platform presets/scripts for Windows/Linux/RPi/macOS.

## v0.2.10.21 mount geometry and QHY explicit recovery

- Added `MountGeometryType`/`MountGeometryModel` to OAL Core with GEM, fork-equatorial, Alt-Az, Alt-Az+derotator, equatorial-platform and custom two-axis profiles.
- Native EQDrive and direct SynScan/EQDrive Wi-Fi now consume the shared Core geometry model instead of treating celestial RA/DEC as motor coordinates.
- Mechanical Home/Park is stored separately from celestial coordinates; default direct-MC Home/Park is Axis1=0°, Axis2=0°. Current axes can be saved as a custom Park from the Mount GUI.
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

### Build infrastructure follow-up — build-fix10

- Corrected the QHY ARM ABI assumption after physical inspection showed the legacy `QHYCCD_Linux_New` `armv8` SDK is 32-bit ARM/EABI, not AArch64.
- QHY cross staging now selects by actual ELF architecture, supports direct SDK archive paths, permits ARM64 bootstrap without QHY by default, and offers `--require-qhy` for a strict full-vendor gate.
- The legacy QHY `armv8` archive is now intentionally supported by the ARMHF staging path.


### Build infrastructure follow-up — build-fix12

- Physical AArch64 compilation reached real OAL sources after the cross sysroot/bootstrap fixes.
- Canon EDSDK Linux header compatibility added locally for GCC (`__int64`, `WCHAR`) without patching vendor files.
- Qt HttpServer bind compatibility added for Qt 6.4-6.7 (`void`) and Qt 6.8+ (`bool`).
- Added `tests/linux_arm_portability_smoke.py`.
- Mount v9 geometry is unchanged.


### Build infrastructure follow-up — build-fix13

- Physical WSL -> AArch64 compilation reaches 100% source compilation; only final executable link remained.
- The linker failure is isolated to foreign-sysroot transitive dependency discovery: OpenCV/Armadillo/ARPACK/SuperLU require BLAS/LAPACK and the Bookworm libc, but GNU ld was not searching the target multiarch/runtime directories for `DT_NEEDED` closure.
- Cross builds now add link-time-only target-sysroot `-rpath-link` and `-L` directories for `/lib/<multiarch>`, `/usr/lib/<multiarch>`, `/lib`, and `/usr/lib`. No install/runtime RPATH is added.
- Raspberry Pi bootstrap explicitly installs and validates `libblas.so.3` and `liblapack.so.3`.
- Added `tests/cross_linker_sysroot_smoke.py`.
- Mount v9 geometry/runtime behavior is unchanged.

### Build infrastructure follow-up — build-fix14
- Added official QHYCCD 26.x Linux ARM64 fetch/cache/staging (`scripts/fetch_qhy_sdk.sh`).
- New packaging (`sdk_linux_arm64_<version>.tar.gz`) is supported from 26.06.04 onward.
- QHY staging validates real ELF architecture and supports shared or static `libqhyccd.a` payloads.
- `bootstrap_rpi_cross.sh --qhy-version 26.06.04 --require-qhy` enables the full ARM64 QHY path.
- Mount v9 geometry/runtime is unchanged.

### Build infrastructure follow-up — build-fix15

- Physical WSL -> AArch64 trial confirms official QHYCCD SDK 26.06.04 downloads/stages as a real ARM64 shared library and `oal_driver_qhy.so` builds successfully.
- Remaining failure is isolated to Debian Bookworm BLAS/LAPACK alternatives resolution at the final executable link.
- Sysroot symlink normalization now runs as root so absolute alternatives links are actually rewritten instead of emitting permission errors.
- Cross link search includes `usr/lib/<multiarch>/blas` and `usr/lib/<multiarch>/lapack`.
- Target `liblapack.so.3` and `libblas.so.3` are validated by ELF architecture and linked explicitly after OpenCV to close Armadillo/ARPACK/SuperLU/OpenCV numerical dependencies.
- No runtime sysroot RPATH is embedded; mount v9 remains frozen and unchanged.

### build-fix16 delta
- Added reproducible host-native vendor SDK bootstrap: official QHYCCD on Linux/macOS/Windows x64, plus pinned INDI-mirror ZWO ASI/EAF on Linux/macOS.
- Added native build-script switches to consume staged vendor SDKs without editing machine-global paths.
- Added `rpi4-cross-arm64-observatory-release` and Windows-host equivalent with `OAS_BUILD_GUI=ON`; node-only cross presets remain headless by design.
- Mount v9 geometry/control code unchanged.


### build-fix17 delta
- Physical WSL -> AArch64 full-native node/probe build is now confirmed at 100% with QHYCCD 26.06.04, Canon EDSDK and ZWO ASI/EAF; build-fix15 BLAS/LAPACK closure is validated.
- INDI compatibility is opt-in globally: `OAS_ENABLE_INDI=OFF` by default and normal observatory/node presets stay native-first.
- Explicit `*-indi-release` presets preserve compatibility for INDI-only equipment without changing native driver priority.
- User presets default to native OAL and expose separate opt-in INDI variants on Windows/Linux/macOS.
- Mount v9 geometry/control code unchanged.

### build-fix21 delta
- Native Linux Jammy bootstrap UX fixed: `build_linux.sh --bootstrap-deps` is the canonical path and installs per-user Qt 6.8.3 with aqtinstall when distro Qt is only 6.2.4.
- Linux build wrapper detects and removes a stale `CMakeCache.txt` copied between WSL `/mnt/c/...` and a native Linux checkout `~/...`; `--clean` is also available explicitly.
- A missing/stale `OAS_QT_ROOT` no longer suppresses discovery of another valid managed/user Qt installation.
- Linux example/user presets no longer hard-code a not-yet-installed Qt prefix and use a relocatable sibling `CANON_EDSDK_ROOT=${sourceDir}/../edsdk` instead of shell `~` paths.
- CMake's Jammy Qt failure now points directly to the automatic bootstrap command and explains that adding more Jammy Qt 6.2.4 development packages cannot satisfy Qt HttpServer/Qt >= 6.4.
- Mount v9 geometry/control code unchanged.
