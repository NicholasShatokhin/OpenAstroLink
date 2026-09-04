## v0.2.10.50 implementation boundary

The protocol/driver model remains native-first with INDI as opt-in compatibility. Cross-platform build qualification now covers Windows x64, Linux x86_64 and Raspberry Pi/Linux ARM64 node targets. Direct-MC mount coordinate model v9 is HIL-qualified and unchanged; the EQDrive transport no longer carries a hidden temporary sky-GOTO qualification envelope. Operator safety belongs to Core/profile policy, while raw-axis calls expose their explicit mechanical guard.

## v0.2.10.47 implementation note

The scheduler now executes both DSO FITS/RAW and planetary SER blocks. Planetary blocks acquire the target on a full frame, optionally autofocus in planet mode, derive a hardware ROI, record a finite SER, move the ROI while preserving its size, and write frame-indexed ROI-origin provenance. Slow mount recentering is an optional calibrated loop and is disabled by default until HIL-qualified. These supervised capabilities do not change the OAL 1.0 planned boundary for durable restart/recovery, weather/roof interlocks, meridian handling, authentication or process isolation.

# OpenAstroLink specification and implementation status

**Canonical language:** English  
**Release covered:** OpenAstroSuite / OpenAstroLink v0.2.10.50
**Status legend:** ✅ implemented in the current codebase; 🟡 partially implemented / hardware qualification pending; 🧪 experimental; ⏳ specified but not yet implemented.

This document is the implementation-facing specification snapshot. It distinguishes the target OAL protocol/driver architecture from features that are merely planned.

## 1. Architectural principles

### 1.1 Node ownership

✅ The observatory hardware is owned by `openastrolink-node`, not by the GUI. A GUI may run on the same Windows/Linux/RPi host or connect remotely. GUI termination must not terminate the node or intentionally disconnect devices.

### 1.2 Native-first hardware model

✅ Native OAL ABI-v2 drivers are the reference path.  
✅ INDI, ASCOM Alpaca and LX200 remain compatibility/migration layers.  
✅ A compatibility adapter must not constrain the native OAL capability model.

### 1.3 Platform model

✅ Windows x64, Linux x86_64 and Linux ARM64/RPi are intended as first-class observatory hosts.  
✅ The same node/GUI architecture is used when hardware is connected directly to a desktop computer.  
🟡 Vendor SDK linkage/runtime installation remains platform-specific and requires HIL/build qualification.

## 2. Native driver ABI v2

✅ Driver manifest and stable `driverId`.  
✅ Lifecycle start/stop.  
✅ Device discovery/enumeration.  
✅ Identity and typed JSON capability publication.  
✅ Health reporting.  
✅ Push event callback foundation.  
✅ Cooperative cancellation callback.  
✅ Native frame publication callback/descriptor.  
✅ Declared thread/concurrency model with loader-side normal-call serialization.  
⏳ Sandboxed out-of-process driver host and crash restart policy.  
⏳ Full ABI compatibility/conformance certification suite for third-party drivers.

## 3. Device identity, capabilities and discovery — P0

**Requirement:** clients must not assume that every mount, camera or focuser supports the same functions.

✅ Native driver registry and device enumeration.  
✅ Per-device capability endpoint.  
✅ Device identity includes driver, device identifier and type.  
✅ Native camera/focuser/mount drivers publish device-specific capabilities.  
🟡 Capability vocabulary exists but is not yet a frozen normative cross-vendor schema for every future device class.  
⏳ Formal capability profiles for filter wheels, rotators, roofs/domes, weather, power/switch, cover/calibrator and GPS/time.

## 4. Asynchronous operation model — P0

Target operation states:

```text
queued → running → succeeded
                 ↘ failed
                 ↘ cancelled
```

✅ Operation resources and listing APIs.  
✅ Progress/phase/result/problem fields.  
✅ Cancellation request support.  
✅ Resource ownership/locks.  
✅ Mount slew is asynchronous.  
✅ Main/guide camera exposure is asynchronous.  
✅ Autofocus is asynchronous.  
🟡 Some other long workflows/endpoints remain synchronous or only partially operation-backed, including the current solve/session/polar orchestration paths.  
⏳ Durable operation state across node restart.

## 5. Resource locks and idempotency — P0

✅ Resource lock manager exists.  
✅ Autofocus reserves camera + focuser.  
✅ Mount slew reserves mount.  
✅ Main camera and guide camera have distinct resources (`camera`, `camera.guide`).  
✅ Conflicting queued operations can wait rather than corrupt device state.  
⏳ HTTP `Idempotency-Key` semantics.  
⏳ Persistent idempotency records across process/network recovery.  
⏳ Complete lock graph for meridian flip, guider, filter wheel, dome/roof and future safety workflows.

## 6. Security and safety interlocks — P0

⏳ TLS termination/native TLS policy.  
⏳ Authentication and role/scope authorization.  
⏳ Audit log with actor/action/device/operation provenance.  
🟡 Device-level abort/halt exists for several current drivers where the hardware protocol is known.  
⏳ Observatory-wide emergency stop semantics.  
⏳ Weather/roof/power safety policy engine with priority over user workflows.  
⏳ Internet-facing deployment hardening.

**Current deployment rule:** use trusted LAN/VPN only; do not expose the unauthenticated HTTP/WebSocket ports directly to the public Internet.

## 7. Camera data plane — P0

✅ Native ABI frame publication avoids making vendor drivers return large frames as JSON.  
✅ In-memory recent-frame preview and PNG preview endpoint.  
✅ Original Canon files can be spooled by the Canon native path.  
🟡 QHY/ZWO native capture paths exist, but production high-rate planetary streaming still needs HIL and durable writer integration.  
⏳ Final FITS/RAW science-frame store with checksums and provenance.  
⏳ Resumable/range download API for science frames.  
⏳ Shared-memory/zero-copy local-host path.  
⏳ Object-storage backend.  
⏳ Production SER writer/ring-buffer/drop accounting pipeline.

JSON/Base64 must remain limited to metadata/small previews rather than becoming the science data transport.

## 8. WebSocket event reliability — P0

✅ WebSocket event channel exists and carries state/operation updates.  
✅ GUI can re-fetch an aggregate state snapshot after reconnect.  
⏳ Stable `eventStreamId`.  
⏳ Monotonic sequence number.  
⏳ Reconnect with `lastSequence`.  
⏳ Replay window.  
⏳ Explicit snapshot/replay recovery contract.

## 9. Error model — P0

✅ Operations carry machine-readable problem objects with codes/messages.  
🟡 HTTP API still uses the project success/error envelope in many paths.  
⏳ Complete RFC 9457 Problem Details responses for HTTP errors.  
⏳ Stable normative OAL error-code registry.

## 10. Conformance and simulators — P0

✅ ABI-v2 simulated camera/mount/focuser driver.  
✅ Structural regression checks for architecture, operations, async exposure, native drivers, QHY header isolation, ZWO, dual-camera/optics, Stellarium and cross-platform build structure.  
🟡 These are project regression checks, not yet a complete public OAL conformance suite.  
⏳ Black-box conformance tests for cancellation, reconnect, event replay, frame transfer, error model, idempotency and safety.  
⏳ Certification profile/versioning for third-party drivers.

## 11. Native hardware drivers

| Driver | Current status | Important boundary |
|---|---|---|
| `oal.qhy` | ✅ basic HIL / 🟡 qualification | QHY5III462C discovery, native live stream, FITS capture and SER writing have real-HIL evidence; histogram/metadata/reconnect regression remains |
| `oal.canon` | 🟡 implemented, HIL pending | EDSDK on Windows; libgphoto2/PTP on Linux |
| `oal.zwo.asi` | ✅ implemented / 🟡 HIL pending | ASI SDK path and multi-camera enumeration are implemented; no real ZWO camera qualification yet |
| `oal.zwo.eaf` | ✅ implemented / 🟡 HIL pending | EAF SDK path with move/halt/status/temp/capabilities is implemented; no real ZWO EAF qualification yet |
| `oal.gemini` | 🟡 protocol path implemented, HIL pending | Direct serial/MyFocuserPro2-compatible path; target firmware must be qualified |
| `oal.skywatcher` | 🟡 SynScan path implemented, HIL pending | GOTO/status/tracking/sync/abort/pulse guide; normative park unavailable in this profile |
| Sky-Watcher motor-controller direct axis mode | 🧪 experimental codec foundation | Needs calibrated coordinate/alignment model before production use |
| `oal.simulated` | ✅ implemented | Reference ABI-v2 simulator |

## 12. Compatibility adapters

✅ INDI compatibility client can be enabled independently.  
✅ ASCOM Alpaca compatibility path.  
✅ Minimal LX200 mount compatibility path.  
✅ Native OAL devices and compatibility devices share the same upper observatory model.  
⏳ Broader compatibility qualification matrix.

## 13. Optical trains and multi-camera configuration

✅ Independent `main` and `guide` camera roles.  
✅ Independent `camera` and `camera.guide` resource locks.  
✅ Main optical train stores aperture/primary diameter, effective focal length, optical design, optional obstruction, camera pixel/sensor data.  
✅ Guide train stores guide aperture/focal length and guide-camera sampling/sensor data.  
✅ Derived f-ratio and image scale.  
🟡 Automatic cross-checking of plate-solved scale vs configured optical profile is not yet a complete validation policy.

## 14. Solver and pointing

✅ Internal catalog-pattern/neural solver scaffolding remains available.  
✅ ASTAP adapter/path exists in the project.  
🟡 Real-sky ASTAP qualification is still required.  
🟡 Closed-loop GOTO/recenter groundwork exists but needs full real-hardware end-to-end qualification.  
⏳ Production astrometry.net adapter.  
⏳ Complete target resolver/ephemeris service with robust named DSO and topocentric Solar-System targets as a normative service.

## 15. Autofocus and polar alignment

✅ Autofocus operation and focus metrics exist.  
🟡 Real focuser/camera backlash, repeatability and exposure tuning are HIL pending.  
✅ Polar-axis estimation/math and sample APIs exist.  
🟡 Full automatic polar-alignment wizard/orchestration and live Alt/Az correction loop are not yet production-qualified.  
⏳ Geometric Bahtinov solver.

## 16. Guiding — P1

✅ Guide camera role and mount pulse-guide primitives exist.  
✅ Basic guiding state/API exists.  
⏳ Production guide-star selection/centroid loop.  
⏳ Calibration and axis mapping.  
⏳ RMS telemetry/quality model.  
⏳ Dither + settle.  
⏳ Backlash handling.  
⏳ Star-loss/reacquisition and meridian-flip recovery.

## 17. Scheduler / durable execution — P1 → OAL 1.0

✅ v0.2.10.47 introduces `ObservationPlan` / `ObservationBlock` / acquisition-step semantics while retaining `SessionTarget` as a compatibility wrapper.
🟡 A first supervised **DSO FITS/RAW** executor now runs `slew -> adaptive solve/recenter -> autofocus -> capture` and supports per-N-frame recenter/autofocus policies. It is not durable across node restart yet.
⏳ Extend DSO blocks with real filter-wheel execution, guiding/dither, time/altitude constraints and durable checkpoints.
⏳ First-class **planetary/lunar SER** blocks with high-speed camera settings, ROI, repeated SER runs and autonomous planet acquisition.  
⏳ Allow solve/recenter and full autofocus after every frame or every SER run when requested by policy.  
⏳ Temperature-triggered autofocus and optional calibrated **in-exposure thermal focus compensation**, with every focus move recorded in science provenance.  
⏳ Planetary target-centering loop using movable camera ROI and/or mount pulse-guide/micro-slew corrections. ROI origin/history must be recorded so calibration frames can reproduce the same sensor region.  
⏳ Durable checkpoints, restart reconciliation/resume, meridian-flip orchestration, weather hold/recovery and solve/recenter/reguide recovery.  

The normative scheduler design is in [`SCHEDULER.md`](SCHEDULER.md). Autonomous/unattended scheduling depends on the OAL 1.0 security, safety, guiding, durable-operation and data-plane work described below.

## 18. Additional device profiles — P1 → OAL 1.0

✅ First-class inert API/UI placeholders exist for future observatory device categories.  
⏳ Real filter-wheel drivers/capability profile.  
⏳ Real rotator drivers/capability profile.  
⏳ Real dome/roll-off-roof drivers and interlock semantics.  
⏳ Weather and safety-monitor drivers/policy.  
⏳ Power/switch drivers.  
⏳ Cover/calibrator drivers.  
⏳ GPS/GNSS/time-source drivers.

## 19. Stellarium integration

✅ Stellarium Telescope Control TCP bridge.  
✅ Mount position reporting and GOTO mapping.  
🟡 Requires HIL/interoperability qualification against the user's installed Stellarium and real mount.  
⏳ Dedicated Stellarium OAL plugin for camera/focus/session controls; the standard telescope protocol itself remains mount-centric.

## 20. Build and deployment specification

✅ Source tree supports Windows x64, Linux x86_64 and Linux ARM64/RPi.  
✅ CMake presets use schema v2 for CMake 3.20+ compatibility.  
✅ Windows official toolchain is MSVC 2022 x64 + Ninja.  
✅ `CMakeUserPresets.json` is local/ignored and stores vendor SDK paths.  
✅ Windows packaging helper runs install + `windeployqt` + runtime DLL staging.  
✅ Linux packaging helper installs into a staging tree and intentionally does not redistribute vendor `.so` files automatically.  
✅ Build environment preflight scripts are provided for Windows and Linux.  
🟡 Full vendor-SDK build matrix is still being qualified on physical Windows/Linux hosts.

## 20.1 Planned OAL 1.0 production/unattended profile

The following requirements are **specified/planned, not yet complete**, and are part of the path from supervised beta operation to a production unattended OAL 1.0 observatory:

- TLS, authentication, roles/scopes and audit provenance;
- HTTP idempotency and durable operation recovery;
- replayable/recoverable WebSocket event streams;
- observatory-wide emergency stop;
- weather, roof/dome and power safety interlocks with priority over science plans;
- production guiding, dither/settle, star-loss recovery and post-meridian-flip recovery;
- durable mixed DSO/planetary scheduler with restart resume;
- automatic meridian flips with solve/recenter/reguide/refocus recovery;
- durable science store, checksums, provenance and resumable downloads;
- driver crash isolation and public conformance/certification suite.

These items may be developed incrementally, but the product must continue to label unattended operation as unqualified until the relevant safety/recovery gates pass HIL.

## 21. Release acceptance boundary after v0.2.10.47

The release is considered **source/build ready for HIL** when:

1. presets parse with CMake 3.20+;
2. Windows configuration selects MSVC, never MinGW/Strawberry, when using Windows presets;
3. QHY SDK headers cannot shadow MSVC CRT/STL headers;
4. native drivers link against matching host-architecture vendor SDKs;
5. `oal-hardware-probe` enumerates intended hardware;
6. each device passes the supervised HIL gates in `VALIDATION.md`.

It is **not** yet considered production-ready for unattended remote observatory operation until the outstanding P0 safety/security/event/error/idempotency/data-plane/conformance items are closed.
