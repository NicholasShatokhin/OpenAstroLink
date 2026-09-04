## v0.2.10.51 implementation boundary

Protocol/driver model лишається native-first, INDI — opt-in compatibility. Cross-platform build qualification охоплює Windows x64, Linux x86_64 та Raspberry Pi/Linux ARM64 node targets. Direct-MC mount coordinate model v9 HIL-qualified і незмінений; EQDrive transport більше не має прихованого тимчасового sky-GOTO qualification envelope. Operator safety належить Core/profile policy, raw-axis calls мають явний mechanical guard.

## Примітка реалізації v0.2.10.47

Scheduler тепер виконує і DSO FITS/RAW, і planetary SER blocks. Planetary block знаходить ціль на full frame, опційно виконує autofocus у planet mode, формує hardware ROI, пише finite SER, може рухати ROI без зміни розміру та веде frame-indexed provenance origin ROI. Slow mount recentering є опційним каліброваним loop і за замовчуванням вимкнений до HIL-кваліфікації. Це не змінює planned boundary OAL 1.0 для durable restart/recovery, weather/roof interlocks, meridian handling, authentication та process isolation.

# Специфікація OpenAstroLink і стан реалізації

**Канонічна мова:** англійська  
**Реліз:** OpenAstroSuite / OpenAstroLink v0.2.10.51
**Позначки:** ✅ реалізовано в поточному коді; 🟡 частково реалізовано / потрібен HIL; 🧪 експериментально; ⏳ визначено, але ще не реалізовано.

Цей документ є зрізом специфікації, прив'язаним до реалізації. Він чітко відділяє цільову архітектуру OAL від планів.

## 1. Архітектурні принципи

✅ Обладнанням володіє `openastrolink-node`, а не GUI. GUI може бути локальним або віддаленим. Закриття GUI не повинно зупиняти node.  
✅ Native OAL ABI-v2 drivers — основний шлях.  
✅ INDI, ASCOM Alpaca та LX200 — compatibility/migration layers.  
✅ Windows x64, Linux x86_64 та Linux ARM64/RPi мають бути first-class observatory hosts.  
🟡 Vendor SDK linkage/runtime installation ще проходять build/HIL-кваліфікацію.

## 2. Native Driver ABI v2

✅ Manifest і стабільний `driverId`.  
✅ Lifecycle start/stop.  
✅ Discovery/enumeration.  
✅ Identity та typed JSON capabilities.  
✅ Health.  
✅ Push-event callback foundation.  
✅ Cooperative cancellation.  
✅ Native frame publication.  
✅ Thread/concurrency model і loader-side serialization для normal calls.  
⏳ Sandboxed out-of-process driver host та crash recovery.  
⏳ Повний public conformance suite.

## 3. Capabilities, identity, discovery — P0

✅ Native registry/device enumeration.  
✅ Per-device capabilities endpoint.  
✅ Device identity з driver/device/type.  
✅ Device-specific capabilities у native camera/focuser/mount drivers.  
🟡 Capability vocabulary ще не заморожена як повністю normative schema для всіх майбутніх класів.  
⏳ Профілі filter wheel, rotator, dome/roof, weather, power/switch, cover/calibrator, GPS/time.

## 4. Асинхронні operations — P0

```text
queued → running → succeeded
                 ↘ failed
                 ↘ cancelled
```

✅ Operation resources.  
✅ Progress/phase/result/problem.  
✅ Cancellation.  
✅ Resource locks.  
✅ Async mount slew.  
✅ Async main/guide exposure.  
✅ Async autofocus.  
🟡 Частина solve/session/polar workflow ще synchronous або не повністю operation-backed.  
⏳ Durable operation state після restart node.

## 5. Resource locks та idempotency — P0

✅ Lock manager.  
✅ Autofocus: camera + focuser.  
✅ Slew: mount.  
✅ Окремі `camera` та `camera.guide`.  
✅ Конфліктні операції можуть чекати.  
⏳ HTTP `Idempotency-Key`.  
⏳ Persistent idempotency records.  
⏳ Повний lock graph для meridian flip/filter wheel/dome/safety.

## 6. Security та safety — P0

⏳ TLS.  
⏳ Authentication + roles/scopes.  
⏳ Audit log.  
🟡 Device-level abort/halt є там, де протокол відомий.  
⏳ Observatory-wide emergency stop.  
⏳ Weather/roof/power safety policy.  
⏳ Internet-facing hardening.

Поточне правило: trusted LAN/VPN; не відкривати 8080/8090 напряму в публічний Internet.

## 7. Camera data plane — P0

✅ Native ABI frame publication.  
✅ In-memory preview + PNG preview endpoint.  
✅ Canon original-file spool.  
🟡 QHY/ZWO capture path є, але production high-rate planetary pipeline ще потребує HIL і durable writer.  
⏳ FITS/RAW science store + checksums/provenance.  
⏳ Resumable/range download.  
⏳ Shared-memory/zero-copy local path.  
⏳ Object storage.  
⏳ Production SER ring-buffer/drop accounting.

## 8. Надійні WebSocket events — P0

✅ WebSocket channel та state/operation updates.  
✅ Snapshot refresh після reconnect.  
⏳ `eventStreamId`.  
⏳ Sequence number.  
⏳ `lastSequence` reconnect.  
⏳ Replay window.  
⏳ Formal snapshot/replay recovery contract.

## 9. Error model — P0

✅ Operation problem objects із code/message.  
🟡 HTTP ще часто використовує project envelope.  
⏳ Повний RFC 9457 Problem Details.  
⏳ Normative OAL error-code registry.

## 10. Conformance і simulator — P0

✅ ABI-v2 simulated camera/mount/focuser.  
✅ Structural regression checks.  
🟡 Це ще не повний public conformance suite.  
⏳ Black-box cancellation/reconnect/event replay/frame/error/idempotency/safety tests.  
⏳ Certification profile для third-party drivers.

## 11. Native hardware drivers

| Driver | Стан | Межа |
|---|---|---|
| `oal.qhy` | ✅ базовий HIL / 🟡 qualification | QHY5III462C discovery, native live stream, FITS capture і SER writing підтверджені на реальному залізі; ще потрібні regression/reconnect checks |
| `oal.canon` | 🟡 реалізовано, HIL pending | EDSDK Windows; libgphoto2/PTP Linux |
| `oal.zwo.asi` | ✅ реалізовано / 🟡 HIL pending | ASI SDK path і multi-camera enumeration реалізовані; реальної ZWO camera ще не кваліфіковано |
| `oal.zwo.eaf` | ✅ реалізовано / 🟡 HIL pending | EAF SDK path реалізований; реальний ZWO EAF ще не кваліфіковано |
| `oal.gemini` | 🟡 protocol path implemented, HIL pending | Direct serial/MyFocuserPro2-compatible |
| `oal.skywatcher` | 🟡 SynScan implemented, HIL pending | GOTO/status/tracking/sync/abort/pulse guide |
| Direct motor-controller Sky-Watcher | 🧪 codec foundation | Потрібна calibrated coordinate/alignment model |
| `oal.simulated` | ✅ | Reference ABI-v2 simulator |

## 12. Compatibility

✅ INDI client легко вмикається.  
✅ ASCOM Alpaca.  
✅ Minimal LX200.  
✅ Native і compatibility devices бачаться через одну верхню model.  
⏳ Широка compatibility qualification matrix.

## 13. Optical trains і дві камери

✅ `main` і `guide` camera roles.  
✅ `camera` та `camera.guide` locks.  
✅ Main optical train: aperture/primary diameter, effective focal length, design, obstruction, pixel/sensor data.  
✅ Guide optical train.  
✅ f-ratio та image scale.  
🟡 Автоматична звірка plate-solved scale з профілем ще не завершена.

## 14. Solver і pointing

✅ Internal solver scaffolding.  
✅ ASTAP adapter/path.  
🟡 Real-sky ASTAP HIL pending.  
🟡 Closed-loop GOTO/recenter groundwork потребує end-to-end HIL.  
⏳ Production astrometry.net adapter.  
⏳ Повний target resolver/ephemeris service.

## 15. Autofocus і polar alignment

✅ Autofocus operation/metrics.  
🟡 Backlash/repeatability/exposure tuning pending.  
✅ Polar-axis math/sample APIs.  
🟡 Full automatic wizard/live Alt-Az loop не production-qualified.  
⏳ Geometric Bahtinov solver.

## 16. Guiding — P1

✅ Guide camera role і pulse-guide primitives.  
✅ Basic guiding API/state.  
⏳ Production star tracking/calibration/RMS/dither/settle/backlash/reacquire/flip recovery.

## 17. Scheduler / durable execution — P1 → OAL 1.0

✅ У v0.2.10.47 введено `ObservationPlan` / `ObservationBlock` / acquisition-step semantics; `SessionTarget` лишився compatibility wrapper.
🟡 Перший supervised **DSO FITS/RAW** executor уже виконує `slew -> adaptive solve/recenter -> autofocus -> capture` і підтримує recenter/autofocus кожні N кадрів. Він ще не durable при restart node.
⏳ Розширити DSO blocks реальним filter-wheel execution, guiding/dither, time/altitude constraints і durable checkpoints.
⏳ Першокласні **planetary/lunar SER** blocks: high-speed settings, ROI, повторні SER та автономне наведення/центрування планети.  
⏳ Full autofocus після кожного кадру/SER або за часом/температурою/якістю.  
⏳ Опційна calibrated **in-exposure thermal focus compensation** з повним журналом рухів фокусера.  
⏳ Автоматичне утримання планети в ROI через переміщення ROI та/або pulse-guide/micro-slew mount; ROI origin/history зберігається для відтворення dark/flat region.  
⏳ Durable checkpoints/restart resume, meridian flip, weather hold/recovery, solve/recenter/reguide recovery.

Повна модель описана у [`SCHEDULER.md`](SCHEDULER.md).

## 18. Додаткові device profiles — P1 → OAL 1.0

✅ У API/UI вже є inert placeholders для майбутніх observatory device categories.  
⏳ Filter wheel.  
⏳ Rotator.  
⏳ Dome / roll-off roof та interlocks.  
⏳ Weather / safety monitor.  
⏳ Power / switch.  
⏳ Cover / calibrator.  
⏳ GPS/GNSS/time.

## 19. Stellarium

✅ Telescope Control TCP bridge.  
✅ Position + GOTO.  
🟡 HIL/interoperability pending.  
⏳ Окремий OAL plugin для повного observatory control із Stellarium.

## 20. Build/deployment

✅ Windows x64, Linux x86_64, Linux ARM64/RPi source paths.  
✅ Preset schema v2 → CMake 3.20+.  
✅ Windows official toolchain: MSVC 2022 x64 + Ninja.  
✅ `CMakeUserPresets.json` локальний/ignored.  
✅ Windows/Linux packaging helpers.  
✅ Build preflight scripts.  
🟡 Повна vendor-SDK build matrix ще кваліфікується на фізичних hosts.

## Запланований production/unattended profile OAL 1.0

Це **заплановані, але ще не завершені** вимоги переходу від supervised beta до unattended OAL 1.0:

- TLS, authentication, roles/scopes та audit;
- idempotency і durable operation recovery;
- replayable/recoverable WebSocket events;
- observatory-wide emergency stop;
- weather, roof/dome, power safety interlocks;
- production guiding, dither/settle, star-loss/post-flip recovery;
- durable mixed DSO/planetary scheduler з restart resume;
- automatic meridian flip + solve/recenter/reguide/refocus recovery;
- durable science store, checksums/provenance/resumable downloads;
- driver isolation і public conformance/certification suite.

До проходження відповідних HIL safety/recovery gates unattended operation має лишатися явно unqualified.

## 21. Acceptance boundary v0.2.10.5

Реліз вважається **готовим до HIL**, коли presets читаються CMake 3.20+, Windows обирає MSVC, QHY headers не затіняють CRT/STL, vendor SDKs мають правильну архітектуру, `oal-hardware-probe` бачить обладнання, а кожен пристрій проходить supervised HIL gates.

До unattended Internet-facing observatory ще треба закрити P0 safety/security/event/error/idempotency/data-plane/conformance.
