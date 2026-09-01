# OpenAstroSuite / OpenAstroLink status — v0.2.10.47

## v0.2.10.47 mixed scheduler execution

- ✅ DSO FITS/RAW block executor retained.
- ✅ Planetary SER block executor: GOTO, full-frame detection, planet autofocus, hardware ROI, finite raw SER and ROI provenance.
- ✅ QHY + ZWO ASI native live ROI implementation; ZWO real-hardware HIL remains pending.
- 🟡 Fast ROI tracker implemented; real planetary HIL pending.
- 🟡 Calibrated slow mount correction implemented but disabled by default pending per-backend HIL.
- ⏳ Durable restart/recovery, weather/roof safety, meridian flip and thermal in-exposure focus remain OAL 1.0 work.

## v0.2.10.46 scheduler execution

- `ObservationPlan` / `ObservationBlock` is now the primary scheduler model; legacy `SessionTarget` remains accepted.
- Supervised DSO blocks execute asynchronously: slew, adaptive solve/recenter with configurable tolerance/retries, autofocus, then FITS/RAW science frames. Recenter/autofocus can repeat every N science frames.
- Planetary SER block data model exists, but autonomous SER/ROI/centroid execution is not enabled yet. Scheduler checkpoints/restart/recovery remain non-durable.

## v0.2.10.45 HIL follow-up

- New HIL falsified the earlier assumption that direct-MC v6 geometry was correct: native EQDrive serial and direct SynScan Wi-Fi move identically but both miss the target, while EQMOD Classic ASCOM points correctly on the same hardware.
- Coordinate model v7 now matches the EQMOD-style physical GEM pointing-state convention instead of treating the two mathematical GEM representations as freely interchangeable shortest paths. For the 2026-08-31 Moon HIL, the old model selected approximately `(-45.69°, -77.30°)`; v7 predicts approximately `(+44.31°, +77.30°)` on `pier=west`.
- QHY HIL capture statistics are close to linear with exposure until clipping. Histogram guidance now measures fixed sensor scale and no longer repeatedly auto-applies changes from unchanged Live View frames.
- SER sidecar metadata and auxiliary observatory device placeholders are implemented in code; real hardware drivers for those new classes remain pending.

## v0.2.10.44 superseded transport-parity step

- Reverted the unsupported Wi-Fi-only DEC polarity hypothesis from v0.2.10.43. The same physical Motor Controller now has one raw-count/mechanical-axis convention on serial and UDP/11880.
- Native serial and direct Wi-Fi GOTO share one `makeGotoPlan()` implementation, eliminating transport-specific direction/count drift.
- Direct Wi-Fi status reports the exact last GOTO deltas, counts and forward bits for HIL comparison.

## v0.2.10.43 HIL follow-up

- Native serial EQDrive is confirmed correct under coordinate model v6.
- Direct `synscan-wifi` HIL indicates mirrored DEC transport polarity on the EQDrive Wi-Fi profile (`9216000/9216000`, timer `53694`); the backend now applies Axis2 transport sign `-1` only for that profile and keeps generic UDP/11880 at `+1/+1`.
- UDP socket shutdown is guarded against post-close datagram polling.


## v0.2.10.42 HIL follow-up

- EQMOD/Classic ASCOM site is authoritative by default; OAL adopts backend latitude/longitude/elevation and uses it for its own coordinate conversions. Blank Classic ASCOM endpoint defaults to `EQMOD.Telescope`.
- Native EQDrive serial and direct SynScan/EQDrive Wi-Fi use direct-MC coordinate model v6 with one mechanical axis convention: the controller counts captured at direct-MC connect define session Home/Park `Axis1=0°, Axis2=0°`; controller position packets remain integer step-count diagnostics only.
- The v6 GEM planner uses polar telescope-direction-vector geometry, evaluates both equivalent GEM branches, and selects the shortest physical route from the current axes. Native Stellarium telemetry is refreshed from live axes at 250 ms while slewing.
- High-level SynScan hand-controller and SynScan App backends remain RA/DEC-native and do not use the direct Motor Controller mechanical model.

## v0.2.10.38 HIL follow-up

- Remote mount profile transport carries preferred `maxGotoSkyDeltaDeg` (plus the legacy alias) and repeatable Home calibration; native safety now measures true sky separation while raw transport retains a separate 180° hard cap.
- Equatorial native mounts can restore their coordinate model automatically from a saved mechanical Home pose; operational profile edits preserve Sync.
- Mount GUI is vertically scrollable and GOTO can auto-unpark; persistent Home/Park has one calibration action across native and Classic ASCOM (`SetPark`).
- Classic ASCOM verifies its driver site against the OAL profile before GOTO, attempts automatic site/time synchronization, blocks unsafe EQMOD site mismatch, and reports driver Az/Alt/LST plus EquatorialSystem diagnostics.
- QHY native Live→Stop→Capture and FITS science spooling are HIL-confirmed.
- Optional preview-only debayer is camera-neutral; QHY Auto uses SDK `CAM_COLOR`, ZWO Auto uses ASI Bayer metadata, and explicit CFA patterns support other one-channel Bayer sources.
- Autofocus preview and manual focus jog are available; Scene AF peak selection was hardened after daylight HIL.
- Mount target entry now synchronizes J2000/JNow/Az-Alt/Galactic fields while keeping J2000 canonical for GOTO/Sync.

Status legend: ✅ implemented; 🟡 implemented/partially implemented but HIL or production qualification pending; 🧪 experimental; ⏳ not yet implemented.

## Core and control plane

| Area | Status | Notes |
|---|---|---|
| GUI / node separation | ✅ | Node owns hardware and workflows; GUI can be local or remote |
| Node survives GUI exit | ✅ | Confirmed in simulator runtime tests |
| Device reconnect/state snapshot | ✅ | GUI rehydrates aggregate state from node |
| Per-device disconnect | ✅ | Main camera, guide camera, mount, focuser independent |
| HTTP API | ✅ | `/api/v1` reference API present |
| WebSocket state/events | 🟡 | Channel exists; reliable sequence/replay contract still missing |
| Stellarium bridge | ✅ basic HIL | Stellarium GOTO confirmed through node and Classic ASCOM/EQMOD; native-mount parity still pending |

## Operations

| Area | Status | Notes |
|---|---|---|
| Operation resource/state machine | ✅ | queued/running/succeeded/failed/cancelled |
| Progress/phase/result/problem | ✅ | Stored in operation records |
| Cancellation | ✅ | Implemented for operation-backed paths where driver supports it |
| Resource locks | ✅ | Main camera, guide camera, mount, focuser locking |
| Async mount slew | ✅ | Operation-backed |
| Async main/guide exposure | ✅ | Operation-backed |
| Async autofocus | ✅ / HIL partial | Camera + focuser reservation; real Gemini motion during autofocus confirmed; optical convergence/repeatability still to qualify |
| Idempotency | ⏳ | HTTP `Idempotency-Key` not implemented |
| Durable operations | ⏳ | Operations are not yet restored after node restart |

## Native driver platform

| Area | Status | Notes |
|---|---|---|
| Native OAL ABI v2 | ✅ | Manifest/lifecycle/discovery/capabilities/events/cancellation/frame publication foundation |
| Driver registry | ✅ | Manifest/device enumeration and generic adapters |
| Reference simulator | ✅ | Camera/mount/focuser |
| Out-of-process sandbox | ⏳ | Manifest policy groundwork exists; driver host not production implemented |
| Third-party conformance suite | ⏳ | Project checks exist, public conformance suite does not |

## Hardware drivers

| Driver | Status | Notes |
|---|---|---|
| QHY | 🟡 HIL active | QHY5III462C discovery/connect/single capture confirmed; user captures spool to FITS. v0.2.10.34 moves Finder Live View to native QHYCCD continuous streaming and makes health probing idle-only with a three-strike disconnect rule; Live→Stop→Capture and repeated reconnect remain the next Windows HIL checks |
| Canon EOS | 🟡 | EDSDK Windows / libgphoto2 Linux; HIL pending |
| ZWO ASI | ✅ implemented / 🟡 HIL pending | Native ASI SDK and multi-camera discovery implemented; real ZWO camera not yet qualified |
| ZWO EAF | ✅ implemented / 🟡 HIL pending | Native EAF SDK implementation exists; real ZWO EAF not yet qualified |
| Gemini EAF | ✅ basic HIL | Windows native discovery/connection, direct motion and autofocus-driven motion confirmed; active serial health polling now detects physical disconnect and refreshes the device catalogue; optical autofocus convergence/repeatability remains to qualify |
| Sky-Watcher/SynScan | ✅ direct Wi-Fi basic HIL | Serial SynScan path retained; `synscan-wifi` direct Motor Controller UDP/11880 has connected and physically moved the mount; `synscan-app` remains the SynScan Pro/App UDP/11881 compatibility path |
| Sky-Watcher direct motor-controller | 🧪 | Shared Motor Controller codec now follows EQMOD/INDI status/direction semantics and is used by direct Wi-Fi plus the EQDrive native fallback |
| EQDrive native | ✅ basic HIL / 🟡 geometry | Separate `oal.eqdrive` discovered and moved the real controller; raw axes route through the Core geometry layer. HIL exposed unqualified installation axis signs, so sky GOTO is back under a 15° qualification envelope until orientation is verified; pier-flip HIL remains pending |
| INDI compatibility | ✅ | Optional compatibility client; independent of native drivers |
| Classic ASCOM | ✅ basic HIL | Windows out-of-process COM bridge; EQMOD HEQ5/6 connect, park/unpark and Stellarium-driven GOTO confirmed |
| ASCOM Alpaca | ✅ | Compatibility backend |
| LX200 | ✅ | Minimal compatibility path |

## Imaging and optics

| Area | Status | Notes |
|---|---|---|
| Main + guide camera roles | ✅ | Independent roles and locks |
| Optical train profiles | ✅ | Main/guide aperture, focal length, pixel/sensor data, derived sampling |
| Single-frame capture | ✅ | Main/guide paths |
| In-memory preview | ✅ | PNG preview endpoint |
| Native frame publication | ✅ | Driver ABI supports non-JSON frame handoff |
| Canon original-file spool | ✅ | Driver path supports original file handling |
| Durable FITS/RAW store | ⏳ | Final science data plane not complete |
| Planetary SER pipeline | ⏳ | Production ring-buffer/SER/drop accounting not complete |

## Astronomy workflows

| Area | Status | Notes |
|---|---|---|
| ASTAP adapter | 🟡 | Adapter exists; real-sky qualification pending |
| Coordinate frames | ✅ foundation | J2000 canonical API/GUI/session frame; optional JNow/of-date input via precession; full apparent/topocentric corrections not yet implemented |
| Adaptive urban solve | 🟡 | Short-exposure quality gate + background removal + star registration/stack + mount hint + retry operation implemented; real city-sky HIL pending |
| Closed-loop GOTO/recenter | 🟡 | Groundwork exists; end-to-end HIL pending |
| Autofocus | 🟡 | Algorithm/operation exists; real optics/backlash tuning pending |
| Polar-axis math | ✅ | Sample/estimation API exists |
| Automatic polar wizard | 🟡 | Full live adjustment orchestration not production-qualified |
| Guiding | 🟡 | Basic state/API and pulse-guide primitives; production loop pending |
| Session/scheduler | 🟡 mixed executor | v0.2.10.47 executes DSO FITS/RAW and planetary SER blocks; planetary ROI tracking and optional slow mount correction require real-sky HIL, while durable restart/recovery remains pending |
| Target resolver/ephemerides | 🟡 | Not yet a complete normative production service |

## Mount geometry foundation — v0.2.10.25

- J2000/JNow sky coordinates are separate from raw mechanical axes.
- Geometry profiles: German equatorial, fork equatorial, Alt-Az, Alt-Az+derotator, equatorial platform, custom two-axis.
- Native EQDrive and direct SynScan/EQDrive Wi-Fi use `MountGeometryModel` in OAL Core; drivers expose raw axes/motion.
- GEM conversion uses UTC/site longitude → local sidereal time → hour angle → selected pier branch → mechanical axes. One Sync establishes installation encoder offset/signs.
- Mechanical Park has no active default for raw native mounts. The user must explicitly save the current safe physical axes; stopping an in-progress park invokes the physical abort path.
- Automatic meridian flips remain disabled by default. After unsafe-direction HIL, native EQDrive sky GOTO again uses a 15° qualification envelope until Axis 1/2 orientation is verified with small moves.
- Alt-Az coordinate conversion exists; production two-axis tracking/derotator control remains future work.
- Classic ASCOM slews emit periodic RA/DEC/pier/tracking diagnostics.
- Explicit Refresh can hard-reload an inactive QHY OAL driver DLL/SDK after a zero-device scan; periodic vendor polling remains disabled.

## P0 protocol hardening

| Item | Status |
|---|---|
| Capabilities/identity/discovery | 🟡 foundation implemented; schemas still evolving |
| Async operations | 🟡 major paths implemented; not every long workflow converted |
| Resource locks | ✅ |
| Idempotency | ⏳ |
| TLS/auth/roles/scopes/audit | ⏳ |
| Safety interlocks/weather/roof/power | ⏳ |
| Separate durable science data plane | 🟡 ABI/preview foundation only |
| Reliable replayable WebSocket stream | ⏳ |
| RFC 9457 HTTP Problem Details | 🟡 operation problems exist; HTTP model incomplete |
| Conformance suite | 🟡 regression checks/simulator exist; public suite incomplete |

## Cross-platform build status

| Target | Status | Notes |
|---|---|---|
| Windows x64, MSVC + Ninja | 🟡 | Configure path qualified; full vendor SDK build still being iterated on physical host |
| Linux x86_64 | 🟡 | Presets and docs prepared; vendor SDK architecture/runtime must be qualified |
| Linux ARM64 / Raspberry Pi | 🟡 | Same Linux architecture; hardware HIL pending |
| WSL compile/test | 🟡 | Supported for build testing; direct hardware requires explicit passthrough |

### v0.2.10.5 build fixes

- CMake preset schema reduced to **v2**, compatible with CMake 3.20+.
- `cmake_minimum_required` reduced to 3.20 because the project does not require 3.24-only CMake features.
- Windows official preset path is now **Ninja + MSVC**, avoiding Visual Studio instance discovery and avoiding MinGW/MSVC ABI mixing.
- Windows build helper can load `vcvars64.bat` automatically.
- QHY Windows header isolation retained from v0.2.10.2.
- `CMakeUserPresets.example.json` now has a no-Canon Windows preset and a separate EDSDK-enabled preset.
- Linux SDK staging and architecture checks are documented.

## Release posture

v0.2.10.25 is a **supervised HIL qualification release**, not yet an unattended observatory release. Windows HIL has confirmed graceful shutdown, Gemini native motion/autofocus motion, native EQDrive discovery/motion, direct SynScan/EQDrive Wi-Fi motion, Classic ASCOM through EQMOD, and Stellarium-driven GOTO. QHY connection/capture has worked, but runtime hot-plug/repeated-capture stability remains under qualification. Coordinate handling is now explicitly J2000-canonical with optional JNow input; full GEM sky↔axis/pier geometry and configurable mechanical park remain major mount tasks.
