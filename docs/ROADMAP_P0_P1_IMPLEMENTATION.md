## v0.2.10.51 execution order

Cross-platform build qualification is no longer the primary blocker: Windows x64, Linux x86_64 and Raspberry Pi/Linux ARM64 node builds are confirmed. The nearest Beta work is now deliberately narrow:

1. **HIL autofocus** — repeatability, backlash, cancel/failure rollback and verification frame.
2. **HIL auto-exposure** — convergence/lock/reacquire across representative scenes.
3. **Scheduler HIL** — mixed DSO/planetary execution, cancellation and restart boundaries.
4. **Mosaic HIL** — tile geometry, solve/recenter and traversal.
5. **Polar Alignment HIL** — guided sample motion and safe-region behavior.

Smart Telescope UX, broader unattended-observatory automation and productized one-button workflows remain OAL 1.0 scope.

# OpenAstroLink roadmap — implementation status after v0.2.10.51

Legend: ✅ done; 🟡 partial/HIL pending; ⏳ not done.

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

v0.2.10.51 adds the offline left-side navigation map: bright stars/selected DSOs, horizon projection, pan/zoom/search, telescope/solved/FOV overlays and controller-backed mount actions. Real-mount UI smoke remains part of Beta qualification; full planetarium/Smart Telescope presentation remains later scope.


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

1. **HIL autofocus:** convergence, repeatability, backlash, rollback/cancel and final verification frame.
2. **HIL auto-exposure:** convergence/lock/reacquire on representative real scenes.
3. **Scheduler HIL:** mixed DSO/planetary blocks, cancellation and restart boundaries.
4. **Mosaic HIL:** tile geometry, solve/recenter and traversal.
5. **Polar Alignment HIL:** real-sky guided sampling and safe-region motion.
6. Continue production guiding, durable session/data-plane and remaining P0 hardening in parallel after the Beta workflow gates.
7. Keep Smart Telescope UX in the OAL 1.0 track.

## Release gates

**Supervised first-light:** clean build + device HIL + ASTAP + autofocus.  
**Full intended imaging workflow:** add closed-loop GOTO, polar alignment, durable DSO storage and planetary SER.  
**Autonomous observatory:** add production guiding, durable scheduler, safety policy and recovery.  
**Public OAL protocol/driver ecosystem:** close all P0 protocol items and conformance suite.

## OAL 1.0 autonomous-observatory target

Specified/planned but not yet complete: TLS/auth/roles/audit, idempotency, durable operations, replayable events, safety/weather/roof/power interlocks, emergency stop, production guiding, durable mixed DSO/planetary scheduler, automatic meridian flip recovery, durable science data/provenance, driver isolation and public conformance. These are explicit 1.0 roadmap items rather than claims about the current beta implementation.
