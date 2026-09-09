# OpenAstroLink roadmap — immediate execution after v0.2.10.58-buildfix9

**Snapshot:** 2026-09-09  
**Master checklist:** `CURRENT_CHECKLIST.md`

The previous `fresh Windows build` blocker is closed. The exact current Windows x64/MSVC build now configures and links successfully with WebRTC enabled, WebRTC runtime staging active, ZWO EAF using the correct DLL import library, and Canon EDSDK locked to the AMD64 `EDSDK_64` runtime pair.

The immediate program is now **runtime/HIL qualification**, not another broad architecture rewrite.

## v0.2.10.58-buildfix9 execution order

1. Runtime-regression the native plugin registry: `oal.canon` and `oal.zwo.eaf` must load with no Win32 error 193.
2. QHY high-rate HIL at short exposure: `Capture FPS=MAX`, `Preview=60`; record capture/record/preview FPS and drops.
3. 5–10 minute SER with `recordDropped=0`; verify AutoStakkert frame count against OAL telemetry/sidecar.
4. Preview 15/30/60/MAX while recording; GUI preview must not materially reduce recording throughput.
5. Confirm actual WebRTC DataChannel transport, then failure/fallback/reconnect behavior.
6. Run Main + Guide Dual Live with two physical cameras and per-role fallback.
7. Repeat 30–50 direct-Wi-Fi manual press/release/direction changes.
8. Repeat Scene AF and sparse-scene still auto-exposure HIL; then night HFR AF.
9. Scheduler DSO + ASTAP/recenter HIL.
10. Planetary SER/ROI executor HIL.
11. Mosaic 2×2 + footprint overlay HIL.
12. Polar Alignment real-sky HIL.
13. Discovery/hotplug regression.
14. Portable Windows package + clean-machine test.
15. Full supervised night qualification and deploy the **current** `site/` source to the already-live `openastro.link` DNS/HTTPS endpoint.

Do not reopen mount coordinate model v9 without new contradictory HIL evidence. Smart Telescope UX and broad unattended-observatory automation remain OAL 1.0 scope.

# OpenAstroLink roadmap — implementation status after v0.2.10.52

Legend: ✅ done; 🟡 partial/HIL pending; ⏳ not done.

- **Sky Map framing — ✅ implemented / HIL pending**: measured solved rectangle, predicted main/guide footprints, Scheduler-synchronized mosaic grid and optional Stellarium Remote Control export are in v0.2.10.53. Keep full planetarium/Smart Telescope presentation for 1.0.
## P0 — must be closed before OAL can be called a robust modern observatory protocol

### 1. Capabilities, identity and discovery — 🟡

Done: native registry, identity, per-device typed capability publication, native driver discovery.  
Remaining: freeze normative schemas/versioning across every device profile and compatibility adapter.

### 2. Asynchronous operations — 🟡

Done: operation manager, async slew, main/guide exposure, autofocus, progress/cancel/result.  
Remaining: migrate remaining long solve/session/polar/park-style workflows; durable operation recovery after node restart.

### 3. Idempotency and resource locking — 🟡

Done: lock manager, camera/focuser/mount locks, separate guide-camera lock, queueing.  
Remaining: `Idempotency-Key`, persistent replay-safe command records, complete cross-device lock graph (meridian flip, filter wheel, guider, roof, safety).

### 4. Security and safety interlocks — ⏳

Remaining: TLS, authentication, roles/scopes, audit log, observatory emergency stop, weather/roof/power policy engine and explicit safety priority over user workflows.

### 5. Separate science data plane — 🟡

Done: native driver frame publication, preview cache/endpoint, Canon original file path.  
Remaining: durable FITS/RAW/SER store, checksums, range/resumable download, shared memory/zero-copy local path, object storage, planetary ring buffer and drop accounting.

### 6. Reliable WebSocket events — ⏳

Existing channel is functional but not recovery-complete. Add stream ID, monotonic sequence, `lastSequence`, replay window and explicit snapshot fallback.

### 7. Unified error model — 🟡

Operation problems are structured. Complete HTTP API migration to RFC 9457 Problem Details and define a stable OAL error-code registry.

### 8. Conformance suite and simulators — 🟡

Simulator and many structural checks exist. Build the black-box public suite for driver/device/API/event/error/cancellation/reconnect/data-plane/safety conformance.

## P1 — after P0 core semantics stabilize
### Sky Map navigation — ✅ MVP

v0.2.10.51 introduced the offline left-side navigation map: bright stars/selected DSOs, horizon projection, pan/zoom/search, telescope/solved/FOV overlays and controller-backed mount actions. v0.2.10.52 adds arbitrary visible-sky point selection with the same Slew/Sync/Scheduler contract. Catalogue-object → Scheduler transfer is confirmed in the running GUI; free-point UI/HIL smoke remains. Full planetarium/Smart Telescope presentation remains later scope.


### Device profiles — ⏳

Filter wheel, rotator, dome/roof, weather/safety, power/switch, cover/calibrator, GPS/time.

### Solver adapters — 🟡

ASTAP path exists, HIL pending. Add production astrometry.net adapter; keep the own indexed blind solver as an independent research/implementation path.

### Durable mixed-mode scheduler — 🟡 supervised DSO executor / ⏳ durable + planetary

v0.2.10.46 implements the first `ObservationPlan`/`ObservationBlock` executor for DSO FITS/RAW: async slew, adaptive solve/recenter with tolerance and retry limit, autofocus, science capture, and per-N-frame recenter/autofocus. v0.2.10.47 adds planetary SER blocks, full-frame acquisition, hardware ROI tracking, ROI provenance and an optional calibrated slow mount recenter loop. Next stages are temperature-triggered/in-exposure focus compensation, plan constraints, durable checkpoints/restart resume, meridian flip, weather interruption/recovery and solve/recenter/reguide recovery.

### Guiding — 🟡

Guide camera role and pulse-guide primitives exist. Add calibration, centroid loop, RMS telemetry, dither/settle, backlash handling, star-loss recovery and post-flip recovery.

### Bahtinov solver — ⏳

Implement geometric diffraction-spike solver rather than relying only on high-frequency focus metrics.

### Sandboxed driver host and SDKs — ⏳

Implement out-of-process driver host/crash isolation. Stabilize public C++ SDK first; add Python and Rust SDK bindings after the ABI/protocol is frozen enough.

## Immediate engineering sequence

1. Fresh build of the current high-rate branch.
2. High-rate main-camera + SER zero-drop HIL.
3. Dual Live main+guide HIL.
4. Repeat direct-Wi-Fi manual-slew HIL.
5. Repeat scene autofocus and still auto-exposure HIL; then night star/HFR AF.
6. Scheduler HIL.
7. Mosaic HIL.
8. Polar Alignment HIL.
9. Full supervised night qualification.
10. Continue production guiding, durable session/data-plane and remaining P0 hardening after the Beta workflow gates.
11. Keep Smart Telescope UX in the OAL 1.0 track.

## Release gates

**Supervised first-light:** clean build + device HIL + ASTAP + autofocus.  
**Full intended imaging workflow:** add closed-loop GOTO, polar alignment, durable DSO storage and planetary SER.  
**Autonomous observatory:** add production guiding, durable scheduler, safety policy and recovery.  
**Public OAL protocol/driver ecosystem:** close all P0 protocol items and conformance suite.

## OAL 1.0 autonomous-observatory target

Specified/planned but not yet complete: TLS/auth/roles/audit, idempotency, durable operations, replayable events, safety/weather/roof/power interlocks, emergency stop, production guiding, durable mixed DSO/planetary scheduler, automatic meridian flip recovery, durable science data/provenance, driver isolation and public conformance. These are explicit 1.0 roadmap items rather than claims about the current beta implementation.

## Immediate Windows gate after buildfix8
Before the high-rate/QHY WebRTC throughput sequence, clear the two native vendor plugin loader failures: run the AMD64 runtime repair/diagnostic, require Canon EDSDK and ZWO EAF to load without `ERROR_BAD_EXE_FORMAT`, then continue with camera-max/Preview-60 and zero-record-drop SER HIL.
