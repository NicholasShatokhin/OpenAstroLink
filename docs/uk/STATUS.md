# Стан OpenAstroSuite / OpenAstroLink — v0.2.10.48

## v0.2.10.48 HIL follow-up автофокуса / експозиції / scheduler

- ✅ Mount milestone: native EQDrive, direct SynScan/EQDrive Wi-Fi та Classic ASCOM/EQMOD тепер HIL-підтверджено однаково правильно наводять те саме реальне монтування з coordinate model v9.
- 🟡 Scene autofocus перероблено після денного HIL: він окремо підбирає й фіксує AF exposure, пробує обидва напрямки фокуса, bracket/refine локальний максимум контрасту та повертається у стартову позицію при flat/failed/cancelled пошуку. Нова поведінка ще потребує convergence HIL на реальній сцені.
- 🟡 Still auto-exposure тепер зберігає абсолютну 16-bit sensor scale по всьому remote path, фіксує результат після входу в target band і повторно шукає експозицію лише після стійкої зміни сцени. Фінальний convergence HIL ще потрібен.
- 🟡 Scheduler перед автономною зйомкою завершує interactive Live View, підтримує edit/delete/reorder/current-pointing fill та immediate/future in-memory UTC start. Durable restart/resume лишається роботою OAL 1.0.

## v0.2.10.47 build-fix 3 — перерахунок direct-MC дзеркальності схід/захід

- Повторний HIL 2026-09-02 підтвердив точне дзеркало схід↔захід при правильному виборі GEM branch відносно Classic EQMOD ASCOM. Перерахунок записаної цілі для всіх чотирьох комбінацій знаків показує, що інверсія Axis1 не може дати спостережуване дзеркало; його створює DEC/polar-distance mapping.
- Coordinate model v9 тому зберігає всі v7 EQMOD HA/Dec branch formulas і використовує HIL-qualified direct-MC mapping `Axis1Sign=+1`, `Axis2Sign=-1`.
- Native EQDrive serial і direct SynScan Wi-Fi лишаються однаковими raw Motor Controller transports; Wi-Fi-only polarity або штучний 90° offset не повертаються.

## v0.2.10.47 mixed scheduler execution

- ✅ DSO FITS/RAW executor збережено.
- ✅ Planetary SER executor: GOTO, full-frame detection, planet autofocus, hardware ROI, finite raw SER та ROI provenance.
- ✅ Native live ROI для QHY + ZWO ASI; ZWO HIL на реальному залізі ще попереду.
- 🟡 Fast ROI tracker реалізований; потрібен real-sky planetary HIL.
- 🟡 Calibrated slow mount correction реалізований, але default OFF до HIL конкретного backend.
- ⏳ Durable restart/recovery, weather/roof safety, meridian flip та thermal focus під час експозиції — OAL 1.0.

## v0.2.10.46 scheduler execution

- `ObservationPlan` / `ObservationBlock` тепер основна scheduler-модель; legacy `SessionTarget` ще приймається.
- Supervised DSO blocks виконуються асинхронно: slew, adaptive solve/recenter з tolerance/retries, autofocus, потім FITS/RAW science frames. Recenter/autofocus можна повторювати кожні N science frames.
- Planetary SER block data model уже є, але autonomous SER/ROI/centroid execution ще не ввімкнено. Scheduler checkpoints/restart/recovery поки non-durable.

## v0.2.10.45 HIL follow-up

- Новий HIL спростував припущення, що direct-MC geometry v6 уже правильна: native EQDrive serial і direct SynScan Wi-Fi рухаються однаково, але обидва промахуються, тоді як EQMOD Classic ASCOM на тому самому залізі наводиться правильно.
- Coordinate model v7 використовує EQMOD-style фізичні GEM pointing states замість вільного вибору найкоротшої з двох математично еквівалентних гілок. Для Moon HIL 2026-08-31 стара модель вибирала приблизно `(-45.69°, -77.30°)`, а v7 дає приблизно `(+44.31°, +77.30°)` на `pier=west`.
- QHY HIL показує майже лінійну залежність сигналу від exposure до clipping. Histogram guidance тепер вимірює fixed sensor scale і не накопичує повторні auto-apply від незмінних Live View кадрів.
- SER sidecar metadata та заглушки додаткових observatory device classes реалізовані в коді; реальні драйвери цих класів ще pending.

## v0.2.10.44 superseded transport-parity step

- Скасовано непідтверджену гіпотезу v0.2.10.43 про окрему DEC polarity для Wi-Fi. Той самий фізичний Motor Controller тепер має єдину інтерпретацію raw counts/механічних осей для serial та UDP/11880.
- Native serial і direct Wi-Fi GOTO використовують один `makeGotoPlan()`, тому transport більше не може розійтися за напрямком або кількістю кроків.
- Direct Wi-Fi status показує точні останні GOTO delta/counts/forward для HIL-порівняння.

## v0.2.10.43 HIL follow-up

- Native serial EQDrive confirmed correct under coordinate model v6.
- Direct `synscan-wifi` HIL indicates mirrored DEC transport polarity on the EQDrive Wi-Fi profile (`9216000/9216000`, timer `53694`); backend now applies Axis2 transport sign `-1` only for that profile and keeps generic UDP/11880 at `+1/+1`.
- UDP socket shutdown is guarded against post-close datagram polling.


## v0.2.10.42 HIL follow-up

- За замовчуванням site з EQMOD/Classic ASCOM є авторитетним; OAL приймає latitude/longitude/elevation backend і використовує їх у власних координатних перетвореннях. Порожній Classic ASCOM endpoint автоматично стає `EQMOD.Telescope`.
- Native EQDrive serial та direct SynScan/EQDrive Wi-Fi використовують direct-MC coordinate model v6 з однією механічною системою: controller counts у момент direct-MC connect визначають session Home/Park `Axis1=0°, Axis2=0°`; controller position packets лишаються integer step-count diagnostics.
- GEM planner v6 перевіряє обидві еквівалентні гілки й вибирає найкоротший фізичний маршрут; Stellarium отримує live native telemetry кожні 250 ms під час slew.
- High-level SynScan hand-controller та SynScan App лишаються RA/DEC backend'ами і не використовують direct-MC axis model.


## v0.2.10.38 — HIL follow-up

- Remote profile монтування передає `maxGotoSkyDeltaDeg` (та legacy alias) і repeatable Home calibration; native safety оцінює реальну кутову відстань по небу, а raw transport має окремий 180° hard cap.
- Native екваторіальні монтування можуть автоматично відновлювати coordinate model зі збереженої механічної Home-позиції; операційні зміни профілю не скидають Sync.
- Mount GUI має vertical scroll, GOTO може auto-unpark; persistent Home/Park калібрується спільною дією для native та Classic ASCOM (`SetPark`).
- Classic ASCOM перед GOTO перевіряє site driver проти OAL profile, намагається auto-sync site/time та блокує небезпечний mismatch з EQMOD; diagnostics включають Az/Alt/LST та EquatorialSystem.
- QHY native Live→Stop→Capture та FITS science spool підтверджені HIL.
- Опційний preview-only debayer camera-neutral: QHY Auto читає SDK `CAM_COLOR`, ZWO Auto — ASI Bayer metadata, а явні CFA-патерни підтримують інші одноканальні Bayer-джерела.
- Доступні autofocus preview та ручний focus jog; Scene AF peak selection посилено після денного HIL.
- Mount target input синхронізує J2000/JNow/Az-Alt/Galactic, а J2000 лишається канонічним для GOTO/Sync.

Позначки: ✅ реалізовано; 🟡 реалізовано/частково реалізовано, але потрібен HIL або production qualification; 🧪 experimental; ⏳ ще не реалізовано.

## Core/control plane

| Область | Стан | Примітка |
|---|---|---|
| GUI / node separation | ✅ | Node володіє hardware/workflows; GUI локальний або remote |
| Node після закриття GUI | ✅ | Перевірено в simulator runtime |
| State snapshot/reconnect | ✅ | GUI відновлює authoritative state |
| Per-device disconnect | ✅ | Main/guide camera, mount, focuser незалежні |
| HTTP API | ✅ | `/api/v1` |
| WebSocket events | 🟡 | Є, але sequence/replay contract ще відсутній |
| Stellarium bridge | ✅ HIL | Stellarium GOTO і live mount position підтверджені; native EQDrive, direct SynScan/EQDrive Wi-Fi та Classic ASCOM/EQMOD тепер однаково правильно наводять реальне монтування. |

## Operations

| Область | Стан | Примітка |
|---|---|---|
| Operation state machine | ✅ | queued/running/succeeded/failed/cancelled |
| Progress/phase/result/problem | ✅ | Є в operation records |
| Cancellation | ✅ | Для operation-backed paths і hardware, що підтримує abort |
| Resource locks | ✅ | Main/guide camera, mount, focuser |
| Async slew | ✅ | operation-backed |
| Async exposure | ✅ | main + guide |
| Async autofocus | ✅ / частковий HIL | camera + focuser; рух реального Gemini під час autofocus підтверджено, але збіжність/повторюваність по реальній оптиці ще треба кваліфікувати |
| Idempotency | ⏳ | `Idempotency-Key` ще немає |
| Durable operations | ⏳ | Restart node ще не відновлює operations |

## Native drivers

| Driver | Стан | Примітка |
|---|---|---|
| QHY | ✅ core HIL / 🟡 workflow hardening | QHY5III462C discovery/connect, повторні FITS, native Live View, Live→Stop→Capture та SER запис підтверджені HIL. v0.2.10.48 виправляє remote 16-bit preview scale і збіжність still auto-exposure; новий lock/reacquire ще треба фінально перевірити HIL. |
| Canon EOS | ✅ базовий HIL / 🟡 regression | EOS 550D EDSDK capture і передавання CR2 уже підтверджені HIL; для актуального релізу ще потрібні regression, довгий Bulb та hotplug/reconnect. |
| ZWO ASI | ✅ реалізовано / 🟡 HIL pending | Native ASI SDK і multi-camera discovery реалізовані; реальної ZWO camera ще не кваліфіковано |
| ZWO EAF | ✅ реалізовано / 🟡 HIL pending | Native EAF SDK реалізований; реальний ZWO EAF ще не кваліфіковано |
| Gemini EAF | ✅ базовий HIL | Windows native discovery/connection, прямий рух і рух під час autofocus підтверджені; ще потрібні long-run/reconnect/limits тести |
| Sky-Watcher/SynScan | ✅ базовий direct Wi-Fi HIL | `synscan-wifi` UDP/11880 підключився і фізично рухав mount; `synscan-app` лишається compatibility path через SynScan Pro/App UDP/11881 |
| EQDrive native | ✅ HIL | Direct-MC coordinate model v9 (`Axis1=+1`, `Axis2=-1`) HIL-кваліфікований проти Classic ASCOM/EQMOD; native serial і direct Wi-Fi тепер наводять однаково. Meridian-flip/limit automation лишається майбутнім hardening. |
| INDI | ✅ | Optional compatibility client |
| Alpaca | ✅ | Compatibility |
| LX200 | ✅ | Minimal compatibility |

## Imaging/workflows

| Область | Стан |
|---|---|
| Main + guide camera roles | ✅ |
| Main + guide optical train | ✅ |
| Single-frame capture | ✅ |
| Preview | ✅ |
| Native frame publication | ✅ |
| Durable FITS/RAW store | ⏳ |
| Planetary SER pipeline | 🟡 реалізовано / частковий HIL | Native SER + metadata sidecar підтверджені HIL. Autonomous full-frame acquisition, hardware ROI tracking і `.roi.jsonl` provenance реалізовані; реальний autonomous planetary tracking та drop/jitter accounting ще потребують HIL/hardening. |
| ASTAP | 🟡 adapter є, real-sky HIL pending |
| Coordinate frames | ✅ foundation: J2000 canonical; JNow/of-date input via precession; full apparent/topocentric corrections ще не реалізовані |
| Adaptive urban solve | 🟡 короткі експозиції + оцінка якості + background removal + registration/stack + mount hint + retry реалізовані; потрібен HIL у міському небі |
| Closed-loop GOTO | 🟡 groundwork, HIL pending |
| Autofocus | 🟡 algorithm є, real optics tuning pending |
| Polar math | ✅ |
| Automatic polar wizard | 🟡 orchestration/HIL pending |
| Guiding | 🟡 basic API, production loop pending |
| Durable scheduler | 🟡 supervised mixed executor | v0.2.10.48 виконує DSO FITS/RAW і planetary SER blocks, перед acquisition завершує interactive Live View, підтримує edit/delete/reorder/current-pointing fill та immediate/future in-memory start. Durable restart/recovery й unattended safety ще попереду. |

## Mount geometry foundation — v0.2.10.25

- J2000/JNow sky coordinates відокремлені від raw mechanical axes.
- Профілі: German equatorial, fork equatorial, Alt-Az, Alt-Az+derotator, equatorial platform, custom two-axis.
- Native EQDrive та direct SynScan/EQDrive Wi-Fi використовують `MountGeometryModel` у OAL Core; hardware driver відповідає за raw axes/motion.
- GEM: UTC/longitude → local sidereal time → hour angle → pier branch → mechanical axes; один Sync визначає encoder offset/signs конкретної установки.
- Mechanical Home/Park direct-MC default Axis1=0°, Axis2=0°, configurable; GUI може зберегти поточні mechanical axes як Park. Classic ASCOM зберігає Park semantics свого ASCOM-драйвера.
- Automatic meridian flip за замовчуванням вимкнений. Тимчасовий 15° test envelope для native/direct GOTO знято; довгі переходи використовують shortest-axis envelope і мають виконуватися під наглядом до повної HIL-кваліфікації pier/limits.
- Alt-Az coordinate conversion уже є; production two-axis tracking/derotator control ще попереду.
- Classic ASCOM operation slew логуватиме RA/DEC/pier/tracking під час руху.
- Явний Refresh може hard-reload неактивний QHY OAL driver DLL/SDK після zero-device scan; periodic vendor polling вимкнений.

## P0 hardening

Capabilities/discovery — 🟡; async operations — 🟡; resource locks — ✅; idempotency — ⏳; TLS/auth/scopes/audit — ⏳; safety/weather/roof/power — ⏳; durable science data plane — 🟡 foundation only; reliable replayable WebSocket — ⏳; RFC 9457 HTTP Problem Details — 🟡 incomplete; public conformance suite — 🟡 incomplete.

## Cross-platform build

| Target | Стан |
|---|---|
| Windows x64 MSVC + Ninja | 🟡 повний vendor SDK build ще кваліфікується |
| Linux x86_64 | 🟡 presets/docs готові, HIL pending |
| Linux ARM64/RPi | 🟡 HIL pending |
| WSL compile/test | 🟡 hardware лише через окремий passthrough |

### Нове у v0.2.10.5

- CMake preset schema v2 → CMake 3.20+.
- `cmake_minimum_required(VERSION 3.20)`.
- Windows official path: Ninja + MSVC.
- `build_windows.ps1` може сам завантажити `vcvars64.bat`.
- Збережена QHY/MSVC header isolation.
- Окремі Windows presets без Canon та з EDSDK.
- Оновлені Linux SDK staging/architecture instructions.

v0.2.10.25 — supervised HIL qualification release, а не unattended observatory release. Native Gemini, native EQDrive, direct SynScan/EQDrive Wi-Fi та Classic ASCOM/EQMOD уже пройшли базові hardware tests; QHY hot-plug/repeated capture, повна GEM geometry і configurable park ще потребують кваліфікації.


## v0.2.10.25 — coordinate frames / discovery

- J2000 є канонічною екваторіальною системою OAL; JNow/of-date підтримується через прецесію.
- Після server-first startup виконується один vendor-neutral native scan; надалі hardware сканується лише явною кнопкою **Refresh native device discovery** / API. Періодичного QHY або іншого vendor polling немає.
