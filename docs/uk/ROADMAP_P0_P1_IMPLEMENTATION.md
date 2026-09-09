# Roadmap OpenAstroLink — immediate execution після v0.2.10.58-buildfix9

**Snapshot:** 2026-09-09  
**Master checklist:** `CURRENT_CHECKLIST.md`

Попередній blocker `fresh Windows build` закритий. Точний current Windows x64/MSVC build тепер успішно configure/link з WebRTC, WebRTC runtime staging, правильним ZWO EAF DLL import library і Canon EDSDK, жорстко спареним з AMD64 `EDSDK_64` runtime.

Найближча програма — **runtime/HIL qualification**, а не новий широкий architecture rewrite.

## v0.2.10.58-buildfix9 execution order

1. Runtime-regression native plugin registry: `oal.canon` і `oal.zwo.eaf` мають load без Win32 error 193.
2. QHY high-rate HIL: short exposure, `Capture FPS=MAX`, `Preview=60`; capture/record/preview FPS і drops.
3. SER 5–10 хв з `recordDropped=0`; AutoStakkert frame count проти OAL telemetry/sidecar.
4. Preview 15/30/60/MAX під час recording; GUI preview не має матеріально знижувати recording throughput.
5. Підтвердити actual WebRTC DataChannel transport, потім failure/fallback/reconnect.
6. Main + Guide Dual Live на двох physical cameras з per-role fallback.
7. Repeat 30–50 direct-Wi-Fi manual press/release/direction changes.
8. Repeat Scene AF і sparse-scene still auto-exposure HIL; потім night HFR AF.
9. Scheduler DSO + ASTAP/recenter HIL.
10. Planetary SER/ROI executor HIL.
11. Mosaic 2×2 + footprint overlay HIL.
12. Polar Alignment real-sky HIL.
13. Discovery/hotplug regression.
14. Portable Windows package + clean-machine test.
15. Full supervised night qualification і deploy **current** `site/` source на вже live `openastro.link` DNS/HTTPS endpoint.

Не відкривати знову mount coordinate model v9 без нових суперечливих HIL-доказів. Smart Telescope UX і broad unattended-observatory automation лишаються scope OAL 1.0.

# Roadmap OpenAstroLink після v0.2.10.52

Позначки: ✅ зроблено; 🟡 частково/HIL pending; ⏳ не зроблено.

## P0

1. **Capabilities/identity/discovery — 🟡**: registry/identity/device capabilities є; треба заморозити normative schemas/versioning.  
2. **Async operations — 🟡**: slew/exposure/autofocus готові; треба перевести решту довгих workflows і додати durable recovery.  
3. **Idempotency/locks — 🟡**: locks/queueing є; `Idempotency-Key` і повний lock graph ще немає.  
4. **Security/safety — ⏳**: TLS/auth/roles/scopes/audit/emergency stop/weather/roof/power policy.  
5. **Science data plane — 🟡**: native frame/preview foundation є; FITS/RAW/SER/checksum/resume/zero-copy/object storage ще треба.  
6. **Reliable WS — ⏳**: stream ID/sequence/replay/lastSequence/snapshot contract.  
7. **Error model — 🟡**: structured operation problems є; повний RFC 9457 HTTP ще ні.  
8. **Conformance — 🟡**: simulator/checks є; public black-box suite ще ні.

## P1

- **Sky Map navigation — ✅ MVP**: v0.2.10.51 додав offline left-side map з catalogue targets, pan/zoom/search і telescope/solved/FOV overlays. v0.2.10.52 додає вибір довільної видимої точки неба з тим самим Slew/Sync/Scheduler contract. Catalogue-object → Scheduler transfer уже підтверджений у працюючому GUI; free-point UI/HIL smoke ще треба швидко перевірити. Full planetarium/Smart Telescope presentation лишається later scope.
- **Sky Map framing — ✅ implemented / HIL pending**: measured solved rectangle, predicted main/guide footprints, Scheduler-synchronized mosaic grid та optional Stellarium Remote Control export є у v0.2.10.53. Full planetarium/Smart Telescope presentation лишається для 1.0.
- Filter wheel, rotator, dome/roof, weather/safety, power/switch, cover/calibrator, GPS/time — ⏳.
- ASTAP — 🟡; astrometry.net production adapter — ⏳.
- Durable mixed-mode scheduler — 🟡 supervised DSO executor / ⏳ durable + planetary: v0.2.10.46 вже виконує slew → solve/recenter → autofocus → FITS/RAW × N і periodic recenter/autofocus; v0.2.10.47 додає planetary SER, full-frame acquisition, ROI tracking/provenance та опційний calibrated mount recenter; далі temperature focus, constraints, checkpoints/restart, meridian flip, weather/recovery. Повна модель — `SCHEDULER.md`.
- Guiding — 🟡 foundation; calibration/RMS/dither/backlash/lost-star recovery — ⏳.
- Geometric Bahtinov — ⏳.
- Sandboxed driver host + public C++/Python/Rust SDKs — ⏳.

## Найближча послідовність

1. Fresh build current high-rate branch.
2. High-rate main-camera + SER zero-drop HIL.
3. Dual Live main+guide HIL.
4. Repeat direct-Wi-Fi manual-slew HIL.
5. Repeat scene autofocus і still auto-exposure HIL; потім night star/HFR AF.
6. Scheduler HIL.
7. Mosaic HIL.
8. Polar Alignment HIL.
9. Full supervised night qualification.
10. Після Beta workflow gates продовжити production guiding, durable session/data-plane та решту P0 hardening.
11. Smart Telescope UX лишити в OAL 1.0 track.

**Supervised first-light:** build + HIL + ASTAP + autofocus.  
**Повний imaging workflow:** додати closed-loop GOTO, polar alignment, DSO storage, SER.  
**Autonomous observatory:** guiding + durable scheduler + safety/recovery.  
**Public OAL ecosystem:** усі P0 + conformance suite.

## Ціль OAL 1.0 — автономна обсерваторія

Заплановано, але ще не завершено: TLS/auth/roles/audit, idempotency, durable operations, replayable events, safety/weather/roof/power interlocks, emergency stop, production guiding, durable mixed DSO/planetary scheduler, automatic meridian flip recovery, durable science data/provenance, driver isolation та public conformance. Це явні roadmap-вимоги 1.0, а не твердження про поточну beta.

## Immediate Windows gate після buildfix8
Перед high-rate/QHY WebRTC throughput sequence треба закрити два native vendor plugin loader failures: виконати AMD64 runtime repair/diagnostic, вимагати завантаження Canon EDSDK і ZWO EAF без `ERROR_BAD_EXE_FORMAT`, а вже потім продовжити camera-max/Preview-60 та zero-record-drop SER HIL.

## Desktop night UI та mobile GUI OAL 1.0

- **v0.2.10.59 / Beta:** Qt Widgets Normal + Night Vision + Strict Night Mode; optional black→red display palette для mono/raw; завершити Windows/UI HIL.
- **OAL 1.0:** залишити Widgets expert desktop client і додати окремий mobile/touch Qt Quick/QML frontend через ті самі OAL HTTP/events/WebRTC interfaces. Hardware ownership у QML не переносити.
