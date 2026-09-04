## v0.2.10.50 — cross-platform build qualification and HIL-qualified mount release

- **Build qualification:** Windows x64/MSVC+Ninja, native Linux x86_64, and Linux/WSL → Raspberry Pi ARM64 cross-builds have all reached successful full builds on real development hosts. The ARM64 build includes the native QHY 26.06.04, Canon EDSDK, ZWO ASI/EAF, Gemini, Sky-Watcher and EQDrive paths. macOS Apple Silicon/Intel presets and dependency bootstrap are implemented; a physical Mac build remains pending.
- **Raspberry Pi 4/5:** the supported 64-bit target is generic Linux `aarch64`. Existing `rpi4-*` preset names are retained for compatibility but their ARM64 binaries are suitable for Pi 4 and Pi 5 when the target userspace/ABI is compatible. Pi 5 physical runtime/HIL is still pending.
- **Native-first policy:** native OAL drivers are the default on every platform. INDI remains an explicit opt-in compatibility layer (`*-indi-release` or `OAS_ENABLE_INDI=ON`).
- **Vendor/bootstrap policy:** Qt/OpenCV/QHY/ZWO are searched first and can be bootstrapped automatically where redistribution/download is deterministic. Canon EDSDK is discovered locally but is not downloaded by OAL.
- **Mount v9 is frozen:** the HIL-qualified direct-MC mapping (`Axis1=+1`, `Axis2=-1`) and GEM geometry are unchanged. The temporary hidden EQDrive `maxNativeGotoDeg`/15° qualification gate has been removed from the driver. The user-controlled Core/profile sky-separation safety policy remains available, and raw-axis calls retain an explicit mechanical ceiling.
- **Beta priority is unchanged:** HIL autofocus → auto-exposure → scheduler → mosaic → Polar Alignment. Smart Telescope UX belongs to OAL 1.0, not the nearest Beta.

## v0.2.10.49 build-fix22: Linux Qt bootstrap correction

The native Linux dependency bootstrap now uses the Qt repository architecture ID `linux_gcc_64` for x86_64 aqtinstall downloads (the installed directory is still `gcc_64`). It validates the advertised architecture and required modules before download, preventing the misleading `qt_base package not found` failure seen with `gcc_64`. Linux system-package bootstrap also attempts installation from existing apt indexes before running `apt-get update`, so an unrelated broken third-party PPA does not unnecessarily block OpenAstroLink setup. INDI remains opt-in and mount v9 geometry is unchanged.

## v0.2.10.49 build-fix20: deterministic Windows MSVC presets

Native Windows presets use Ninja with an explicit `CMAKE_CXX_COMPILER=cl.exe` and separate `*-msvc-ninja` build directories. This avoids both Strawberry/MinGW compiler capture and CMake Visual-Studio-instance discovery failures. Run raw presets from an x64 MSVC Developer Command Prompt, or use `scripts/build_windows.ps1` to load `vcvars64` automatically. RPi cross presets on Windows remain GNU/Ninja.

# OpenAstroSuite / OpenAstroLink

### Automatic native dependency bootstrap

Recommended platform build wrappers now search existing dependencies first and bootstrap missing redistributable ones automatically:

```bash
./scripts/build_linux.sh my-linux-observatory
./scripts/build_macos.sh my-macos-arm64-observatory
```

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

Qt/OpenCV/QHY/ZWO are resolved where deterministic distribution is available. Canon EDSDK is auto-discovered locally but must still be obtained from Canon separately. Add `--no-auto-deps` on Unix/macOS or `-NoAutoDeps` on Windows for manual dependency management.

## v0.2.10.49 — persistent observing calendar, mosaics and optional polar-motion safety

- Every `ObservationBlock` can now carry its own `startAtUtc`, so a single node calendar can schedule independent astronomical events months or a year ahead instead of applying one start time to an entire plan.
- The calendar, armed state and next-block cursor are persisted on the node. A restart between blocks resumes from the first unfinished block; a restart inside a block restarts that block from its beginning. Frame/SER mid-block checkpoint resume remains an OAL 1.0 item.
- Each block has `parkAfter` and `autoUnparkBefore`, allowing the telescope to return to Park between distant events and wake for the next scheduled block.
- Added `mosaic-fits`: the scheduler derives tile centers from the optical profile/main-sensor field of view, grid rows/columns, overlap and rotation, traverses tiles serpentine, and reuses normal DSO slew/solve/recenter/autofocus/FITS policies per tile.
- Polar Alignment manual/guided RA-offset motion can now be constrained to a configured Az/Alt safe region. OAL samples the complete requested slew path and rejects motion if any intermediate point leaves the allowed sky region.

## v0.2.10.48 — safe scene AF, convergent exposure and scheduler lifecycle

- Mount qualification milestone: native EQDrive, direct SynScan/EQDrive Wi-Fi and Classic ASCOM/EQMOD now point identically on the 2026-09-02 HIL with direct-MC coordinate model v9 (`Axis1=+1`, `Axis2=-1`).
- Scene autofocus now meters its own exposure, uses a local contrast-detect search that probes both directions and brackets a peak, and **restores the starting focuser position on unreliable/failed focus** instead of leaving the telescope at the end of a blind sweep.
- Remote 16-bit previews preserve the same fixed sensor scale as the node, fixing saturated AF frames that could render black and stopping remote histogram measurements from losing absolute exposure information. Histogram Auto now has an explicit convergence/lock state and only unlocks after a persistent illumination change.
- Scheduler start has a camera-resource preflight: an active interactive Live View is stopped/finalized before autonomous capture. The GUI can update, delete, clear and reorder blocks, copy the current telescope pointing into a target, and start immediately or at a selected date/time. Future scheduling is in-memory; durable restart/resume remains OAL 1.0 work.


## v0.2.10.47 build-fix 3 — direct-MC DEC/polar-distance mirror correction

- Follow-up HIL on 2026-09-02 showed an exact east/west sky mirror even after the Axis1 hypothesis in build-fix 2. Recomputing the recorded target from raw encoder deltas under all four axis-sign mappings proves that Axis1 inversion cannot create that mirror; the DEC/polar-distance mapping can.
- Coordinate model v9 keeps the v7 EQMOD HA/Dec branch equations and uses the HIL-qualified direct-MC physical mapping `Axis1=+1`, `Axis2=-1`. For the recorded target, the required raw move is approximately `Axis1=-11.116°`, `Axis2=+67.847°`; the superseded v8 command `+11.116°,-67.847°` corresponds to the observed mirrored east-side pointing.
- Serial EQDrive and UDP/11880 SynScan Wi-Fi remain transport peers. Existing v7/v8 profiles migrate automatically without discarding valid Home/Park values; profiles older than v7 still receive the legacy one-time Home/Park reset.

## v0.2.10.47 — mixed DSO + autonomous planetary SER executor

- `ObservationPlan` now executes both `dso-fits` and `planetary-ser` blocks in one node-side plan.
- Planetary execution is **GOTO → full-frame acquisition/detection → optional planetary autofocus → reacquisition → hardware ROI → finite SER**.
- A fast tracker may move the fixed-size hardware ROI without moving the mount. Every ROI origin change is written beside the SER as `<basename>.roi.jsonl`, while the human-readable `.txt` capture sidecar remains present.
- An optional slow mount loop can self-calibrate the camera orientation with bounded RA/DEC micro-slews, invert the measured 2×2 pixel response, and recenter large drift. It is disabled by default and requires HIL qualification for each mount/backend.
- QHY and ZWO ASI native live paths accept hardware ROI. ZWO ASI remains implemented but real-hardware HIL pending.
- Scheduler restart durability, weather/roof safety, meridian-flip recovery and in-exposure thermal focus compensation remain OAL 1.0 roadmap work.

**Current package: v0.2.10.50 — cross-platform build-qualified native-first Beta foundation with HIL-qualified direct-MC v9 mapping and no hidden EQDrive qualification slew cap**

## v0.2.10.46 — ObservationPlan and supervised DSO executor

- Replaced the scheduler's primary flat `SessionTarget` model with `ObservationPlan` / `ObservationBlock`, while retaining legacy session targets as an API compatibility wrapper.
- Added the first real node-side DSO workflow executor: **slew → adaptive plate solve/recenter → autofocus → FITS/RAW science capture × N**. All long actions reuse `OperationManager` resource locks.
- Recenter can run before the first frame and every N science frames, with configurable arcminute tolerance and retry limit. It uses the solved field center, `Sync`, a correction slew, and re-solves until converged. Autofocus has matching before-first/every-N policy.
- Session telemetry now exposes the active block, step, operation ID, per-block/global progress and failure reason. The GUI builds DSO blocks directly.
- `planetary-ser` already exists in the plan/API model but autonomous ROI/centroid/SER-run execution is intentionally deferred to the next scheduler stage. Restart durability, meridian/weather recovery and unattended safety remain OAL 1.0 work.

**Previous package: v0.2.10.46 — supervised DSO scheduler executor**

## v0.2.10.45 — EQMOD GEM geometry, histogram control, SER metadata and observatory stubs

- Replaced the direct-Motor-Controller v6 free-shortest-branch telescope-frame model with **v7 EQMOD-style GEM mechanical HA/Dec pointing states**. Northern mechanical Home remains `Axis1=0°, Axis2=0°`, but is explicitly `HA=-6h, Dec=+90°`, `pier=west`; target branch is selected from hour angle instead of whichever mathematically equivalent branch is shortest. Native EQDrive serial and direct SynScan Wi-Fi still share the exact same Core geometry and low-level GOTO plan.
- Fixed histogram auto-exposure measurement for 16-bit cameras: preview conversion now preserves a fixed 0..65535 sensor scale instead of min/max normalizing every frame. Exposure correction is proportional/damped, includes a P99 highlight ceiling and saturation retreat, and no longer accumulates repeated auto-corrections from unchanged Live View frames.
- Every SER recording now finalizes a same-basename FireCapture-style `.txt` sidecar with requested/actual exposure, gain, offset, binning, FPS, raw format/CFA, optical profile, site and UTC recording metadata. Live View gained an explicit offset control.
- Reserved first-class OAL placeholders for filter wheel, rotator, dome/roof, weather, GPS/GNSS, power/switch, cover/calibrator and safety monitor. They are intentionally non-connectable stubs until real backends are implemented.

**Previous package: v0.2.10.45 — EQMOD GEM geometry / capture metadata HIL follow-up**

## v0.2.10.44 — SynScan Wi-Fi/native Motor Controller parity fix

- Removed the v0.2.10.43 Wi-Fi-only axis-polarity layer. UDP/11880 and native EQDrive serial address the same Motor Controller axes, so identical controller counts now have identical mechanical meaning on both transports.
- Serial and Wi-Fi GOTO now use the same shared `skywatcher_mc::makeGotoPlan()` for mechanical delta → direction, step count and brake count. Only the I/O transport differs.
- Added Wi-Fi HIL diagnostics for the last commanded per-axis delta, raw count increment and forward/reverse bit, plus firmware payloads.
- Retained the UDP socket shutdown guard from v0.2.10.43.

**Previous package: v0.2.10.44 — SynScan Wi-Fi/native Motor Controller parity fix**

## v0.2.10.43 — SynScan Wi-Fi DEC transport polarity hotfix

- Native `oal.eqdrive` v6 geometry is unchanged; the serial HIL path already points correctly.
- Direct `synscan-wifi` now separates **transport count polarity** from the shared sky-geometry axis signs. The current EQDrive Wi-Fi HIL profile (`9216000` counts/rev on both axes, timer `53694`) uses `Axis1=+1`, `Axis2=-1`, matching the observed physical DEC direction without reintroducing a fake 90° sky offset.
- Generic Sky-Watcher UDP/11880 remains `+1/+1` because the official protocol defines Wi-Fi as the same Motor Controller command set as serial. Operators can override either transport sign with `OAL_SYNSCAN_WIFI_AXIS1_SIGN` / `OAL_SYNSCAN_WIFI_AXIS2_SIGN`.
- GOTO, manual slew, tracking direction and live axis decoding all use the same transport polarity, so status/Stellarium and physical motion remain mutually consistent. Park still targets the captured startup Home/Park counts.
- Hardened UDP disconnect handling to avoid calling `QUdpSocket::hasPendingDatagrams()` after the socket has left `BoundState`.

**Previous package: v0.2.10.43 — SynScan Wi-Fi DEC transport polarity hotfix**

## v0.2.10.42 — direct-MC polar telescope-frame geometry

- Native `oal.eqdrive` serial and direct `synscan-wifi` UDP/11880 keep the **actual Motor Controller position counts at connect time** as the physical session Home/Park reference. That startup pose is exposed as `Axis1=0°, Axis2=0°`; no fixed 90° DEC correction is invented.
- Fixed the remaining native east/west pointing error. Coordinate-model ABI **v6** now follows the proven SkyWatcher/INDI telescope-direction-vector transform: JNow sky -> horizontal direction vector -> polar-aligned mount frame. `Axis1` is the mount-frame azimuth of the RA axis and `Axis2` is signed polar distance. This replaces the incorrect v5 `H+90°` phase assumption.
- Both equivalent GEM branches are still evaluated and the branch requiring the shortest physical controller motion is selected. The 2026-08-31 HIL regression that previously sent an eastern Deneb-region target north now maps to the nearby flipped branch instead.
- The exact inverse transform is applied to every live encoder sample, so Stellarium receives changing RA/DEC throughout a native slew.
- Direct-MC startup Home automatically restores a valid sky model; manual Polaris Sync is not required for normal startup. Plate-solve Sync remains available only as a later pointing refinement.
- High-level SynScan backends stay separate: `oal.skywatcher` hand-controller and `synscan-app` use SynScan's own RA/DEC alignment model. Only direct Motor Controller Wi-Fi (`synscan-wifi`, UDP/11880) shares v6 with native EQDrive serial.
- Classic ASCOM still defaults to `EQMOD.Telescope`, and a valid EQMOD site remains authoritative by default.


## v0.2.10.38 — mount site normalization, sky-angle safety and shared persistent Park

- Fixed native near-pole GOTO safety semantics: the user limit is now the **true angular separation on the sky**, not raw RA/DEC motor-axis rotation. Near the celestial pole a 3–5° sky move may legitimately require >100° of RA-axis rotation; the native transport keeps a separate 180° mechanical hard cap. `maxGotoSkyDeltaDeg` is the preferred API name while `maxGotoAxisDeltaDeg` remains a compatibility alias.
- Classic ASCOM now automatically checks its own `SiteLatitude/SiteLongitude` against the OAL observatory profile on connect and before every GOTO. If the driver allows site writes, OAL pushes the canonical site/time automatically. If EQMOD rejects writes, GOTO is blocked with an explicit instruction to enable **ASCOM Options → Allow Site Writes** or configure the same site in EQMOD Properties.
- Classic ASCOM diagnostics now include driver-reported Azimuth, Altitude and SiderealTime in addition to RA/DEC, pier side, tracking, site and UTC. This makes East/West geometry disagreements directly visible in the node log.
- ASCOM `EquatorialSystem` is decoded according to the standard (`Topocentric=1`, `J2000=2`). Legacy EQMOD commonly advertises `equOther=0`; for `EQMOD.*` OAL uses an explicit compatibility assumption of topocentric/JNow and logs that assumption. Selecting an explicit JNOW or J2000 epoch in EQMOD Setup is still recommended.
- Added a backend-neutral **Calibrate current physical pose as persistent Home / Park** action. Native raw-axis mounts persist the same pose as OAL Home+Park and enable auto-Home restore; Classic ASCOM uses the standard `CanSetPark/SetPark` mechanism. Calibrate each backend once at the same physical pose (without moving between backend switches) and the park pose remains persistent thereafter.
- Native Axis1/Axis2 inversion controls are explicitly native-only; they are blocked for Classic ASCOM because EQMOD/ASCOM owns its own motor-to-sky transform.
- Existing v0.37 behavior remains: repeatable native Home can restore the sky model on connect, operational profile changes preserve Sync, GOTO can auto-unpark, and the Mount tab is vertically scrollable.

**Current package: v0.2.10.38 — mount site normalization, sky-angle GOTO safety, shared persistent Home/Park**

## v0.2.10.36.1 package hotfix — Qt 6.10 QTimer signal call

**Package hotfix:** fixes the Windows Qt 6.10/MSVC build failure caused by explicitly calling `QTimer::timeout()`. Core/protocol version remains `0.2.10.36`; no runtime behavior or protocol schema changed.


## v0.2.10.36 Mount diagnostics, SER and tracking-rate HIL

- Live/Finder can optionally record the raw pre-debayer stream to SER, preserving Bayer/mono science pixels while the GUI remains independently stretched/debayered.
- Added optional Mil-Dot and angular measurement grids to Live View. The angular grid uses the optical profile pixel scale; Mil-Dot spacing is one milliradian.
- Mount tracking now exposes Sidereal, Lunar and Solar rates. Lunar mode is the appropriate first test when keeping the Moon centered; sidereal tracking intentionally follows the stellar sky instead.
- Native EQDrive raw-axis GOTO safety is configurable (0.1–180°) and is exposed on the Mount tab. The conservative default remains 15° until the installation axis mapping is qualified.
- Mount GOTO/Sync/connect/tracking now log UTC, local time, site, LST, J2000, JNow, Az/Alt, spherical sky separation, pier side, raw axes and backend diagnostics. Near-pole Sync emits an explicit warning because RA is poorly conditioned close to the celestial pole.
- Classic ASCOM status is marked coordinate-valid so its live RA/DEC can reach Stellarium. ASCOM diagnostics include its own site/time/alignment/tracking-rate values, and the Mount tab can explicitly push the OAL site and UTC to writable ASCOM drivers.
- Remote 10-fps Live View fetches the node's latest frame rather than stale per-frame IDs, removing preview-cache races seen during long QHY streams.



## v0.2.10.35.1 ZWO ASI MSVC build hotfix

- Fixed a C++ most-vexing-parse in the new ZWO ASI Live View buffer allocation. MSVC interpreted `std::vector<unsigned char> buffer(size_t(bytes));` as a function declaration, which caused the misleading `.data()` and `ASIGetVideoData` argument-count diagnostics.
- The Live View buffer now uses an unambiguous default construction plus `resize(static_cast<std::size_t>(bytes))`.
- Added `tools/zwo_asi_sdk_compile_check.py`, which syntax-compiles the full native ZWO ASI driver against an API-shape stub containing the real four-argument `ASIGetVideoData` contract.

## v0.2.10.35 Focus preview, optional debayer and synchronized target coordinates

- Autofocus publishes one operational preview frame per focus position, so the main camera pane stays visually useful while the focuser scans. The Focus tab also provides manual `-- / - / STOP / + / ++` jog controls with a configurable step.
- Scene autofocus now uses an intensity-preserving contrast metric, rejects flat curves, extends a coarse scan when the best sample lands on an edge, and no longer replaces a strong coarse peak with a weaker fine-pass peak. This addresses the daylight HIL case where a visually good ~7160 position was displaced by a lower-score fine scan.
- Live View software debayer is optional and preview-only. `AUTO` consumes CFA metadata from the active driver; RGGB/BGGR/GRBG/GBRG can be selected explicitly for any one-channel Bayer source. QHY obtains its Bayer sequence from `CAM_COLOR`; ZWO ASI publishes `ASI_CAMERA_INFO::BayerPattern`. Science FITS/RAW pixels remain untouched.
- 16-bit color previews are converted correctly to the 8-bit GUI transport after debayer instead of being interpreted as RGB888 byte data.
- Saturation/underexposure are image-quality warnings only. A clipped frame remains a valid camera frame and never implies transport failure or physical disconnect.
- The Mount tab now exposes synchronized editable J2000, JNow, horizontal Az/Alt and Galactic l/b target fields. Editing any system updates the others using current UTC and the observatory location; all GOTO/Sync requests enter the mount layer canonically as J2000.
- Added a Polaris J2000 preset and clearer Park/Sync guidance for the raw EQDrive workflow. Mechanical Park is not silently treated as a sky Sync: after startup, establish one known pointing anchor (Polaris or a plate solve) before Stellarium/coordinate GOTO.

## v0.2.10.34 QHY Live View stability and false-disconnect fix

- QHY Live View now uses the SDK continuous-stream path (`SetQHYCCDStreamMode(...,1)` / `BeginQHYCCDLive` / `GetQHYCCDLiveFrame` / `StopQHYCCDLive`) instead of emulating video with repeated single-frame exposures. The driver reopens the camera in single-frame mode when Live View stops, so normal Capture/Autofocus remains usable afterwards.
- Native health polling now skips resources owned by active operations. QHY health no longer calls `GetQHYCCDChipInfo` every 1.8 s; it uses a lighter idle-only control probe and requires three consecutive failures before emitting `device.disconnected`, preventing transient SDK errors from looking like USB removal.
- Native camera sensor geometry is cached at connect time, avoiding extra QHY capability/vendor calls after every readout.
- Daylight Live/Finder defaults are now 1 ms, gain 0, 2x2, 5 fps. The GUI explicitly warns when a preview is almost completely white or black, which is especially useful for finder alignment.
- QHY Live View cancellation no longer routes through the single-frame exposure-abort API; the stream loop exits and performs the correct `StopQHYCCDLive` cleanup itself.
- Remote GUI capture now forwards `saveRaw` / `savePath` through HTTP, fixing the missing-QHY-FITS symptom when the GUI runs against a separate node. Live View and autofocus frames remain preview-only and are not science-spooled.

## v0.2.10.33 Live View, Scene Autofocus and Finder Alignment

- Added a node-local `camera.live-view` operation exposed through OAL HTTP/WebSocket. It continuously acquires short preview frames while holding the camera resource and never science-saves those frames. The default GUI profile is 50 ms, 2x2 binning, 5 fps for low-latency QHY/ASI-style acquisition.
- Added the **Live / Finder** GUI tab with auto-stretch, a center crosshair, robust brightest-region highlighting and pixel offset readout.
- Added a five-step **Finder Alignment wizard** for daylight alignment: acquire a distant target -> Scene autofocus -> center the main optical axis -> adjust finder screws -> verify.
- Added **Scene autofocus** for terrestrial/lunar/planetary structure. It uses an edge-energy metric and configurable AF exposure/gain instead of requiring stellar PSFs.
- Star autofocus now has a minimum-star gate and fails explicitly with `No suitable stars detected` instead of selecting a noise peak when the field contains no stars.
- Live preview keeps a small in-memory frame cache so a remote GUI can fetch several outstanding preview frames without racing the next frame.
- Repeated-still Canon DSLR Live View is deliberately blocked to protect the shutter. A dedicated Canon EDSDK EVF transport is the next Canon-specific live-view step.
- Added an initial static web-site source under `site/` for `openastro.link`, with English canonical content and a Ukrainian mirror. No DNS/deployment changes are performed by the application.

## v0.2.10.32 Safety-first hardware HIL fixes

- Active native QHY, Gemini EAF and EQDrive devices receive lightweight background health probes. Physical USB/serial loss emits `device.disconnected`, cancels the owning resource operation, removes the stale connected object from node state, and refreshes only the affected native driver catalogue. Persisted reconnect bindings are retained.
- User-triggered native camera captures request science-file preservation. Cameras that publish only host-frame pixels (notably QHY) are spooled by OAL Core to FITS under `Pictures/OpenAstroLink/<VENDOR>/`; Canon continues to preserve its native CR2/CR3 original. Adaptive/autofocus temporary frames are not automatically written.
- EQDrive `ABORT` now uses Motor Controller instant-stop (`:L`) with normal-stop fallback and verifies both axes actually stop. Explicit disconnect also attempts a stop first.
- Native mechanical Park is disabled until the user explicitly saves the current safe physical axes as Park. Unchecking Park while a park slew is active now calls the physical abort path. The unsafe generic `90°,0°` Park restore button was removed.
- Mount Sync UX now states that Sync asserts the telescope is physically pointing at the entered coordinates, adds `Sync mount to last successful plate solve`, and exposes quick Axis 1/Axis 2 mapping reversal controls for HIL qualification.
- After the first long-slew HIL showed an unqualified installation-axis mapping, the 15° native sky-GOTO qualification envelope is temporarily restored. Mechanical Park has its own explicitly calibrated raw-axis target.

The EOS 550D HIL showed that EDSDK can emit `camera-added` before `EdsGetCameraList()` exposes the body. v0.2.10.29 therefore turns the callback into a bounded, debounced Canon-only settle/retry sequence instead of an immediate one-shot scan. If normal retries still see zero devices, the final fallback hard-reloads only the idle `oal.canon` EDSDK driver and rescans it. There is still no periodic vendor polling. ISO/gain, Bulb capture, full-resolution operational preview, and CR2/CR3 science-file preservation remain unchanged.

## v0.2.10.30 Adaptive solve stability + histogram assistant

- Adaptive plate solving now enforces the requested solver bin in software when a camera (notably Canon DSLR/EDSDK) cannot bin in hardware. A 5184×3456 Canon preview requested as 2×2 is reduced immediately to about 2592×1728 before gradient removal, star registration, and stacking, sharply reducing transient RAM/CPU pressure.
- The adaptive capture phase has a bounded wall-clock budget (`maxCapturePhaseSec`, default 120 s) and a small Canon inter-frame settle interval.
- Adaptive exposure growth is now quality-aware: background level, p99, saturation fraction, and detected-star count influence the next short exposure. ISO/gain is not changed automatically.
- Operation progress no longer rebuilds/broadcasts the complete hardware state on every progress tick; full state snapshots are emitted at start/final transitions, keeping HTTP/WebSocket control responsive during CPU-heavy preprocessing.
- The Capture & Solve tab has an optional log-scale preview histogram. It reports P1/median/P99 and clipping fractions, suggests the next exposure for a configurable background target, and can auto-apply that exposure to the next capture while leaving ISO/gain unchanged. For Canon this is derived from the embedded JPEG preview, so it is an operational guide rather than a linear-RAW photometric histogram.

## v0.2.10.29 Canon hot-plug settle/retry recovery

- Canon `device.discoveryHint` starts debounced driver-scoped retries at approximately 0.7, 2.2, 4.5 and 8 seconds after the most recent hot-plug edge.
- A newer Canon hot-plug edge supersedes the previous retry series; once a Canon device is cached, remaining retries become no-ops.
- The final retry may hard-reload only an idle `oal.canon` driver after a zero-device scan, reproducing the successful explicit-Refresh recovery without probing QHY or serial drivers.
- Focused hard-recovery requests now preserve both their selected driver scope and the hard-recovery flag even when another native discovery pass is already running.
- Retains the v0.2.10.27 Qt 6.10/MSVC `QJsonValue` capture-result build fix and all v0.2.10.26 ISO/science-file behavior.

## v0.2.10.26 Canon automatic rediscovery and ISO gain

- Canon EDSDK `camera-added` emits `device.discoveryHint`; the core starts a non-blocking `oal.canon`-only rediscovery and the node reuses its normal persisted auto-connect restore path.
- OAL `gain` on Windows Canon now means ISO. `0` preserves the camera's current ISO; positive values map to the nearest body-advertised EDSDK ISO and a failed ISO write fails the exposure explicitly.
- Canon capture results carry `scienceFilePath`; the GUI logs `Science/original file: ...` after a successful capture.
- Plate solving still uses the operational `CameraFrame.image` (for the tested 550D RAW path: a 5184×3456 embedded JPEG). The CR2 is preserved separately for calibration/stacking/photometry.

## v0.2.10.22 Windows GUI link fix

- Restores the missing `MainWindow::refreshFocuserStatus()` implementation that caused MSVC `LNK2019/LNK1120` while linking `OpenAstroSuite.exe`.
- Adds `tools/gui_link_contract_check.py`, which verifies that ordinary `MainWindow` methods declared in the header have matching out-of-class definitions.
- Replaces the discarded `QtConcurrent::run()` future in `OperationManager` with `QThreadPool::start()`, removing MSVC/Qt warning C4858 without changing operation scheduling semantics.

English is the canonical project language. Ukrainian mirrors are provided in `README_UA.md` and `docs/uk/`.

OpenAstroLink (OAL) is a local-first observatory control stack. The reference path is **native OAL drivers**; INDI, ASCOM Alpaca and LX200 remain optional compatibility layers for equipment that does not yet have a native OAL driver.



## v0.2.10.21 mount geometry, mechanical Park, and explicit QHY recovery

- Native hardware is enumerated once after node startup. There is **no periodic vendor polling**. Hardware connected later is discovered when the user presses **Refresh native device discovery** (or calls `POST /api/v1/drivers/refresh`).
- Refresh is asynchronous on a remote node, so QHY/serial/vendor discovery does not freeze the GUI or HTTP control plane.
- The refresh is vendor-neutral: it asks the native registry to enumerate all enabled drivers rather than assuming a QHY camera is present.
- **J2000 is the canonical OAL equatorial frame.** Mount GOTO/Sync API bodies accept `coordinateFrame: J2000|JNOW`; omitted frame means J2000.
- The Mount GUI can enter/display `J2000 / catalog` or `JNow / of-date`. The node converts JNow input to J2000.
- Stellarium bridge coordinates are treated as J2000. Classic ASCOM reads `EquatorialSystem`; J2000 is passed through, while an ASCOM topocentric/of-date driver is currently approximated as JNow using precession at the compatibility boundary (full apparent/topocentric correction is not yet implemented).
- `MountGeometry` profiles now cover GEM, fork-equatorial, Alt-Az, Alt-Az+derotator, equatorial platform and custom two-axis. Native raw-axis drivers no longer own sky geometry.
- Mechanical Home/Park is stored separately from celestial coordinates; default direct-MC Home/Park is Axis1=0°, Axis2=0°, and the GUI can set the current mechanical axes as Park.
- Native EQDrive and direct SynScan/EQDrive Wi-Fi use the same Core geometry model. A known-position Sync establishes installation encoder offsets/signs.
- Explicit **Refresh native device discovery** may hard-reload the QHY OAL driver DLL (and therefore its QHYCCD dependency) after a zero-device scan, but only when no QHY camera is active. There is still no periodic vendor polling.
- Classic ASCOM slews now emit periodic RA/DEC/pier/tracking diagnostics while moving, useful for distinguishing EQMOD geometry from OAL coordinate conversion.
- See `docs/COORDINATE_FRAMES.md` and `docs/MOUNT_GEOMETRY.md`.

## v0.2.10.19 HIL startup/catalogue/mount correction

- **Server-first node startup:** driver libraries are loaded, HTTP/WebSocket listeners start, and hardware enumeration runs on a background thread. Slow Gemini reset recovery or EQDrive/QHY discovery no longer prevents the GUI from connecting to the node.
- **Live native catalogue fix:** native device records are no longer incorrectly filtered by a driver-only `native` flag. A Gemini/QHY/native mount discovered after startup is propagated in node state and appears in the remote GUI combobox without restarting the GUI.
- **Asynchronous serial selector:** applying a Gemini/Sky-Watcher/EQDrive COM override queues discovery instead of blocking the GUI. If the selected native device is already cached, no redundant serial reopen/reset is performed.
- **Direct SynScan/EQDrive Wi-Fi:** `synscan-wifi` continues to talk directly to the mount adapter over UDP 11880, while `synscan-app` remains the separate SynScan Pro/App compatibility backend on UDP 11881.
- **Motor Controller semantics corrected:** running/GOTO/initialization/direction bits and GOTO motion-mode encoding now follow the Sky-Watcher/EQMOD convention. Direct GOTO uses the proven `G -> H -> M -> J` sequence and verifies encoder movement before reporting success.
- **Native EQDrive dual protocol:** `oal.eqdrive` first probes the EQMOD-compatible Sky-Watcher Motor Controller command set used by the proven EQMOD path, then falls back to official EQDrive ASTEP (`St`, `Pos`, `Cg`, `Speed`, `Goto`) where exposed by firmware. A busy COM port exits discovery immediately rather than fighting EQMOD/ASCOM.
- **Long GOTO enabled:** direct Wi-Fi and native EQDrive still require Sync, but the temporary 15° HIL envelope is removed. Automatic meridian flips remain disabled; supervise long slews and keep ABORT available.

## Current hardware coverage

Native OAL driver code exists for:

- QHY cameras (`oal.qhy`, QHYCCD SDK);
- Canon EOS (`oal.canon`: Canon EDSDK on Windows/macOS; selectable EDSDK or libgphoto2/PTP on Linux);
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
- **Native rediscovery:** periodic reconnect probing is disabled. Canon EDSDK can request bounded automatic hot-plug rediscovery; other later-connected devices use explicit Refresh, while serial-driver COM selection refreshes only the selected driver.
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
  -BuildDir build/windows-observatory-msvc-ninja `
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

# macOS build (Apple Silicon or Intel)

Host-native macOS presets use Apple Clang. The build wrapper searches existing dependencies first and can bootstrap redistributable Qt/OpenCV/QHY/ZWO dependencies. Canon EDSDK remains user-supplied/local-discovery only. Apple Silicon/Intel preset support is implemented; physical Mac build/HIL qualification is still pending.

```bash
xcode-select --install   # if the command-line tools are not installed
brew install cmake        # Homebrew is optional but convenient
cp CMakeUserPresets.example.json CMakeUserPresets.json
./scripts/build_macos.sh my-macos-arm64-observatory --bootstrap-deps
```

Use `my-macos-x86_64-observatory` on Intel Macs. Physical Mac build/HIL qualification and signed/notarized app packaging are still pending.

# Native Linux build (x86_64 or ARM64/RPi)

> **Linux baseline:** the system Qt must be **6.4 or newer**. Ubuntu 22.04/Jammy ships Qt 6.2.4 and has no `qt6-httpserver-dev`, so it is not a supported system-Qt build host. Use Ubuntu 24.04+ or Debian/Raspberry Pi OS 12 Bookworm+, or provide a custom Qt >= 6.4 through `CMAKE_PREFIX_PATH`/`Qt6_DIR`.

The same node and GUI can own hardware directly on a normal Linux workstation, mini-PC or Raspberry Pi.

WSL is useful for compilation/testing, but direct observatory hardware access is better on native Linux unless USB/serial passthrough has been configured explicitly.

## Base dependencies (Debian/Ubuntu family)

Package names vary slightly by distribution/release. Typical dependencies are:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  qt6-positioning-dev libopencv-dev libgphoto2-dev libjpeg-dev
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

Then use the canonical wrapper. It searches existing dependencies first and bootstraps missing redistributable ones. On Ubuntu 22.04/Jammy it installs a supported per-user Qt instead of using the system Qt 6.2.4:

```bash
./scripts/build_linux.sh my-linux-observatory --bootstrap-deps --clean
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
*-native-release             native OAL only
*-observatory-release        native OAL drivers by default (INDI OFF)
*-observatory-indi-release   native OAL + optional INDI compatibility
```

`OAS_ENABLE_INDI` now defaults to `OFF` globally. A running `indiserver` is only needed when you actually use an INDI-only device. Enabling INDI never replaces a native OAL driver; native drivers remain the preferred/default transport.

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

1. Native device discovery is now cached. Normal `/api/v1/node/backends`, `/api/v1/state`, and GUI startup reads do **not** rescan USB/serial hardware. A vendor-neutral rescan is started once **after** the HTTP/WebSocket control plane reports ready and thereafter only by the explicit **Refresh native device discovery** action (`POST /api/v1/drivers/refresh`). This prevents serial probing from causing the remote GUI's 3 s metadata timeout.
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

### v0.2.10.25 Canon EOS 550D HIL follow-up

EOS 550D now has HIL-confirmed 1–30 s non-AF exposure and original-file transfer. This patch fixes the remaining observed issues: explicit hot-plug Refresh waits for delayed EDSDK enumeration, CR2/JPEG preview falls back to the stored file/embedded JPEG instead of failing on an empty EDSDK thumbnail, and >30 s Bulb uses held `Completely_NonAF` shutter with `BulbStart/BulbEnd` only as fallback. The next HIL should verify hot-plug Refresh, GUI preview, 45 s Bulb and cancellation.

## v0.2.10.29 Canon hot-plug EDSDK thread-affinity fix

- Fixes a Canon EOS hot-plug regression where final fallback discovery found and connected the camera, ISO/shutter commands worked, but exposures timed out waiting for `kEdsObjectEvent_DirItemRequestTransfer`.
- Root cause: the v0.2.10.28 hard-reload fallback called `EdsInitializeSDK()` from the short-lived asynchronous discovery `QThread`. Canon EDSDK event/callback delivery is thread-affine; once that worker exited, transfer callbacks were no longer delivered.
- Canon DLL/EDSDK hard restart is now marshalled synchronously to the long-lived `ApplicationController` Qt event-loop thread, matching the already-working cold-start path. Enumeration remains asynchronous.
- QHY and serial-driver discovery/recovery behavior is unchanged.

### build-fix4 cross bootstrap

Linux/WSL Raspberry Pi cross builds can now prepare their target environment explicitly with `./scripts/bootstrap_rpi_cross.sh arm64`. The script can create a Debian 12/Bookworm ARM sysroot locally or mirror a real Pi with `--from-pi`, prepares matching host Qt tools, and stages a compatible QHY ARM SDK from a sibling `QHYCCD_Linux_New` checkout after ELF-architecture validation.

### build-fix5 WSL foreign-architecture bootstrap hardening

The local Bookworm bootstrap now explicitly mounts `binfmt_misc`, registers and verifies the `qemu-aarch64`/`qemu-arm` handler, and executes `/bin/true` inside the foreign sysroot before debootstrap stage two. This fixes WSL hosts where installing `qemu-user-static` alone leaves ARM binaries failing with `Exec format error`. An interrupted build-fix4 sysroot is resumed safely; `--recreate` remains available for a clean restart. The bootstrap also installs `debian-archive-keyring` and includes Qt Positioning in the target development package set.

### build-fix6 Bookworm signing-key bootstrap

Ubuntu 22.04/Jammy ships a `debian-archive-keyring` too old to validate current Debian 12/Bookworm `InRelease` signatures. The cross bootstrap no longer falls back to `--no-check-gpg`: it fetches the official Bookworm archive, release and security public keys from Debian over HTTPS, verifies pinned fingerprints, writes a cached OAL-local binary keyring, and passes it explicitly to `debootstrap --keyring`. The generated target also enables Bookworm main, updates and security repositories before installing Qt/OpenCV development dependencies.

### build-fix7 verified-key bootstrap under Bash `set -u`

The Debian 12 signing-key fetch helper no longer declares `name` and the dependent `out` initializer in one `local` command. This removes the `name: unbound variable` failure seen in WSL/Jammy when running `bootstrap_rpi_cross.sh`.


### build-fix8 target package discovery in ARM cross sysroots

The ARM64/ARMHF toolchains now pin Debian multiarch package locations and seed the target `Qt6_DIR` and `OpenCV_DIR` from the already-bootstrapped sysroot. `OAS_QT_HOST_PATH` remains host-tools-only for runnable `moc`/`rcc`/`uic`. The bootstrap env record persists both target package directories and the build helper passes them back to CMake.


### build-fix9 QHY cross-staging contract

The first physical build after build-fix8 proved that the ARM64 Bookworm sysroot, target Qt/OpenCV discovery and host Qt tools are valid, but also exposed a bootstrap contract bug: QHY staging failure could be downgraded to a warning while the final environment summary still advertised a non-existent `qhy-arm64` directory. build-fix9 makes QHY staging fatal whenever it is requested, validates the exact `include/qhyccd.h` and `lib/libqhy.so` paths consumed by CMake, follows package symlinks safely, and passes the staged paths from `.oal/rpi-cross-*.env` into the build helper. `--no-qhy` remains available for a deliberately QHY-free base build.

### build-fix10: QHY `armv8` ABI correction

The legacy `qhyccd-lzr/QHYCCD_Linux_New` archive named `...armv8.tar.gz` is not an AArch64 SDK: physical `file(1)` inspection reports `ELF 32-bit ... ARM, EABI5`. Cross staging now selects QHY libraries by actual ELF architecture rather than filename. ARM64 bootstrap may continue with QHY disabled in the default `auto` mode; use `--require-qhy` only when a genuine current QHY `Arm_64`/`AARCH64` SDK is available. The legacy archive is usable by the ARMHF build.


### build-fix12: first physical ARM64 source portability fixes

The Bookworm/AArch64 cross build now gets past configure and into the real source tree. build-fix12 adds a local Linux compatibility shim for Canon EDSDK 13.20.x (`__int64`/`WCHAR`) and makes the Qt HttpServer bind path compatible with Qt 6.4-6.7 as well as Qt 6.8+.


### build-fix13: ARM64 sysroot transitive-link closure

The physical WSL -> AArch64 build now compiles every enabled OAL target and reaches the final 98-100% executable link. The remaining failure was not an OAL source error: GNU ld could see OpenCV in the Bookworm sysroot but did not search that foreign sysroot for transitive `DT_NEEDED` libraries (`libblas.so.3`, `liblapack.so.3`, Armadillo/ARPACK/SuperLU and the matching Bookworm libc). build-fix13 adds link-time-only `-rpath-link` and `-L` search directories for the target sysroot multiarch/runtime directories. It also makes BLAS/LAPACK explicit bootstrap dependencies and validates their SONAMEs before declaring the sysroot ready. No runtime RPATH is embedded, and mount v9 is unchanged.

### build-fix14: official QHY 26.x ARM64 SDK bootstrap

The RPi cross toolchain can now fetch and stage the official QHYCCD Linux ARM64 SDK directly. `scripts/fetch_qhy_sdk.sh arm64 26.06.04` maps the post-26.06.04 QHY packaging scheme to `sdk_linux_arm64_26.06.04.tar.gz`, caches the vendor archive, then delegates to architecture-validated staging. The stager now accepts both QHY shared libraries and the official static `libqhyccd.a`, preferring shared libraries when both are present and validating static archives by inspecting an actual ELF object member. `bootstrap_rpi_cross.sh --qhy-version 26.06.04 --require-qhy` enables a reproducible full ARM64 vendor build without relying on the legacy mislabeled 32-bit `armv8` archive. Mount v9 is unchanged.

### build-fix15: Debian BLAS/LAPACK alternatives closure for ARM cross-link

The first physical build with the official QHYCCD 26.06.04 ARM64 SDK proved the QHY path itself: the vendor archive downloaded, staged as a real AArch64 `libqhy.so`, and `oal_driver_qhy.so` compiled and linked successfully. The only remaining failure is the final `openastrolink-node` / `oal-hardware-probe` link. Debian Bookworm installs the reference BLAS/LAPACK SONAMEs under `usr/lib/<multiarch>/blas` and `usr/lib/<multiarch>/lapack`, with alternatives-managed links above them. build-fix14 searched only the parent multiarch directory, and its post-APT `symlinks -cr` ran without root privileges against a root-owned sysroot, producing `Permission denied` and leaving absolute alternatives links unrepaired. build-fix15 normalizes sysroot symlinks with `sudo`, adds the BLAS/LAPACK subdirectories to link-time-only `-rpath-link`/`-L`, validates that both leaf libraries are target-architecture ELF files, and links target `liblapack.so.3` / `libblas.so.3` explicitly after OpenCV. No runtime sysroot RPATH is embedded. Mount v9 is unchanged.

## v0.2.10.49 build-fix16: native vendor bootstrap and RPi GUI cross preset

`build-fix16` records the first successful full ARM64 cross-build of `openastrolink-node` and `oal-hardware-probe` with Canon EDSDK, ZWO ASI/EAF, and the official QHYCCD 26.06.04 AArch64 SDK. It also removes two setup asymmetries:

* `scripts/bootstrap_vendor_sdks.sh` can stage host-native QHY and ZWO SDKs on Linux/macOS. QHY is downloaded from QHYCCD's official versioned SDK repository; ZWO ASI/EAF blobs are fetched from a pinned `indi-3rdparty/libasi` revision so builds are reproducible instead of scraping ZWO's moving download page.
* `scripts/bootstrap_vendor_sdks.ps1` downloads/stages the official QHY Windows x64 SDK and auto-discovers an already installed/unpacked ZWO Windows SDK. Canon EDSDK remains user-supplied because Canon distribution/licensing is not suitable for silent third-party redistribution/bootstrap.
* `build_linux.sh` and `build_macos.sh` accept `--bootstrap-vendor` / `--use-vendor-sdk`; `build_windows.ps1` accepts `-BootstrapVendor` / `-UseVendorSdk`.
* `rpi4-cross-arm64-observatory-release` builds **both** `OpenAstroSuite` and `openastrolink-node`. The older `rpi4-cross-arm64-node-release` deliberately sets `OAS_BUILD_GUI=OFF`; that is why a successful node cross-build did not produce `OpenAstroSuite`.

Examples:

```bash
# Native Linux: stage QHY + ZWO, then configure/build with those SDKs.
./scripts/build_linux.sh my-linux-observatory --bootstrap-vendor

# macOS Apple Silicon.
./scripts/build_macos.sh my-macos-arm64-observatory --bootstrap-vendor

# RPi ARM64 GUI + node cross-build after the normal RPi sysroot bootstrap.
cmake --preset my-rpi4-cross-arm64-observatory-full
cmake --build --preset my-rpi4-cross-arm64-observatory-full -j"$(nproc)"
```

Windows PowerShell:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -BootstrapVendor -Clean
```

Qt/OpenCV and open-source system libraries should still come from the OS/package manager (or the Qt installer on supported hosts) rather than being vendored into OAL. Canon EDSDK is intentionally not auto-downloaded.


## v0.2.10.49 build-fix17: native-first defaults; INDI opt-in

All standard observatory/node presets now default to native OAL drivers with `OAS_ENABLE_INDI=OFF`. INDI remains available as an explicit compatibility layer through `*-indi-release` presets or `-DOAS_ENABLE_INDI=ON`; it is intended only for equipment without a native OAL driver. The pinned `indi-3rdparty` source used by the vendor bootstrap for ZWO on Linux/macOS is only an SDK blob mirror and does **not** enable or build the INDI runtime. Physical WSL -> AArch64 evidence now also confirms the full native ARM64 node/probe build reaches 100% with QHYCCD 26.06.04, Canon EDSDK and ZWO ASI/EAF. Mount v9 geometry/control remains frozen and unchanged.

### Ubuntu 22.04 / Jammy Qt note

Jammy provides Qt 6.2.4 only. Installing more Jammy `qt6-*`/`libqt6*-dev` packages does not satisfy OpenAstroLink's Qt >= 6.4 + Qt HttpServer requirement. Use:

```bash
./scripts/build_linux.sh my-linux-observatory --bootstrap-deps
```

The wrapper installs a complete per-user Qt through `aqtinstall` under `~/.local/share/openastrolink/qt` and does not require the Qt GUI installer or a Qt account. Do not run the Qt GUI installer with `sudo`. The Linux wrapper also removes a stale CMake cache automatically when a checkout has moved between WSL (`/mnt/c/...`) and native Linux (`~/...`).
