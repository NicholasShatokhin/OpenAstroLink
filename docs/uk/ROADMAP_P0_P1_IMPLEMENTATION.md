# OpenAstroLink — план реалізації P0/P1 після v0.2

Українське дзеркало. Канонічна версія: `../ROADMAP_P0_P1_IMPLEMENTATION.md`. Оновлено **21 серпня 2026**.

## 0. Архітектурний принцип

OpenAstroLink розвивається як новий **native device stack**, а не як оболонка над INDI/ASCOM.

```text
Native OAL drivers        ← reference architecture
INDI / Alpaca / LX200     ← compatibility / migration
vendor SDK                ← дозволений low-level hardware access під native OAL driver
```

Нові capabilities спочатку проектуються в найкращій OAL-моделі. Compatibility adapters відображають лише те, що реально підтримує legacy API, і не обмежують native OAL semantics.

P0 реалізується вертикальними інкрементами: schema → core → driver boundary → REST/WS → simulator/reference driver → conformance → GUI/client migration. Driver не вважається production-ready лише тому, що він компілюється.

## P0.D — Native OAL driver foundation

ABI v2 у `include/oal/driver_api.h` має manifest/ABI negotiation, lifecycle, device enumeration, self-described capabilities, health, invoke context, cancellation, push events і host frame publication без Base64 pixels.

Node сканує `*.manifest.json`, завантажує ABI-v2 libraries і реєструє devices. Native key:

```text
native:<driverId>/<deviceId>
```

Поточні native hardware paths:

```text
QHY camera      → QHYCCD SDK         → oal.qhy        → ABI v2
Canon EOS       → USB/PTP/libgphoto2 → oal.canon      → ABI v2
Gemini EAF      → USB serial         → oal.gemini     → ABI v2
Sky-Watcher     → SynScan serial     → oal.skywatcher → ABI v2
ZWO ASI camera  → ASI SDK            → oal.zwo.asi    → ABI v2
ZWO EAF         → EAF SDK            → oal.zwo.eaf    → ABI v2
```

INDI для цих paths не потрібен, але лишається легким optional compatibility layer для іншого обладнання.

`oal.qhy`: exact ID, exposure, abort, ROI/binning, gain/offset, 16-bit where supported, frame publication; planetary streaming/SER pending.

`oal.canon`: USB/PTP discovery, shutter/ISO, Bulb, cooperative Bulb cancel, RAW/JPEG file spool і preview.

`oal.gemini`: direct MyFocuserPro2-compatible position/move/status/temperature. Неперевірений HALT не заявляється.

`oal.skywatcher`: documented SynScan status/GOTO/abort/sync/tracking/alignment/pier-side/pulse guide. Direct motor-controller поки experimental codec до завершення calibrated coordinate/alignment layer.

`oal.zwo.asi`: multi-camera enumeration, sensor/capability discovery, exposure, ROI/binning, gain/offset, cancel, native frame publication. SDK video capture є, production OAL ring-buffer/streaming operation ще pending.

`oal.zwo.eaf`: enumeration, absolute/relative move, halt, position/moving, temperature, max step, reverse/backlash.

Manifest із `out-of-process` не буде тихо завантажено in-process; sandboxed driver host є раннім P0/P1 завданням.

## P0.0 — Compatibility pilots

INDI, ASCOM Alpaca і LX200 лишаються interoperability/migration paths:

```text
rpi4-native-release       → native hardware, INDI OFF
rpi4-observatory-release  → native hardware + INDI ON
```

## P0.1 — Capabilities, identity, discovery, schemas

Потрібні `DeviceIdentity`, `DriverIdentity`, `TransportInfo`, typed `DeviceCapabilities`, limits/ranges/precision, quantity/time/coordinate schemas, `/.well-known/openastrolink`, canonical `/devices/{id}/capabilities` і version/profile negotiation.

Definition of Done: OpenAPI + JSON Schema; native і compatibility devices використовують ту саму capability model; GUI не припускає park/pulse-guide/pier-side/temperature тощо; CI schema validation.

## P0.2 — RFC 9457 Problem Details

Єдина machine-readable error model: `type`, `title`, `status`, `detail`, `instance`, `oalCode`, `retryable`, device/operation IDs та structured causes. Registry включатиме `capability-not-supported`, `device-busy`, `safety-interlock`, `invalid-state`, `timeout`, `transport-lost`, `cancelled`, `checksum-mismatch`, `stale-sequence`, `driver-crashed`.

## P0.3 — Async operations

Уже є `autofocus.run`, `mount.slew`, `camera.exposure`, `camera.guide.exposure`.

Залишилось перевести long focuser move, solve, park, session і meridian flip orchestration.

```text
queued → running → succeeded
               ↘ failed
               ↘ cancelled
```

HTTP thread не повинен чекати довгу physical operation.

## P0.4 — Idempotency + resource locks

`Idempotency-Key`: same key + same request → same operation; same key + different request → conflict; TTL documented.

Canonical locks:

```text
autofocus       → camera + focuser
main exposure    → camera
guide exposure   → camera.guide
focuser move     → focuser
slew/park        → mount
guiding calib.   → mount + camera.guide / guider
meridian flip    → mount + guider + camera
roof motion      → roof + safety dependencies
```

Dual-camera v0.2.9 навмисно розділяє `camera` і `camera.guide`.

## P0.5 — Security + safety

TLS поза loopback, authentication, roles/scopes, OAuth/OIDC або mTLS profiles, short-lived credentials, audit, secret redaction.

Окремий `SafetyPolicyEngine`: emergency stop, weather safe/unsafe/unknown, roof/dome, power dependencies, horizon/mechanical limits, hardware interlocks, policy precedence over workflows.

## P0.6 — Science data plane

Native ABI уже прибрав Base64 на plug-in boundary. Наступний network/storage profile:

```text
immutable frame resource
→ FITS/RAW
→ byte length + digest
→ HTTP Range/resume
→ provenance
```

Потім shared-memory/ring buffers, object storage, planetary live stream, SER, independent preview. Для ZWO/QHY high-rate SDK video має перейти в standardized OAL stream/ring-buffer contract.

## P0.7 — Reliable event stream

`streamId`, monotonic `sequence`, reconnect `lastSequence`, replay buffer, snapshot fallback, duplicate tolerance, priority safety events. Driver events живлять цей stream, але драйвери не реалізують WebSocket самі.

## P0.8 — Conformance/fault injection

Перевіряються manifests/ABI, discovery/capabilities, simulator, native QHY/Canon/Gemini/Sky-Watcher/ZWO contracts, compatibility adapters, operation lifecycle/cancel/timeouts, dual-camera isolation, idempotency/locks, Problem Details, event replay, frame integrity/resume, auth/safety, crash/network loss/stale state/partial frame/clock jump.

Рівні: simulator-conformant → protocol/driver-conformant → hardware-validated → safety-reviewed.

## P0/P1.D — Sandboxed out-of-process driver host

Target:

```text
openastrolink-node
   ├─ oal-driver-host oal.qhy
   ├─ oal-driver-host oal.zwo.asi
   ├─ oal-driver-host oal.zwo.eaf
   ├─ oal-driver-host oal.gemini
   └─ oal-driver-host oal.skywatcher
```

Потрібні process crash isolation/restart, permissions manifest enforcement, IPC negotiation, cancellation/events/frame handles, restricted hardware/filesystem access, `health=driver-crashed` і recovery telemetry.

## P1.1 — Complete device profiles

Mount, camera, focuser, filter wheel, rotator, dome/roof, weather/safety, power/switch, cover/calibrator, GPS/time.

Node також зберігає main і guide optical trains: aperture, effective focal length, camera pixel size/sensor geometry для solver hints, image-scale validation, guiding і workflow validation.

## P1.2 — Production solver/target services

ASTAP уже є. Далі: astrometry.net, async/cancellable solve, WCS/provenance contract, target resolver/planet ephemerides, closed-loop GOTO/recenter, automatic polar alignment і session integration.

## P1.3 — Durable scheduler/session

Persistent checkpoints/resume, meridian flip, dither, refocus triggers, weather interruption, safe shutdown, recenter/reguide recovery, idempotent child operations.

## P1.4 — Guiding completion

Dual-camera foundation уже дозволяє dedicated guide camera. Ще потрібні guide-camera subframes/stream, star selection/centroid tracking, calibration, RA/DEC response, dither settle, RMS telemetry, backlash, lost-star recovery, flip/reconnect recovery.

## P1.5 — Geometric Bahtinov

Виявлення трьох spike families, signed center-spike offset, confidence/uncertainty, robust multi-frame estimate, synthetic + real validation dataset.

## P1.6 — External planetarium integrations

v0.2.9 додає Stellarium Telescope Control TCP bridge. Standard Stellarium telescope protocol обмежений mount position/GOTO; camera/focuser/autofocus/polar/session залишаються OAL-native. Далі: interoperability tests, optional sync if standardized, і за потреби окремий Stellarium OAL plug-in для повного observatory panel.

## Поточна послідовність релізів

1. `v0.2.1` — Gemini compatibility profile.
2. `v0.2.2` — headless node + local/remote GUI.
3. `v0.2.3` — operation manager/resource locks/slew/autofocus.
4. `v0.2.4` — async exposure.
5. `v0.2.5` — RPi/ASTAP/QHY/INDI first hardware path.
6. `v0.2.6` — Native OAL ABI v2 + registry + simulator + native QHY.
7. `v0.2.7` — Native Telescope Hardware Pack: Gemini + Sky-Watcher; INDI optional.
8. `v0.2.8` — Native Canon EOS.
9. **`v0.2.9` — native ZWO ASI/EAF, main+guide cameras, optical profiles, Stellarium bridge, English-canonical documentation.**
10. `v0.2.10` candidate — native QHY/ZWO planetary streaming + ring buffer/SER + science-frame persistence.
11. `v0.3-alpha` — normative capabilities/errors/idempotency/events/security/conformance.
12. `v0.4` — durable sessions, guiding completion, full device profiles/services.
