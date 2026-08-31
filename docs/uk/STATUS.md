# Стан OpenAstroSuite / OpenAstroLink — v0.2.10.38


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
| Stellarium bridge | ✅ базовий HIL | Stellarium GOTO підтверджено через node та Classic ASCOM/EQMOD; parity native mount ще кваліфікується |

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
| QHY | 🟡 активний HIL | QHY5III462C discovery/connect/single capture підтверджені; користувацькі кадри зберігаються у FITS. У v0.2.10.34 Finder Live View переведено на native QHYCCD continuous stream, а health-probe виконується лише в idle та вимагає три послідовні помилки перед disconnect. Наступний Windows HIL: Live→Stop→Capture і повторний reconnect |
| Canon EOS | 🟡 | EDSDK Windows / libgphoto2 Linux; HIL pending |
| ZWO ASI | 🟡 | Native ASI SDK, multi-camera; HIL pending |
| ZWO EAF | 🟡 | Native EAF SDK; HIL pending |
| Gemini EAF | ✅ базовий HIL | Windows native discovery/connection, прямий рух і рух під час autofocus підтверджені; ще потрібні long-run/reconnect/limits тести |
| Sky-Watcher/SynScan | ✅ базовий direct Wi-Fi HIL | `synscan-wifi` UDP/11880 підключився і фізично рухав mount; `synscan-app` лишається compatibility path через SynScan Pro/App UDP/11881 |
| EQDrive native | ✅ базовий HIL / 🟡 geometry | `oal.eqdrive` знайшов і рухав реальний контролер; v0.2.10.25 переводить raw axes через Core GEM/fork/Alt-Az geometry layer та mechanical Park; long-slew/pier-flip HIL ще потрібен |
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
| Planetary SER pipeline | ⏳ |
| ASTAP | 🟡 adapter є, real-sky HIL pending |
| Coordinate frames | ✅ foundation: J2000 canonical; JNow/of-date input via precession; full apparent/topocentric corrections ще не реалізовані |
| Adaptive urban solve | 🟡 короткі експозиції + оцінка якості + background removal + registration/stack + mount hint + retry реалізовані; потрібен HIL у міському небі |
| Closed-loop GOTO | 🟡 groundwork, HIL pending |
| Autofocus | 🟡 algorithm є, real optics tuning pending |
| Polar math | ✅ |
| Automatic polar wizard | 🟡 orchestration/HIL pending |
| Guiding | 🟡 basic API, production loop pending |
| Durable scheduler | 🟡 scaffolding, recovery/flip/dither/refocus pending |

## Mount geometry foundation — v0.2.10.25

- J2000/JNow sky coordinates відокремлені від raw mechanical axes.
- Профілі: German equatorial, fork equatorial, Alt-Az, Alt-Az+derotator, equatorial platform, custom two-axis.
- Native EQDrive та direct SynScan/EQDrive Wi-Fi використовують `MountGeometryModel` у OAL Core; hardware driver відповідає за raw axes/motion.
- GEM: UTC/longitude → local sidereal time → hour angle → pier branch → mechanical axes; один Sync визначає encoder offset/signs конкретної установки.
- Mechanical Park default Axis1=90°, Axis2=0°, configurable; GUI може зберегти поточні mechanical axes як Park. Classic ASCOM зберігає Park semantics свого ASCOM-драйвера.
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
