# Оновлення стану — v0.2.10.51

## v0.2.10.51 — Sky Map MVP

OpenAstroSuite тепер має lightweight offline Sky Map у лівій робочій області. Вона рендерить horizon/all-sky view через existing observer/time coordinate path, має bright-star/DSO search, pan/zoom, live telescope і plate-solve markers, approximate main-camera FOV та controller-backed Slew/Sync/Abort/Park і transfer target у Scheduler. Mount geometry не змінена; direct-MC v9 лишається frozen.


**Build foundation:** Windows x64 ✅, Linux x86_64 ✅, Raspberry Pi/Linux ARM64 cross node+probe+native drivers ✅, macOS presets/bootstrap 🟡 physical build pending. Native OAL drivers — default; INDI — opt-in.

**Mount:** direct-MC coordinate model v9 HIL-підтверджений і frozen. Тимчасовий driver-level 15°/`maxNativeGotoDeg` qualification gate видалений. Core/profile sky-safety лишається user-controlled; raw-axis motion має явний mechanical guard.

**Наступна Beta-кваліфікація:** HIL autofocus → auto-exposure → scheduler → mosaic → Polar Alignment. Smart Telescope UX — OAL 1.0.

# Стан OpenAstroSuite / OpenAstroLink — v0.2.10.51

## v0.2.10.49 — persistent calendar / mosaic / Polar safety

- ✅ Час scheduler тепер per `ObservationBlock`: незалежні події можна розпланувати в одному node-calendar на місяці або рік уперед.
- ✅ Calendar JSON, armed-state і next-block cursor переживають restart node. Restart між подіями продовжує з першого незавершеного блока; restart усередині події запускає цей block заново. Resume всередині FITS/SER лишається майбутнім checkpointing OAL 1.0.
- ✅ Опційні `parkAfter` та `autoUnparkBefore` дозволяють ставити mount у Park між рідкими подіями.
- ✅ `mosaic-fits` executor рахує центри тайлів із optical-profile FOV, rows/columns, overlap та rotation, проходить їх serpentine і застосовує звичайний DSO solve/recenter/autofocus/FITS policy для кожного tile.
- ✅ Polar Alignment motion має persisted Az/Alt safe region; кожний RA-offset slew перевіряє весь шлях і відхиляється, якщо хоча б одна проміжна точка виходить за дозволену ділянку. Solve/sample wizard поки guided, а не один повністю автоматичний composite operation.

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
| Scheduler/calendar | ✅ persistent mixed calendar / 🟡 unattended hardening | v0.2.10.49 виконує DSO FITS/RAW, planetary SER і mosaic FITS blocks з per-block UTC датами, опційним Park/Unpark та persisted plan/armed/next-block cursor. Mid-block resume, weather/roof hold, meridian recovery й повна unattended safety ще попереду. |

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

### v0.2.10.49 build-fix9 — build matrix

- ✅ Windows observatory `--clean-first` збірка підтверджена на реальному MSVC 2022 host.
- ✅ Linux presets більше не вимагають Ninja: використовують Unix Makefiles, Windows лишається MSVC + Ninja.
- ✅ Native RPi ARM64/ARMHF presets тепер перевіряють реальну target architecture.
- ✅ Є cross node presets ARM64/ARMHF як із Linux/WSL, так і з установлених Windows Arm GNU Toolchains.
- ✅ build-fix4 додав автоматичне створення Bookworm ARM64/ARMHF sysroot, matching host Qt tools і staging QHY ARM SDK із `QHYCCD_Linux_New`.
- ✅ build-fix5 явно монтує/реєструє `binfmt_misc`, перевіряє QEMU виконання перед debootstrap stage two, уміє продовжити перерваний sysroot, додає Debian archive keyring та Qt Positioning.
- ✅ build-fix6 виправляє Jammy stale-keyring failure (`F8D2585B8783D481`): Bookworm signing keys завантажуються з Debian, fingerprints перевіряються, а verified keyring передається в `debootstrap --keyring`; GPG verification не вимикається.
- ✅ build-fix7 виправляє `name: unbound variable` у verified-key helper під `set -u` шляхом розділення `local` declarations та assignments.
- ✅ build-fix8 підтвердив discovery target Qt 6.4.2/OpenCV 4.6 у Bookworm ARM64 sysroot.
- ✅ build-fix9 робить QHY ARM staging перевіреним і фатальним, якщо його запитано: до readiness мають реально існувати `include/qhyccd.h` і `lib/libqhy.so` правильної ELF-архітектури; build helper бере ці шляхи з env record.
- 🟡 Повна ARM64 cross-build/HIL-кваліфікація ще триває; Canon EDSDK ARM64 і ZWO `armv8` уже підтверджені. Для QHY ARM64 потрібен справжній AArch64 SDK; legacy `QHYCCD_Linux_New` `armv8` archive фактично містить 32-bit ARM.


| Target | Стан | Примітки |
|---|---|---|
| Windows x64 MSVC + Ninja | ✅ clean build | v0.2.10.49 build-fix1 успішно пройшов повну observatory збірку `--clean-first` на реальному Windows/MSVC/Qt/vendor-SDK host 2026-09-03. |
| Linux x86_64 | 🟡 build qualification | build-fix3 прибирає випадкову обов’язковість Ninja: Linux presets використовують Unix Makefiles; далі треба кваліфікувати Qt6/OpenCV/vendor runtime. |
| Linux ARM64/RPi | 🟡 build qualification | Bookworm ARM64 sysroot, matching host Qt tools, target Qt 6.4.2 та OpenCV 4.6 уже фізично підтверджені у WSL. Перший full configure виявив неіснуючий QHY staging path; build-fix9 тепер не дозволяє оголосити environment ready без валідного QHY SDK. |
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

- Linux build baseline clarified: Qt >= 6.4 is required because the node REST API uses Qt HttpServer; Ubuntu 22.04/Jammy system Qt 6.2.4 is unsupported, while Ubuntu 24.04+ and Debian/Raspberry Pi OS Bookworm+ are valid system-Qt baselines.

- 🟡 macOS ARM64/x86_64: додані presets Apple Clang, пошук Canon EDSDK і ZWO macOS SDK; фізична build/HIL-кваліфікація на Mac ще попереду.


### v0.2.10.49 build-fix8 — target Qt/OpenCV у cross sysroot

ARM cross toolchains явно фіксують Debian multiarch (`aarch64-linux-gnu` / `arm-linux-gnueabihf`) та автоматично задають `Qt6_DIR` і `OpenCV_DIR` на target-конфіги всередині `OAS_CROSS_SYSROOT`. Host Qt (`OAS_QT_HOST_PATH`) використовується тільки для виконуваних `moc/rcc/uic`. Bootstrap ARM64 sysroot + host Qt + QHY staging уже підтверджено реальним запуском у WSL 2026-09-03; повна cross-компіляція ще триває.


### v0.2.10.49 build-fix9 — валідований QHY staging

Реальний `my-rpi4-cross-arm64-full` успішно знайшов target Qt/OpenCV і зупинився лише тому, що bootstrap записав QHY SDK path, якого фізично не було. Тепер QHY staging є частиною success contract: збій не ковтається як warning, exact header/library paths та ELF architecture перевіряються, а `build_rpi_cross.sh` явно передає staged QHY SDK у CMake.

### v0.2.10.49 build-fix10 — виправлення QHY ARM ABI

- ✅ Фізична перевірка `qhyccdsdk-v2.0.11-Linux-Debian-Ubuntu-armv8.tar.gz` показала, що `libqhyccd.so` є **ELF 32-bit ARM EABI5**, попри назву `armv8`. Її не можна лінкувати в ARM64/AArch64 OAL node.
- ✅ QHY staging тепер вважає назву archive лише підказкою, а остаточно вибирає SDK за реальною ELF-архітектурою через `file(1)`.
- ✅ Legacy `QHYCCD_Linux_New` `armv8` archive приймається для ARMHF, але не ARM64.
- ✅ `bootstrap_rpi_cross.sh` за замовчуванням працює в QHY `auto` mode: відсутній/несумісний QHY SDK не блокує валідний ARM64 sysroot; `--require-qhy` залишає strict full-vendor gate.
- ✅ `stage_qhy_cross_sdk.sh` приймає як каталог vendor archives, так і прямий шлях до актуального QHY SDK archive.
- 🟡 ARM64 build-кваліфікацію продовжуємо спочатку з Canon EDSDK ARM64 + ZWO ASI/EAF `armv8` і QHY OFF. Повний QHY ARM64 gate чекає на справжній актуальний QHY **Arm_64/AARCH64** SDK.


### v0.2.10.49 build-fix12 — перші реальні ARM64 portability blockers у source

Реальна WSL -> AArch64 збірка вже успішно конфігурується проти Bookworm ARM64 sysroot (Qt 6.4.2 + OpenCV 4.6) і компілює simulated, EQDrive, Gemini, SkyWatcher та ZWO native drivers. Перші source-portability помилки локалізовано в Linux headers Canon EDSDK та переході API Qt HttpServer.

- ✅ Для Canon EDSDK 13.20.x Linux додано локальний GCC compatibility shim: MSVC-спелінг `__int64` та відсутній alias `WCHAR` мапляться без редагування vendor SDK.
- ✅ `OalServer::start()` тепер сумісний і з Qt 6.4-6.7 (`QAbstractHttpServer::bind()` повертає `void`), і з Qt 6.8+ (`bind()` повертає `bool`).
- ✅ Геометрія/runtime mount v9 не змінювались.
- 🟡 ARM64 compile/link qualification продовжується з наступної реальної compiler/linker помилки.


### v0.2.10.49 build-fix13 — dependency closure фінального ARM64 link

Реальна WSL -> AArch64 збірка тепер доходить до 100% компіляції source. Canon EDSDK ARM64, ZWO ASI/EAF ARM64, Gemini, SkyWatcher, EQDrive та `oas_core` успішно компілюються/лінкуються; падали лише фінальні executable `openastrolink-node` і `oal-hardware-probe`. Причина — search path cross-linker, а не source OAL: GNU ld не резолвив транзитивні BLAS/LAPACK/Armadillo/ARPACK/SuperLU залежності OpenCV з Bookworm sysroot і міг перейти до Jammy cross-runtime libc. build-fix13 додає target-sysroot `-rpath-link`/`-L` closure та явну bootstrap-перевірку BLAS/LAPACK. QHY для ARM64 лишається OFF до справжнього AArch64 SDK. Mount v9 не змінювався.

## Cross-platform build status — v0.2.10.51

| Target | Статус | Evidence / boundary |
|---|---|---|
| Windows x64, MSVC 2022 + Ninja | ✅ confirmed | Clean/full observatory build успішний на physical Windows host; preset pin-ить `cl.exe`. |
| Linux x86_64 | ✅ confirmed | Native observatory build успішний на Ubuntu 22.04 через OAL per-user Qt bootstrap, а не system Qt 6.2.4. |
| Linux ARM64 / Raspberry Pi 4 | ✅ confirmed build | WSL/Linux→AArch64 build доходить до 100% для node/probe та native QHY 26.06.04, Canon EDSDK, ZWO ASI/EAF, Gemini, Sky-Watcher, EQDrive. |
| Linux ARM64 / Raspberry Pi 5 | 🟡 ABI-supported | Той самий generic AArch64 target; physical Pi 5 runtime/HIL ще pending. |
| OpenAstroSuite ARM64 GUI | 🟡 pending | Cross observatory GUI preset є; node/probe confirmed, display/runtime qualification ще потрібна. |
| macOS ARM64 / x86_64 | 🟡 configured | Apple Clang presets/bootstrap є; physical Mac build/sign/runtime pending. |

Native OAL drivers — default, INDI — явний opt-in compatibility. Temporary EQDrive `maxNativeGotoDeg` gate видалений після v9 HIL; Core/profile sky-safety та raw-axis explicit guard лишаються.

