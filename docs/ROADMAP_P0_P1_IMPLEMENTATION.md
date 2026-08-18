# OpenAstroLink — план реалізації P0/P1 після v0.2

Статус документа: implementation plan, 17 August 2026.

## 0. Загальний принцип

P0 — це не набір незалежних endpoint-ів. Це фундамент, від якого залежать усі device profiles і
high-level workflows. Тому реалізація йде **вертикальними інкрементами**: schema → core → REST/WS →
adapter → simulator → conformance test → GUI/client migration.

Conformance suite починається одразу, а не в кінці: кожен P0 інкремент додає тести до тієї самої
матриці. GeminiAstro EAF використовується як перший реальний focuser profile для перевірки того,
що OAL не робить припущень про capabilities конкретного hardware.

---

## P0.0 — GeminiAstro EAF compatibility profile

### Реалізовано в цьому інкременті

- backend `gemini-eaf`;
- ASCOM/Alpaca transport;
- INDI transport при `OAS_ENABLE_INDI=ON`;
- явні transport prefixes `alpaca:` і `indi:`;
- окрема device identity у OpenAstroSuite;
- hardware validation checklist;
- direct USB/serial **не заявляється**, доки протокол поточної firmware не підтверджений.

### Чому це корисний pilot

Фокусер має достатньо простий API, але вже демонструє всі майбутні проблеми OAL: optional
absolute/relative movement, halt, limits, moving state, temperature, transport differences,
resource locking під час autofocus і cancellation довгого move.

---

## P0.1 — Capabilities, identity, discovery і нормативні schemas

### Core

Додати окремі типи:

- `DeviceIdentity`;
- `DriverIdentity`;
- `TransportInfo`;
- `DeviceCapabilities` + typed per-device profiles;
- `Limit/Range/Precision` structures;
- `Quantity`, time і coordinate schemas.

`IDevice` не повинен обростати десятками `supportsX()` методів. Краще один typed capability
document із common header та device-specific payload.

### API

- `GET /.well-known/openastrolink`;
- `GET /api/v1/devices`;
- `GET /api/v1/devices/{id}`;
- `GET /api/v1/devices/{id}/capabilities`;
- version/profile negotiation.

### Focuser capability minimum

- `absoluteMove.supported`;
- `relativeMove.supported`;
- `halt.supported`;
- `position.min/max/step/precision`;
- `movingTelemetry.supported`;
- `temperature.supported` + precision;
- optional backlash / reverse / temperature compensation / calibration;
- transport type and backend identity.

### Definition of Done

- OpenAPI + JSON Schema;
- simulated mount/camera/focuser capabilities;
- Gemini EAF capabilities derived from actual transport, а не hard-coded як універсальні;
- клієнт/GUI не викликає park, pulse guide, pier side, temperature тощо без capability gate;
- schema validation tests у CI.

---

## P0.2 — Єдина RFC 9457-compatible error model

Error model варто реалізувати **до масової міграції endpoint-ів на operations**, щоб усі нові
API одразу мали однакову семантику помилок.

### `ProblemDetails`

Мінімум:

- `type`;
- `title`;
- `status`;
- `detail`;
- `instance`;
- `oalCode`;
- `retryable`;
- `deviceId` / `operationId` за наявності;
- optional structured `causes`.

Визначити registry кодів: capability-not-supported, device-busy, safety-interlock,
invalid-state, timeout, transport-lost, cancelled, checksum-mismatch, stale-sequence тощо.

### Definition of Done

Усі нові P0 endpoints повертають тільки цю модель; старі довільні strings позначаються deprecated.

---

## P0.3 — Async operation resources

### Core

`OperationManager` + persistent-enough in-memory baseline:

```text
queued → running → succeeded
               ↘ failed
               ↘ cancelled
```

Поля: id, kind, owner/client, target resources, created/started/updated timestamps, progress,
phase, timeout/deadline, cancelSupported, result, problem, lock set.

### Перший vertical slice

1. `focuser.move`;
2. `autofocus.run`;
3. `mount.slew`;
4. `camera.exposure`;
5. solve;
6. park;
7. session execution.

Починаємо з focuser/autofocus, бо Gemini EAF дає реальний hardware pilot і одразу перевіряє
camera+focuser locking.

### API

- command endpoint повертає `202 Accepted` + `Location`;
- `GET /operations/{id}`;
- `POST /operations/{id}/cancel`;
- optional list/filter endpoints;
- result доступний після success, Problem Details після failure.

### Definition of Done

HTTP thread не блокується на slew/exposure/AF/solve/park/session; cancel і timeout мають тести.

---

## P0.4 — Idempotency і ResourceLockManager

### Idempotency

Для mutating commands підтримати `Idempotency-Key`:

- key scope = authenticated client + method + canonical resource;
- зберігати request fingerprint;
- same key + same request → повернути ту саму operation;
- same key + different payload → conflict Problem Details;
- TTL/retention policy задокументувати.

### Locks

Canonical resource ids і deterministic lock ordering, щоб уникнути deadlock.

Обов'язкові lock sets:

- autofocus → camera + focuser;
- exposure → camera;
- focuser move → focuser;
- slew/park → mount;
- guiding calibration → mount + guider/camera залежно від profile;
- meridian flip → mount + guider + camera;
- roof motion → roof + safety policy; за політикою також mount.

### Definition of Done

Retry після network failure не створює другий move/exposure/slew; конкурентний AF не може
одночасно використовувати ту саму camera/focuser pair.

---

## P0.5 — Security та safety baseline

### Security

- TLS поза loopback;
- authentication abstraction;
- roles/scopes;
- OAuth/OIDC deployment profile з актуальними OAuth security practices;
- short-lived tokens для remote deployments;
- audit log для небезпечних команд;
- secrets redaction.

### Safety

Окремий `SafetyPolicyEngine`, а не `if(weather)` у scheduler:

- emergency stop;
- weather safe/unsafe/unknown;
- roof/dome state;
- power dependencies;
- hardware/driver limits;
- policy priority вище session workflow;
- machine-readable deny reason.

### Definition of Done

Неможливо обійти safety через інший REST endpoint або high-level workflow. Emergency stop
працює локально без cloud dependency.

---

## P0.6 — FITS/RAW data plane

### Control plane

Exposure result повертає metadata/frame resource, не science pixels у Base64 JSON.

### Перший production profile

`https-download`:

- immutable frame resource;
- media type;
- byte length;
- SHA-256 / Content-Digest;
- HTTP Range;
- resumable download;
- capture provenance;
- retention/lifecycle.

### Наступні profiles

- local shared-memory/zero-copy;
- object storage;
- preview JPEG/PNG;
- planetary/live stream.

Preview може лишатися WS/JSON-friendly, science frame — ні.

### Definition of Done

50+ MB FITS переживає interrupted download без повторної exposure і без повторного завантаження
всього файла.

---

## P0.7 — Надійний event stream

Кожен stream:

- `streamId`;
- monotonically increasing `sequence`;
- event timestamp;
- subject/type/payload;
- bounded replay buffer або durable tail;
- reconnect із `lastSequence`;
- snapshot endpoint;
- explicit `replayUnavailable` path;
- duplicate-tolerant clients.

Safety events мають окрему гарантію пріоритету та не можуть тихо губитися через telemetry flood.

### Definition of Done

Тест: розірвати WebSocket під час autofocus/exposure, згенерувати події, reconnect із
`lastSequence`, відновити state без пропусків або отримати контрольований snapshot-required flow.

---

## P0.8 — Conformance suite, simulators і fault injection

Цей milestone **завершує** P0, але test harness росте з P0.1.

Матриця:

- schema/examples;
- identity/capabilities;
- mount/camera/focuser;
- Gemini EAF profile contract;
- async lifecycle;
- cancellation/timeouts;
- idempotency;
- lock conflicts;
- Problem Details;
- WS reconnect/replay/snapshot;
- frame checksum/range/resume;
- auth/scopes;
- safety deny/emergency stop;
- fault injection: driver crash, network loss, stale state, partial frame, clock jump.

Рівні результату:

1. simulator-conformant;
2. adapter-conformant;
3. hardware-validated;
4. safety-reviewed.

Жоден backend не називається production-ready лише тому, що він компілюється.

---

# P1 — після стабілізації P0 foundation

## P1.1 — Повні device profiles

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

Gemini EAF переходить від compatibility profile до повного typed focuser capability mapping;
native serial transport додається лише за наявності підтвердженого протоколу й hardware tests.

## P1.2 — Production solver adapters

Спочатку adapters до ASTAP і astrometry.net з уніфікованим WCS/result/provenance contract.
Власний indexed blind solver розвивати окремо, не блокуючи production solve path.

## P1.3 — Durable scheduler/session engine

- persistent checkpoints;
- resume після restart;
- meridian flip;
- dither;
- refocus triggers (time/temperature/HFR/filter);
- weather interruption;
- safe shutdown;
- recovery/recenter/reguide;
- idempotent child operations.

## P1.4 — Guiding completion

- calibration state machine;
- RA/DEC response model;
- dither settle;
- RMS telemetry;
- backlash handling;
- star-loss detection/reacquisition;
- recovery після flip/reconnect.

## P1.5 — Geometric Bahtinov solver

Замість лише high-frequency metric:

- detect three diffraction spike families;
- estimate center-spike offset;
- convert to signed focus error;
- confidence/uncertainty;
- robust multi-frame estimate;
- synthetic + real mask dataset.

## P1.6 — Sandboxed driver host і SDK

- out-of-process driver host;
- crash isolation/restart;
- permissions manifest;
- ABI/protocol negotiation;
- C++ SDK;
- Python SDK;
- Rust SDK;
- generated examples/conformance harness.

---

# Рекомендована послідовність pull requests / releases

1. `v0.2.1` — Gemini EAF compatibility profile + docs.
2. `v0.2.2` — Raspberry Pi headless `openastrolink-node`, split core/GUI, local-or-remote thin GUI control, persisted device bindings.
3. `v0.2.3 First Light` — mount abort/limits, QHY HIL, Gemini HIL, ASTAP adapter and first-light workflow gates.
4. `v0.3-alpha1` — identity/capabilities/discovery + Problem Details + test harness.
5. `v0.3-alpha2` — operations + cancel + idempotency + locks; focuser/AF first.
6. `v0.3-alpha3` — security/safety baseline.
7. `v0.3-alpha4` — FITS/RAW data plane.
8. `v0.3-alpha5` — sequenced/replayable events.
9. `v0.3-beta1` — full P0 conformance/fault-injection matrix, migration cleanup.
10. `v0.4` line — P1 device/services/automation work.

Release numbers are working labels; protocol compatibility, not calendar timing, determines promotion.
