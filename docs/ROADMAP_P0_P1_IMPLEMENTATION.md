# OpenAstroLink roadmap — implementation status after v0.2.10.5

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

### Device profiles — ⏳

Filter wheel, rotator, dome/roof, weather/safety, power/switch, cover/calibrator, GPS/time.

### Solver adapters — 🟡

ASTAP path exists, HIL pending. Add production astrometry.net adapter; keep the own indexed blind solver as an independent research/implementation path.

### Durable scheduler — 🟡

Session scaffolding exists. Add checkpoints/resume, meridian flip, dithering, refocus triggers, weather interruption/recovery and solve/recenter/reguide recovery.

### Guiding — 🟡

Guide camera role and pulse-guide primitives exist. Add calibration, centroid loop, RMS telemetry, dither/settle, backlash handling, star-loss recovery and post-flip recovery.

### Bahtinov solver — ⏳

Implement geometric diffraction-spike solver rather than relying only on high-frequency focus metrics.

### Sandboxed driver host and SDKs — ⏳

Implement out-of-process driver host/crash isolation. Stabilize public C++ SDK first; add Python and Rust SDK bindings after the ABI/protocol is frozen enough.

## Immediate engineering sequence

1. **Build qualification:** clean Windows MSVC+Ninja and native Linux builds with real vendor SDKs.
2. **HIL qualification:** QHY/Canon/ZWO/Gemini/Sky-Watcher, native-only first, then native+INDI compatibility.
3. **Real-sky qualification:** ASTAP → autofocus → closed-loop GOTO → polar alignment.
4. **Imaging data plane:** durable FITS/RAW + QHY/ZWO planetary SER pipeline.
5. **Production guider.**
6. **Durable session engine.**
7. Close the remaining P0 protocol hardening items in parallel.

## Release gates

**Supervised first-light:** clean build + device HIL + ASTAP + autofocus.  
**Full intended imaging workflow:** add closed-loop GOTO, polar alignment, durable DSO storage and planetary SER.  
**Autonomous observatory:** add production guiding, durable scheduler, safety policy and recovery.  
**Public OAL protocol/driver ecosystem:** close all P0 protocol items and conformance suite.
