# OpenAstroLink — план реалізації P0/P1 після v0.2

Статус документа: implementation plan, **19 August 2026**.

## 0. Архітектурний принцип

OpenAstroLink розвивається як **новий native device stack**, а не як оболонка над INDI/ASCOM.

```text
Native OAL drivers        ← reference architecture
INDI / Alpaca / LX200     ← compatibility / migration
vendor SDK                ← allowed low-level hardware access under a native OAL driver
```

Нові можливості проектуються у найкращій OAL-моделі. Compatibility adapters відображають у неї стільки старого API, скільки можливо; вони не обмежують native OAL семантику.

P0 реалізується вертикальними інкрементами: schema → core → driver boundary → REST/WS → simulator/reference driver → conformance → GUI/client migration.

Conformance suite росте з кожним інкрементом. Жоден backend/driver не називається production-ready лише тому, що він компілюється.

---

# P0.D — Native OAL driver foundation

Цей foundation був піднятий у пріоритеті після практичних RPi/INDI тестів, щоб не закріпити legacy backend як основну архітектуру.

## v0.2.6 — реалізовано

### ABI v2

`include/oal/driver_api.h` тепер визначає compiler-neutral C ABI v2:

- manifest/ABI negotiation;
- lifecycle `start/stop`;
- device enumeration;
- self-described capabilities;
- health;
- invoke context з request/operation/deadline;
- cancellation;
- push events;
- host frame publication без Base64 pixels.

### Manifest-based registry

Node автоматично сканує native drivers, перевіряє `*.manifest.json`, завантажує ABI-v2 library та реєструє devices. `OAL_DRIVER_PATH` дозволяє додаткові каталоги.

Native device key:

```text
native:<driverId>/<deviceId>
```

GUI показує native devices окремо від compatibility/embedded backends.

### Reference simulator

`oal.simulated` реалізує camera + mount + focuser через справжній ABI v2 і використовується як reference/conformance driver.

### Native QHY

`oal.qhy` — перший hardware native driver:

```text
QHY camera → QHYCCD SDK → oal.qhy → ABI v2 → OAL core
```

INDI для QHY більше не є необхідним у reference path.

Поточний native QHY scope:

- exact hardware ID discovery;
- connect/disconnect;
- single exposure;
- abort;
- ROI/binning;
- gain/offset;
- 16-bit request where supported;
- health/capabilities/events;
- host-frame-v2 publication.

Planetary streaming/SER ще не заявляється (`supported:false`).

### Isolation rule

Manifest може вимагати `out-of-process`, але v0.2.6 **відмовляється** завантажувати такий driver in-process. Ми не будемо непомітно послаблювати requested isolation. Sandboxed driver host тепер є раннім P0/P1 завданням.

## Наступні native drivers

### Gemini EAF

Поточний INDI/Alpaca шлях — compatibility only. Native `oal.gemini` реалізується після підтвердження low-level protocol для фактичної firmware/USB bridge. Неперевірені serial commands не допускаються.

### Mount

Native mount driver додається для конкретного hardware/protocol після визначення фактичної моделі/transport. INDI/LX200/Alpaca залишаються compatibility fallback.

---

# P0.0 — Compatibility pilots

Gemini EAF compatibility profile уже існує через INDI/Alpaca. Його роль тепер чітко визначена: interoperability і hardware pilot, а не reference driver architecture.

---

# P0.1 — Capabilities, identity, discovery і нормативні schemas

ABI-v2 foundation вже має manifest/device/capability discovery. Наступний крок — зробити capability model нормативною для HTTP API та всіх device classes.

## Core/API

- `DeviceIdentity`, `DriverIdentity`, `TransportInfo`;
- typed `DeviceCapabilities`;
- `Limit/Range/Precision`;
- Quantity/time/coordinate schemas;
- `GET /.well-known/openastrolink`;
- canonical `GET /devices/{id}/capabilities`;
- version/profile negotiation.

## Definition of Done

- OpenAPI + JSON Schema;
- native simulator/QHY conform to same schemas;
- compatibility adapters translate capabilities without inventing unsupported features;
- GUI never assumes park/pulse-guide/pier-side/temperature/etc.;
- CI schema validation.

---

# P0.2 — RFC 9457 Problem Details

Єдина machine-readable error model:

- `type`, `title`, `status`, `detail`, `instance`;
- `oalCode`, `retryable`;
- `deviceId`/`operationId`;
- optional structured causes.

Registry: capability-not-supported, device-busy, safety-interlock, invalid-state, timeout, transport-lost, cancelled, checksum-mismatch, stale-sequence, driver-crashed тощо.

---

# P0.3 — Async operations

Уже є vertical slices:

- `autofocus.run`;
- `mount.slew`;
- `camera.exposure`.

Залишилося перевести:

- focuser long move where needed;
- solve;
- park;
- session execution;
- meridian flip orchestration.

State machine:

```text
queued → running → succeeded
               ↘ failed
               ↘ cancelled
```

HTTP thread не повинен чекати тривалу physical operation.

---

# P0.4 — Idempotency + locks

## Idempotency

`Idempotency-Key` для mutating create commands:

- same key + same request → same operation;
- same key + different request → conflict;
- retention/TTL documented.

## Canonical locks

- autofocus → camera + focuser;
- exposure → camera;
- focuser move → focuser;
- slew/park → mount;
- guiding calibration → mount + guider/camera as required;
- meridian flip → mount + guider + camera;
- roof motion → roof + safety dependencies.

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

- emergency stop independent of cloud;
- weather safe/unsafe/unknown;
- roof/dome state;
- power dependencies;
- horizon/mechanical limits;
- driver/hardware interlocks;
- policy precedence over workflows.

---

# P0.6 — Science data plane

ABI-v2 already removes Base64 from the **native plugin boundary**. The network/storage science data plane remains to implement.

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

---

# P0.7 — Reliable event stream

- `streamId`;
- monotonic `sequence`;
- reconnect `lastSequence`;
- replay buffer;
- snapshot fallback;
- duplicate-tolerant clients;
- priority delivery for safety events.

Driver ABI push events feed into this stream; drivers themselves do not implement WebSocket transport.

---

# P0.8 — Conformance/fault injection

Test matrix:

- manifests/ABI negotiation;
- discovery/capabilities;
- native simulator;
- native QHY contract;
- compatibility adapters;
- operation lifecycle/cancel/timeouts;
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

Це завдання перенесене вперед із пізнього P2, тому що native OAL driver ecosystem має бути безпечним для third-party/vendor code.

Target:

```text
openastrolink-node
   ├─ oal-driver-host oal.qhy
   ├─ oal-driver-host oal.gemini
   └─ oal-driver-host oal.mount...
```

Requirements:

- process crash isolation/restart;
- permissions manifest enforcement;
- IPC ABI/protocol negotiation;
- cancellation/events/frame handles;
- no direct access outside declared USB/serial/network/filesystem permissions;
- health=`driver-crashed` and recovery telemetry.

Trusted/reference in-process remains useful for development and very low-latency paths, but third-party default should become out-of-process.

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

---

# P1.2 — Production solver services

ASTAP adapter already exists. Next:

- astrometry.net adapter;
- async/cancellable solve operation;
- WCS/result/provenance contract;
- closed-loop GOTO/recenter;
- target resolver/planet ephemerides.

The experimental own indexed blind solver remains separate.

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

- calibration state machine;
- RA/DEC response model;
- dither settle;
- RMS telemetry;
- backlash handling;
- lost-star recovery;
- flip/reconnect recovery.

---

# P1.5 — Geometric Bahtinov solver

- detect three diffraction-spike families;
- signed center-spike offset;
- confidence/uncertainty;
- robust multi-frame estimate;
- synthetic + real validation dataset.

---

# Поточна послідовність релізів

1. `v0.2.1` — Gemini compatibility profile.
2. `v0.2.2` — headless node + local/remote GUI.
3. `v0.2.3` — operation manager/resource locks/slew/AF.
4. `v0.2.4` — async exposure.
5. `v0.2.5` — RPi/ASTAP/QHY/INDI first hardware path.
6. **`v0.2.6` — Native OAL ABI v2 + registry + reference simulator + native QHY.**
7. `v0.2.7` — native driver host groundwork + native Gemini/mount only where protocol is verified; async ASTAP/closed-loop GOTO can proceed in parallel.
8. `v0.2.8` — QHY native planetary streaming/SER + science-frame persistence/data-plane vertical slice.
9. `v0.3-alpha` line — normative capabilities/errors/idempotency/events/security/conformance hardening.
10. `v0.4` line — durable sessions, guiding completion, full device profiles/services.

Release labels are working labels; protocol compatibility and validation gates determine promotion.
