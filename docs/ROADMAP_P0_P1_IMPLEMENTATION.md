# OpenAstroLink — P0/P1 implementation roadmap after v0.2

Document status: implementation plan, updated **21 August 2026**.

## 0. Architectural principle

OpenAstroLink is being developed as a **new native device stack**, not as a wrapper around INDI or ASCOM.

```text
Native OAL drivers        ← reference architecture
INDI / Alpaca / LX200     ← compatibility / migration
vendor SDK                ← permitted low-level hardware access beneath a native OAL driver
```

New capabilities are designed in the best OAL model first. Compatibility adapters translate as much of a legacy API as can be represented truthfully; legacy semantics do not constrain the native OAL model.

P0 is implemented as vertical increments: schema → core → driver boundary → REST/WS → simulator/reference driver → conformance → GUI/client migration.

The conformance suite grows with each increment. No backend or driver is called production-ready merely because it compiles.

---

# P0.D — Native OAL driver foundation

This foundation was moved earlier after practical RPi/INDI work showed that a legacy backend must not become the reference architecture by accident.

## v0.2.6 — foundation implemented

### ABI v2

`include/oal/driver_api.h` defines a compiler-neutral C ABI v2 with:

- manifest/ABI negotiation;
- `start/stop` lifecycle;
- device enumeration;
- self-described capabilities;
- health;
- invoke context containing request/operation/deadline information;
- cancellation;
- push events;
- host frame publication without Base64 pixel payloads.

### Manifest-based registry

The node scans native drivers, validates `*.manifest.json`, loads ABI-v2 libraries, and registers their devices. `OAL_DRIVER_PATH` allows additional search directories.

Native device key:

```text
native:<driverId>/<deviceId>
```

The GUI presents native devices separately from compatibility/embedded backends.

### Reference simulator

`oal.simulated` implements camera + mount + focuser through the real ABI v2 boundary and acts as the reference/conformance driver.

### Native hardware family implemented so far

The current native path includes:

```text
QHY camera      → QHYCCD SDK        → oal.qhy          → ABI v2
Canon EOS       → USB/PTP/libgphoto2→ oal.canon        → ABI v2
Gemini EAF      → USB serial        → oal.gemini       → ABI v2
Sky-Watcher     → SynScan serial    → oal.skywatcher   → ABI v2
ZWO ASI camera  → ASI SDK           → oal.zwo.asi      → ABI v2
ZWO EAF         → EAF SDK           → oal.zwo.eaf      → ABI v2
```

INDI is therefore not required for these supported native paths. It remains deliberately easy to enable for other equipment.

Current QHY scope includes exact hardware-ID discovery, connect/disconnect, single exposure, abort, ROI/binning, gain/offset, requested 16-bit acquisition where supported, health/capabilities/events, and host-frame-v2 publication. Planetary streaming/SER is still pending.

Current Canon scope includes USB/PTP discovery, shutter/ISO, Bulb, cooperative Bulb cancellation, original RAW/JPEG file spooling, and preview publication.

Current Gemini scope includes direct MyFocuserPro2-compatible serial position/movement/status/temperature. An unverified hardware stop command is not advertised as a capability.

Current Sky-Watcher scope includes the documented SynScan serial path for RA/Dec status, GOTO, abort, sync, tracking, alignment/pier-side probing, and pulse guide. Direct motor-controller support currently remains a low-level experimental codec until the full calibrated coordinate/alignment layer is implemented.

Current ZWO ASI scope includes multi-camera enumeration, sensor/capability discovery, single exposure, ROI/binning, gain/offset, cancellation, and native frame publication. The SDK exposes high-rate video capture, but the production OAL ring-buffer/streaming operation is still pending.

Current ZWO EAF scope includes enumeration, absolute/relative motion, halt/cancellation, position/moving state, temperature when available, max step, reverse, and backlash capability reporting.

### Isolation rule

A manifest may request `out-of-process`, but the current in-process loader refuses such a driver rather than silently weakening the requested isolation. A sandboxed driver host is therefore an early P0/P1 task.

---

# P0.0 — Compatibility pilots

INDI, ASCOM Alpaca, and LX200 remain first-class interoperability/migration paths. Their purpose is broad equipment coverage while native OAL drivers are added, not to define OAL semantics.

The Raspberry Pi presets make this explicit:

```text
rpi4-native-release       → native hardware, INDI OFF
rpi4-observatory-release  → native hardware + INDI compatibility ON
```

---

# P0.1 — Capabilities, identity, discovery, and normative schemas

The ABI-v2 foundation already exposes manifest/device/capability discovery. The next step is to make the capability model normative across HTTP, all device classes, and compatibility adapters.

## Core/API

- `DeviceIdentity`, `DriverIdentity`, `TransportInfo`;
- typed `DeviceCapabilities`;
- `Limit/Range/Precision`;
- quantity/time/coordinate schemas;
- `GET /.well-known/openastrolink`;
- canonical `GET /devices/{id}/capabilities`;
- version/profile negotiation.

## Definition of Done

- OpenAPI + JSON Schema;
- native simulator and hardware drivers conform to the same schemas;
- compatibility adapters translate capabilities without inventing unsupported features;
- GUI never assumes park/pulse-guide/pier-side/temperature/etc.;
- CI schema validation.

---

# P0.2 — RFC 9457 Problem Details

Adopt one machine-readable error model:

- `type`, `title`, `status`, `detail`, `instance`;
- `oalCode`, `retryable`;
- `deviceId` / `operationId`;
- optional structured causes.

Registry examples: `capability-not-supported`, `device-busy`, `safety-interlock`, `invalid-state`, `timeout`, `transport-lost`, `cancelled`, `checksum-mismatch`, `stale-sequence`, `driver-crashed`.

---

# P0.3 — Async operations

Vertical slices already exist for:

- `autofocus.run`;
- `mount.slew`;
- `camera.exposure`;
- `camera.guide.exposure`.

Still to migrate where appropriate:

- long focuser motion;
- solve;
- park;
- session execution;
- meridian-flip orchestration.

State machine:

```text
queued → running → succeeded
               ↘ failed
               ↘ cancelled
```

The HTTP request-processing thread must never wait for a long physical operation.

---

# P0.4 — Idempotency + locks

## Idempotency

`Idempotency-Key` for mutating create commands:

- same key + same request → same operation;
- same key + different request → conflict;
- retention/TTL documented.

## Canonical locks

- autofocus → `camera + focuser`;
- main exposure → `camera`;
- guide exposure → `camera.guide`;
- focuser move → `focuser`;
- slew/park → `mount`;
- guiding calibration → `mount + camera.guide` or the declared guider resource;
- meridian flip → `mount + guider + camera`;
- roof motion → `roof + safety dependencies`.

The dual-camera model introduced in v0.2.10 deliberately separates `camera` and `camera.guide` so a guide acquisition does not inherently block the main imager.

---

# P0.5 — Security + safety

## Security

- TLS outside loopback;
- authentication abstraction;
- roles/scopes;
- OAuth/OIDC or mTLS deployment profiles;
- short-lived remote credentials;
- audit log;
- secret redaction.

## Safety

Separate `SafetyPolicyEngine`:

- emergency stop independent of cloud connectivity;
- weather safe/unsafe/unknown;
- roof/dome state;
- power dependencies;
- horizon/mechanical limits;
- driver/hardware interlocks;
- policy precedence over workflows.

---

# P0.6 — Science data plane

ABI v2 already removes Base64 from the **native plug-in boundary**. The network/storage science data plane remains to implement.

First production profile:

```text
immutable frame resource
→ FITS/RAW file
→ byte length + digest
→ HTTP Range/resume
→ provenance
```

Next:

- local shared memory/ring buffers;
- object storage;
- planetary live stream;
- SER writer;
- preview independent from science bytes.

For ZWO and QHY, this is also where high-rate planetary acquisition moves from SDK video APIs into a standardized OAL stream/ring-buffer contract.

---

# P0.7 — Reliable event stream

- `streamId`;
- monotonic `sequence`;
- reconnect with `lastSequence`;
- replay buffer;
- snapshot fallback;
- duplicate-tolerant clients;
- priority delivery for safety events.

Driver ABI push events feed this stream; drivers themselves do not implement WebSocket transport.

---

# P0.8 — Conformance and fault injection

Test matrix:

- manifests/ABI negotiation;
- discovery/capabilities;
- native simulator;
- QHY/Canon/Gemini/Sky-Watcher/ZWO native contracts;
- compatibility adapters;
- operation lifecycle/cancel/timeouts;
- dual-camera role isolation;
- idempotency/locks;
- Problem Details;
- event replay;
- frame integrity/resume;
- auth/scopes/safety;
- driver crash/network loss/stale state/partial frame/clock jump.

Levels:

1. simulator-conformant;
2. protocol/driver-conformant;
3. hardware-validated;
4. safety-reviewed.

---

# P0/P1.D — Sandboxed out-of-process driver host

This task was moved forward from late P2 because a native OAL driver ecosystem must safely host third-party/vendor code.

Target:

```text
openastrolink-node
   ├─ oal-driver-host oal.qhy
   ├─ oal-driver-host oal.zwo.asi
   ├─ oal-driver-host oal.zwo.eaf
   ├─ oal-driver-host oal.gemini
   └─ oal-driver-host oal.skywatcher
```

Requirements:

- process crash isolation/restart;
- permission-manifest enforcement;
- IPC ABI/protocol negotiation;
- cancellation/events/frame handles;
- no direct access outside declared USB/serial/network/filesystem permissions;
- `health=driver-crashed` and recovery telemetry.

Trusted/reference in-process mode remains useful for development and low-latency paths, but third-party default should become out-of-process.

---

# P1.1 — Complete device profiles

- mount;
- camera;
- focuser;
- filter wheel;
- rotator;
- dome/roof;
- weather/safety;
- power/switch;
- cover/calibrator;
- GPS/time.

Native drivers implement richer profiles directly; compatibility adapters expose partial profiles based on actual legacy capabilities.

The node now also stores explicit **main and guide optical trains**. Main and guide aperture, effective focal length, camera pixel size, and sensor geometry become inputs to solver hints, image-scale validation, guiding, and future workflow validation.

---

# P1.2 — Production solver and target services

ASTAP adapter already exists. Next:

- astrometry.net adapter;
- async/cancellable solve operation;
- WCS/result/provenance contract;
- target resolver and planet ephemerides;
- closed-loop GOTO/recenter;
- integration into automatic polar alignment and sessions.

The experimental in-house indexed blind solver remains separate.

---

# P1.3 — Durable scheduler/session engine

- persistent checkpoints/resume;
- meridian flip;
- dither;
- refocus triggers;
- weather interruption;
- safe shutdown;
- recenter/reguide recovery;
- idempotent child operations.

---

# P1.4 — Guiding completion

The v0.2.9 dual-camera foundation allows a dedicated guide camera to coexist with the main imager. The production guider still needs:

- guide-camera subframe/stream pipeline;
- star selection and centroid tracking;
- calibration state machine;
- RA/Dec response model;
- dither settle;
- RMS telemetry;
- backlash handling;
- lost-star recovery;
- meridian-flip/reconnect recovery.

---

# P1.5 — Geometric Bahtinov solver

- detect three diffraction-spike families;
- signed center-spike offset;
- confidence/uncertainty;
- robust multi-frame estimate;
- synthetic + real validation dataset.

---

# P1.6 — External planetarium integrations

v0.2.9 adds a Stellarium Telescope Control TCP bridge. The standard Stellarium telescope protocol is intentionally limited to mount position and GOTO, so camera/focuser/autofocus/polar/session functions remain OAL-native.

Next integration steps:

- interoperability tests against current Stellarium builds;
- optional sync support if the chosen Stellarium client profile provides a standardized path;
- a dedicated Stellarium OAL plug-in only if we want the complete observatory control panel inside Stellarium;
- keep the generic external telescope bridge independent of native-vs-compatibility mount transport.

---

# Current release sequence

1. `v0.2.1` — Gemini compatibility profile.
2. `v0.2.2` — headless node + local/remote GUI.
3. `v0.2.3` — operation manager/resource locks/slew/autofocus.
4. `v0.2.4` — async exposure.
5. `v0.2.5` — RPi/ASTAP/QHY/INDI first-hardware path.
6. `v0.2.6` — Native OAL ABI v2 + registry + reference simulator + native QHY.
7. `v0.2.7` — Native Telescope Hardware Pack: native Gemini + native Sky-Watcher/SynScan; INDI stays easy and optional.
8. `v0.2.8` — Native Canon EOS, completing the initial native hardware set for the user's observatory.
9. **`v0.2.9` — native ZWO ASI/EAF, main+guide camera roles, main+guide optical profiles, Stellarium mount bridge, English-canonical documentation.**
10. `v0.2.10` candidate — native QHY/ZWO planetary streaming + ring buffer/SER + science-frame persistence.
11. `v0.3-alpha` line — normative capabilities/errors/idempotency/events/security/conformance hardening.
12. `v0.4` line — durable sessions, guiding completion, full device profiles/services.

Release labels are working labels; protocol compatibility and validation gates determine promotion.
