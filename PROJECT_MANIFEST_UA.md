## v0.2.10.50

- Package: `0.2.10.50-cross-platform-mount-v9-release`
- Core version: `0.2.10.50`
- Підтверджені real-host builds: Windows x64/MSVC+Ninja, native Linux x86_64 та Linux/WSL→AArch64 Raspberry Pi node/probe + native vendor-driver matrix.
- Native OAL drivers — default; INDI лишається тільки opt-in compatibility.
- Raspberry Pi 4 і Pi 5 використовують спільний Linux ARM64/aarch64 target; legacy `rpi4-*` назви presets лишаються сумісними. Physical Pi 5 runtime qualification ще pending.
- `oal.eqdrive` v0.2.10.50 видаляє тимчасовий прихований `maxNativeGotoDeg` qualification envelope після HIL-підтвердження coordinate model v9. Геометричні формули, signs, Home/Park та transport direction logic не змінювалися.
- Core/profile `maxGotoSkyDeltaDeg` (`maxGotoAxisDeltaDeg` legacy alias) лишається operator-controlled supervised sky-safety policy; raw-axis request має окремий `maxAxisDeltaDeg` guard.
- Найближча Beta: HIL autofocus, auto-exposure, scheduler, mosaic, Polar Alignment. Smart Telescope UX — OAL 1.0.

### Відновлення Linux dependency bootstrap — build-fix22

- Виправлено architecture identifier aqtinstall для Linux x86_64: `linux_gcc_64` (а не назва каталогу встановлення `gcc_64`).
- Додано live-перевірку через `aqt list-qt` потрібної Qt-архітектури та модулів HttpServer/WebSockets/SerialPort/Positioning.
- Linux apt bootstrap спочатку використовує наявні package indexes і лише як fallback запускає `apt-get update`; сторонні зламані PPA автоматично не змінюються.
- Додано timeout для apt-lock та regression-перевірку.
- Runtime, геометрія mount v9 та default-політика INDI не змінювались.

### Відновлення Windows build-system — build-fix21
- Нативні Windows presets повернено на перевірений шлях **MSVC + Ninja** з явним `CMAKE_CXX_COMPILER=cl.exe`; CMake більше не залежить від Visual Studio instance discovery.
- Це одночасно не дозволяє Strawberry/MinGW `c++.exe` потрапити в збірку з Qt MSVC2022 та vendor `.lib`.
- Нативні Windows build directories отримали новий суфікс `*-msvc-ninja`, тому і старі GNU/Ninja cache, і невдалі VS-generator cache ігноруються.
- `scripts/build_windows.ps1` сам завантажує `vcvars64` і знаходить Ninja у `PATH` або в Visual Studio CMake tools. Сирий preset запускайте з x64 MSVC Developer Command Prompt.
- Windows-hosted Raspberry Pi cross presets залишаються GNU/Ninja і не змінювались.

# Маніфест проєкту — OpenAstroSuite / OpenAstroLink v0.2.10.50

## v0.2.10.49

- Пакет: `0.2.10.49-calendar-mosaic-polar-safety`
- Версія Core: `0.2.10.49`
- Scheduling тепер per-block: кожна подія має власні UTC дату/час, тому persistent calendar node може планувати спостереження на місяці або рік уперед.
- Node зберігає календар, armed state і cursor наступного блока. Завершені blocks після restart не повторюються; block, перерваний restart процесу, стартує заново з межі блока. Resume всередині FITS/SER лишається роботою OAL 1.0.
- Blocks підтримують `parkAfter` і `autoUnparkBefore` для опційного Park між рідкими календарними подіями.
- Додано executable `mosaic-fits` з геометрією тайлів із optical-profile FOV, overlap, rotation, serpentine traversal та звичайними DSO solve/recenter/autofocus/capture policies для кожного тайла.
- RA-offset рух Polar Alignment можна обмежити persistent горизонтальною Az/Alt safe region; весь шлях slew семплується й відхиляється, якщо виходить за дозволену область.
- Mount v9 лишається HIL-кваліфікованим для native EQDrive, direct SynScan/EQDrive Wi-Fi та Classic ASCOM/EQMOD.

### Build infrastructure follow-up — build-fix9
- Додано явний автоматичний bootstrap Raspberry Pi cross-sysroot для Debian/Ubuntu/WSL: відтворюваний Debian 12/Bookworm target root або точне дзеркало реального Pi через `--from-pi`.
- Додано bootstrap matching host Qt для cross-build інструментів `moc`/`rcc` та підтримку `OAS_QT_HOST_PATH`.
- Додано staging QHY ARM SDK із sibling checkout `QHYCCD_Linux_New` з перевіркою ELF-архітектури; ARM vendor files не встановлюються в x86_64 host.
- Посилено WSL foreign-root bootstrap: явне mount/register `binfmt_misc`, перевірка виконання ARM перед debootstrap stage two, продовження перерваного root, `debian-archive-keyring` і Qt Positioning у target dependencies.
- build-fix6 обходить застарілий `debian-archive-keyring` Ubuntu 22.04 без вимкнення перевірки підписів: bootstrap завантажує офіційні Debian 12 archive/release/security keys через HTTPS, перевіряє зафіксовані fingerprints, формує локальний verified keyring і передає його в `debootstrap --keyring`.
- build-fix7 виправляє Bash-помилку порядку ініціалізації під `set -u` у helper для завантаження Bookworm keys (`name: unbound variable`): local declarations і assignments тепер розділені, тому bootstrap доходить до verified-key download/import stage на Jammy/WSL.
- build-fix8 явно задає target Qt/OpenCV CMake package directories із Debian multiarch sysroot (`aarch64-linux-gnu` / `arm-linux-gnueabihf`) і зберігає їх у bootstrap env; host Qt лишається тільки для `moc/rcc/uic`.
- build-fix9 робить запитаний QHY ARM staging обов’язковою перевіреною частиною success contract, перевіряє staged header/library architecture та передає SDK з bootstrap env record у cross-build helper.
- Coordinate model mount v9 та вся HIL-кваліфікована геометрія не змінювалися.

### Інфраструктура збірки — build-fix18

- Native wrapper-и Linux/macOS/Windows тепер шукають і за потреби автоматично готують redistributable залежності перед CMake: Qt, OpenCV, QHY і ZWO там, де є детерміноване джерело.
- CMake повторно використовує per-user managed cache Qt/vendor SDK при наступних native configure.
- Ubuntu 22.04 може автоматично отримати custom Qt >= 6.4 замість несумісного системного Qt 6.2.4.
- Canon EDSDK лишається локальним search-only; INDI лишається опціональним/OFF; mount v9 не змінено.

## v0.2.10.48

- Пакет: `0.2.10.48-safe-af-exposure-scheduler-lifecycle`
- Версія Core: `0.2.10.48`
- Підтверджено HIL: native EQDrive, direct SynScan/EQDrive Wi-Fi та Classic ASCOM/EQMOD тепер однаково правильно наводять реальне монтування з direct-MC coordinate model v9 (`Axis1=+1`, `Axis2=-1`).
- Scene autofocus тепер окремо метерує та фіксує AF exposure, пробує обидва напрямки від стартового фокуса, bracket/refine локальний contrast peak та транзакційно повертає стартову позицію при flat/failed/cancelled пошуку.
- Remote 16-bit preview більше не робить повторний per-frame min/max stretch; display auto-stretch також не перетворює clipped/flat white frame на чорний.
- Still histogram auto-exposure має explicit convergence/LOCKED state, запам'ятовує найкращу exposure і повторно запускається лише після стійкої зміни сцени; Live/AF frames не змінюють still-controller.
- Scheduler перед autonomous acquisition завершує interactive Live View і чекає звільнення camera lock; GUI підтримує edit/delete/clear/reorder блоків, копіювання поточних J2000 координат телескопа, Start now та future in-memory UTC start.
- Future start ще non-durable при restart node; durable scheduler recovery, weather/roof safety, meridian recovery та unattended hardening лишаються OAL 1.0 work.

## v0.2.10.47

- Пакет: `0.2.10.47-planetary-ser-executor-buildfix3`
- Версія Core: `0.2.10.47`
- `ObservationPlan` отримав реальний mixed-mode planetary executor: GOTO, full-frame пошук планети, planetary autofocus, hardware ROI незмінного розміру та finite SER runs.
- Додано `PlanetDetector` для acquisition/tracking яскравих компактних об’єктів.
- Native QHY і ZWO ASI live transport підтримують hardware ROI; SER лишається raw/pre-debayer.
- Додано `<SER basename>.roi.jsonl` provenance для точного відтворення регіону сенсора під dark/flat.
- Fast loop рухає ROI; опційний slow loop калібрує 2×2 RA/DEC→pixel response та робить обмежені coordinate micro-slew corrections. Slow loop за замовчуванням вимкнений до HIL.
- DSO і planetary blocks можна змішувати в одному плані. Durable restart, safety/weather/meridian та thermal focus під час експозиції лишаються роботою до OAL 1.0.
- Build-fix 3 оновлює direct EQDrive/SynScan-WiFi Core geometry profiles до coordinate model v9. Повний перерахунок повторного HIL 2026-09-02 для всіх чотирьох фізичних mappings визначає точне дзеркало схід↔захід як polarity-проблему Axis2/DEC-polar-distance: `Axis1Sign=+1`, `Axis2Sign=-1`. v7 EQMOD branch geometry та валідні Home/Park v7/v8 зберігаються.

## v0.2.10.46

- Пакет: `0.2.10.46-observation-plan-dso-executor`
- Core version: `0.2.10.46`
- Основна scheduler-модель тепер `ObservationPlan` / `ObservationBlock`; старий `SessionTarget` лишився compatibility wrapper.
- Додано перший реальний supervised DSO executor на node через звичайні async OAL operations/resource locks: `slew -> adaptive solve/recenter -> autofocus -> FITS/RAW capture`.
- Solve/recenter вимірює plate-solved pointing error, робить `Sync + correction slew`, повторює до заданої tolerance в arcmin і може запускатися перед першим кадром або кожні N science frames.
- Autofocus може запускатися перед першим кадром або кожні N science frames. DSO science capture запитує durable output (`saveRaw=true`).
- Session status тепер містить block cursor, current step/operation, per-block/global frame counters та terminal failure reason. `/api/v1/sessions/current/plan` повертає завантажений план.
- `planetary-ser` вже є first-class mode/wire model, але autonomous executor навмисно ще не ввімкнений; ROI/centroid/SER-run orchestration — наступний етап.
- Scheduler поки non-durable при restart node; weather/meridian/recovery/unattended safety лишаються roadmap OAL 1.0.

## v0.2.10.45

- Пакет: `0.2.10.45-eqmod-gem-histogram-ser-metadata`
- Core version: `0.2.10.45`
- Direct EQDrive serial і direct SynScan Wi-Fi використовують coordinate model v7: EQMOD-style механічні HA/Dec pointing states із північним Home `0°,0° = HA -6h / Dec +90° / pier west`.
- v7 прибирає v6-вибір найкоротшої еквівалентної GEM-гілки, який HIL показав як неправильний фізичний pointing state при тому, що serial і UDP transport уже рухалися однаково.
- Histogram assistant працює у фіксованій sensor scale, використовує proportional/damped correction, bright-tail saturation protection і робить не більше одного auto-apply на реально отриманий science frame.
- SER отримує same-basename текстовий metadata sidecar; Live View передає writer-у exposure/gain/offset/bin.
- Зарезервовано OAL/API/UI заглушки filter wheel, rotator, dome/roof, weather, GPS, power, cover/calibrator та safety monitor.

## v0.2.10.44

- Пакет: `0.2.10.44-synscan-wifi-native-parity`
- Core version: `0.2.10.44`
- Прибрано Wi-Fi-специфічний шар polarity Motor Controller, доданий у v0.2.10.43.
- Native serial EQDrive і direct UDP/11880 використовують спільний GOTO-plan та одну механічну інтерпретацію controller counts.
- Додано точні Wi-Fi diagnostics GOTO для кожної осі для HIL-порівняння.

## v0.2.10.43

- Пакет: `0.2.10.43-synscan-wifi-polarity`
- Core version: `0.2.10.43`
- `synscan-wifi` має transport-specific polarity для HIL EQDrive Wi-Fi profile: Axis1 `+1`, Axis2 `-1`; generic Sky-Watcher UDP/11880 лишається `+1/+1`.
- Native serial EQDrive geometry v6 не змінена.
- UDP shutdown не опитує datagrams після закриття socket.


## v0.2.10.42

- Пакет: `0.2.10.42-direct-mc-polar-frame`
- Версія ядра: `0.2.10.42`
- Native serial EQDrive і direct UDP/11880 запам'ятовують controller counts у момент connect як Home/Park поточної сесії; фізична стартова поза є `Axis1=0°, Axis2=0°` без вигаданого 90° DEC-offset.
- ABI direct-MC моделі v6 використовує polar telescope-direction-vector transform і вибирає найкоротшу еквівалентну GEM-гілку.
- Auto-Home direct-MC відновлює валідну sky-модель при connect, тому нормальний запуск не потребує ручного Sync по Полярній.
- Stellarium отримує координати з живих axis counts під час slew.
- High-level SynScan hand-controller/App лишаються RA/DEC-backend'ами; direct-MC модель використовує лише `synscan-wifi`.
- Classic ASCOM за замовчуванням — `EQMOD.Telescope`, site EQMOD використовується як authoritative, якщо доступний.

## v0.2.10.38

- Package: `0.2.10.38-mount-site-shared-park`
- Core version: `0.2.10.38`
- Native GOTO safety працює за реальною кутовою відстанню по небу; raw transport має окремий mechanical cap 180°.
- Classic ASCOM перевіряє та, якщо driver дозволяє, автоматично синхронізує site/time перед GOTO; для EQMOD є явна підказка `Allow Site Writes`.
- ASCOM diagnostics містять driver Az/Alt/LST та явний compatibility fallback `equOther(0)` → topocentric/JNow для EQMOD.
- Persistent Park уніфікований на рівні OAL: native зберігає Home+Park, Classic ASCOM використовує стандартний `SetPark`.
- Remote profile має бажане поле `maxGotoSkyDeltaDeg` і legacy alias `maxGotoAxisDeltaDeg`.

## Hotfix пакета v0.2.10.36.1

- Package: `0.2.10.36.1-gui-qtimer-msvc-hotfix`
- Версія core лишається: `0.2.10.36`
- Виправлення: прибрано некоректний явний виклик Qt 6.10 сигналу `QTimer::timeout()` після запуску таймера годинника монтування.

Англійська документація є канонічною. `PROJECT_MANIFEST_UA.md` — українське дзеркало.

- Package: `0.2.10.36-mount-diagnostics-ser-overlays`
- Core version: `0.2.10.36`
- Основний HIL-фокус: діагностика координат монтування, ASCOM telemetry/site-time, sidereal/lunar/solar tracking, SER і вимірювальні overlays Live View

## v0.2.10.36

- Запис сирого pre-debayer Live View у SER.
- Опційні Mil-Dot та кутова вимірювальна сітки.
- Sidereal/Lunar/Solar tracking для native EQDrive/direct SynScan і Classic ASCOM, де підтримується.
- Налаштовуваний qualification limit raw-axis GOTO.
- Повна діагностика site/time/LST/координат/backend та warning для Sync біля полюса.
- Валідна Classic ASCOM позиція у Stellarium та явне застосування site/time до ASCOM.
- Усунення stale-preview race у remote Live View.


## Previous package history

# Маніфест проєкту — OpenAstroSuite / OpenAstroLink v0.2.10.35.1 hotfix package

Канонічна документація — англійська. `PROJECT_MANIFEST_UA.md` є українським дзеркалом.

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostics: `oal-hardware-probe`
- Core: `oas_core`
- Protocol/native driver framework: `OpenAstroLink (OAL)`
- Package: `0.2.10.35.1-zwo-msvc-hotfix`
- Core version: `0.2.10.35-focus-debayer-coordinates`
- C++20
- Minimum CMake: 3.20
- Preset schema: v2
- Windows toolchain: MSVC 2022 x64 + Ninja
- Qt 6.4+, OpenCV 4
- Native driver ABI: C ABI v2

## v0.2.10.35 — focus preview, optional debayer, synchronized coordinates

- Operational autofocus preview у головній панелі та ручний jog фокусера.
- Scene autofocus із стабільнішою метрикою/вибором піку.
- Preview-only optional debayer: Auto CFA metadata + ручні RGGB/BGGR/GRBG/GBRG; QHY `CAM_COLOR`, ZWO `BayerPattern`; science RAW/FITS не змінюються.
- Синхронні J2000/JNow/Az-Alt/Galactic target fields та Polaris preset.
- Saturation є quality warning, а не camera/transport error.

## v0.2.10.34 — стабільність QHY Live View та health

- Native QHY continuous SDK streaming для Live/Finder.
- Автоматичне повернення QHY у single-frame mode після завершення Live View.
- Health polling не запускається для зайнятих operation-ресурсів; QHY використовує три послідовні idle-probe помилки замість `GetQHYCCDChipInfo`.
- Кешування геометрії сенсора native-камери без зайвих post-readout capability calls.
- Безпечніші денні defaults та діагностика пересвіченого/темного кадру.

## v0.2.10.33 — Live/Finder та scene autofocus

- Безперервний Live View як node-side operation з camera lock і доставкою кадрів remote GUI.
- Вкладка Live / Finder: auto-stretch, crosshair, пошук яскравої області та wizard юстування шукача.
- Scene autofocus з окремими exposure/gain; star autofocus відхиляє поле без достатньої кількості зір.
- RAM-cache preview для remote live-frame fetch.
- Початкова структура `site/` для `openastro.link` (англійська канонічна + українське дзеркало).
- Псевдо-Live View через серійні Canon still-capture заблокований до реалізації EDSDK EVF.

## v0.2.10.32 — safety-фікси hardware HIL

- Фонові liveness-probe та очищення stale connected-state при фізичному від’єднанні QHY, Gemini й EQDrive.
- Core FITS science spool для користувацьких capture, якщо native camera не повертає шлях до оригіналу (QHY).
- Instant ABORT EQDrive з перевіркою фактичної зупинки; explicit disconnect спочатку зупиняє рух.
- Mechanical Park вимагає явно відкалібровану поточну raw-axis позицію; unpark під час паркування викликає фізичний abort.
- Безпечніший Sync UX, Sync з plate solve, кнопки реверсу mapping осей і тимчасовий 15° qualification envelope для sky-GOTO.

## v0.2.10.32 — стабільність adaptive solver / гістограма

- Програмний fallback binning для solver-кадрів, якщо апаратний binning відсутній.
- Обмежена за часом capture-фаза adaptive solve та пауза між Canon-кадрами.
- Підбір наступної витримки за histogram/quality-метриками; gain/ISO лишається явним.
- Опційна гістограма прев’ю у GUI зі статистикою clipping та suggestion/auto-apply витримки.
- Менше повних state broadcast під час progress, щоб HTTP/WS не блокувалися.

## v0.2.10.29 — settle/retry відновлення hot-plug Canon

- HIL EOS 550D підтвердив, що EDSDK `camera-added` може прийти раніше, ніж камера стане видимою через `EdsGetCameraList()`.
- `ApplicationController` debounce-ить callback і повторно сканує тільки `oal.canon` приблизно через 0,7 / 2,2 / 4,5 / 8 с.
- Остання спроба після нульового scan може hard-reload-нути лише неактивний Canon EDSDK driver; QHY та serial drivers цей recovery path не опитує.
- Focused hard-recovery зберігає driver scope, навіть якщо запит стає в чергу за вже активним native discovery.
- Включено виправлення Qt 6.10/MSVC capture-result із v0.2.10.27.

## v0.2.10.26 — автоматичний rediscovery Canon + ISO gain
- EDSDK `camera-added` тепер перетворюється на OAL `device.discoveryHint`, який `ApplicationController` обробляє як асинхронний перепошук лише `oal.canon`.
- Windows Canon OAL `gain` реалізовано як ISO через descriptor `kEdsPropID_ISOSpeed`, який рекламує камера.
- `CameraFrame` містить `scienceFilePath`; operation result та GUI показують шлях до оригінального CR2/CR3.
- Autofocus/plate solve використовують оперативний preview, а RAW залишається science-артефактом.

## v0.2.10.25 — Canon EDSDK capture/event fix

- HIL EOS 550D показав, що EDSDK `0x8D01` є помилкою автофокуса від generic `TakePicture`; Windows astronomy capture тепер використовує non-AF shutter command.
- Короткі requested exposure відображаються в найближче підтримуване `kEdsPropID_Tv`; metadata містить requested та actual exposure.
- Довгі exposure використовують Bulb через `BulbStart`/`BulbEnd` і UI lock/unlock, якщо це підтримується.
- Постійний `EdsGetEvent()` pump обробляє EDSDK object-transfer та camera-added callbacks у процесі node.
- `EdsSetCameraAddedHandler` оновлює внутрішній стан EDSDK при hot-plug; у новішому control plane ця подія запускає bounded автоматичний Canon-only rediscovery.
- Діагностика EDSDK тепер містить символічні назви AF failure та device-busy.

## v0.2.10.22 — build correction

- Виправлено Windows GUI link failure через оголошений, але не визначений `MainWindow::refreshFocuserStatus()`.
- Додано regression-check контракту declaration/definition для GUI.
- Невикористаний future від `QtConcurrent::run()` в `OperationManager` замінено на `QThreadPool::start()`, щоб прибрати C4858.


## macOS build target

- Додані host-native presets для macOS, окремо для Apple Silicon ARM64 та Intel x86_64.
- Canon використовує EDSDK; ZWO ASI/EAF підтримують `mac_arm64`, `mac_x64`/`mac` SDK layouts.
- QHY на macOS опційний до підключення сумісної vendor library.
- Статус: source/configuration-qualified; physical Mac build/HIL pending.

## Native OAL drivers

`oal.qhy`, `oal.canon`, `oal.zwo.asi`, `oal.zwo.eaf`, `oal.gemini`, `oal.skywatcher`, reference `oal.simulated`.

Усі фізичні native drivers ще HIL-pending для конкретних device/firmware/host.

## Compatibility

INDI, ASCOM Alpaca, LX200, OpenCV/UVC і remote OAL adapters збережені. Native OAL пріоритетний; INDI легко вмикається для іншого обладнання.

## Уже реалізовані foundations

- Local/remote GUI, hardware належить node.
- Per-device disconnect і state snapshot/reconnect.
- Operation manager + resource locks.
- Async slew, main/guide exposure, autofocus.
- Main+guide camera roles та optical profiles.
- Native ABI-v2 registry/capabilities/events/cancellation/frame publication.
- Stellarium position/GOTO bridge.
- Cross-platform Windows/Linux/RPi/macOS presets/scripts.

## Основні відкриті задачі

Idempotency; повний RFC 9457 HTTP error model; replayable WebSocket events; TLS/auth/scopes/audit/safety; durable FITS/RAW/SER; production guiding; durable scheduler; driver sandbox/crash recovery; public conformance suite.

## Документація

Головна: `README.md`, `PROJECT_MANIFEST.md`, `docs/*.md`.  
Українські дзеркала: `README_UA.md`, `PROJECT_MANIFEST_UA.md`, `docs/uk/*.md`.  
Специфікація: `docs/OAL_SPECIFICATION.md`.  
Handoff: `docs/NEW_CHAT_HANDOFF.md` і `docs/uk/NEW_CHAT_HANDOFF.md`.


## Windows HIL update v0.2.10.11

- Gemini EAF: native discovery, connection, direct motion і autofocus-driven motion підтверджені на реальному Windows/USB-serial обладнанні.
- Node shutdown: `Ctrl+C` тепер проходить через main-thread graceful shutdown до руйнування Qt event dispatcher; другий `Ctrl+C` залишається force-terminate escape hatch.

- Статус HIL: Gemini EAF пройшов базову Windows-перевірку discovery/connection/motion; інші physical native drivers ще потребують повної HIL-кваліфікації.


## v0.2.10.21 — mount geometry та explicit QHY recovery

- Додано `MountGeometryType`/`MountGeometryModel` у OAL Core: GEM, fork-equatorial, Alt-Az, Alt-Az+derotator, equatorial-platform та custom two-axis.
- Native EQDrive і direct SynScan/EQDrive Wi-Fi використовують спільну Core geometry model замість трактування celestial RA/DEC як motor coordinates.
- Mechanical Home/Park відокремлено від небесних координат; default direct-MC Home/Park Axis1=0°, Axis2=0°, а поточні axes можна зберегти як custom Park у GUI.
- GEM використовує UTC/longitude → local sidereal time → hour angle; pier branch і axis signs є installation config. Automatic meridian-flip planning поки experimental/off.
- Mount status/API/state віддають geometry type і raw Axis1/Axis2, якщо backend їх підтримує.
- Classic ASCOM async slew логуватиме RA/DEC/pier/tracking під час руху для діагностики EQMOD geometry.
- Явний all-native Refresh може hard unload/reload неактивний ABI-v2 QHY driver DLL разом із QHYCCD dependency перед другим scan. Це ніколи не виконується при активній QHY та ніколи не запускається periodic timer.
- Документація: `docs/uk/MOUNT_GEOMETRY.md`.

## v0.2.10.17 — адаптивний plate solving для міського неба

- `solver.adaptive` виконується на node та захоплює серію коротких експозицій замість одного довгого кадру.
- Кадри оцінюються до запуску ASTAP, вирівнюються за зорями, складаються та очищаються від великомасштабного градієнта засвітки.
- Binning тепер зберігається в `CameraFrame`, а ASTAP отримує правильне поле зору для 2x2/іншого binning.
- За наявності реального монтування поточні RA/Dec автоматично використовуються як hint; radius пошуку адаптивно розширюється.
- Додано `POST /api/v1/solve/adaptive` та кнопку **Adaptive urban capture + solve** у GUI.

## v0.2.10.17 HIL status/recovery
- підтверджено graceful Ctrl+C shutdown;
- Stellarium bridge приймає GOTO та передає RA/Dec у node;
- QHY auto-connect retry тепер виконує повторний native discovery;
- Gemini move не блокує status до завершення руху; autofocus чекає фізичний idle;
- додано live GUI polling position/moving;
- Sky-Watcher native discovery має детальний SynScan serial handshake log і same-session retry.
- Важливо: native RA/DEC Sky-Watcher device наразі підтримує SynScan hand-controller protocol. Direct USB/EQDIR motor-controller path ще не експонується як повний mount backend.


## v0.2.10.17 — сумісність mount

- Native ABI-v2 driver `oal.eqdrive` збережено окремо від `oal.skywatcher`; тепер спочатку пробується EQMOD-сумісний Sky-Watcher Motor Controller serial transport, а публічний EQDrive ASTEP використовується як fallback.
- Native EQDrive/direct Wi-Fi віддають raw mechanical axes у OAL Core. `MountGeometryModel` виконує J2000/JNow ↔ hour-angle/Alt-Az ↔ mechanical-axis conversion для GEM/fork/Alt-Az; один Sync на відому точку визначає encoder offsets/signs установки.
- Windows backend `ascom-classic` виконує COM automation в окремому native `oas-ascom-host.exe`; локальний GUI має ASCOM Chooser та setup dialog.
- `synscan-wifi` напряму працює з mount/EQDrive Wi-Fi adapter через Motor Controller protocol UDP 11880; `synscan-app` реалізує SynScan App Protocol до запущеного SynScan Pro через UDP 11881.
- Native serial discovery включає EQDrive та міграцію persisted port binding; новий вибір Gemini COM оновлює збережений native backend після успішного rediscovery.
- Документація: `docs/uk/EQDRIVE.md`, `docs/uk/ASCOM_CLASSIC.md`, `docs/uk/SYNSCAN_NETWORK.md`.


## v0.2.10.17 HIL-надійність камери/монтування/мережі

- QHY single-frame readout дотримується vendor lifecycle Exp/Get/Cancel, має watchdog скасування, readback параметрів SDK та логування статистики кадру.
- Native reconnect discovery фільтрується за відсутнім драйвером, тому активна QHY-сесія й COM, зайнятий EQMOD/ASCOM, не зачіпаються через інший відсутній пристрій.
- SynScan endpoint-и зберігаються окремо: direct `synscan-wifi` auto-discover-ить adapter на UDP 11880, а `synscan-app` auto-discover-ить хост SynScan Pro на UDP 11881; TCP 11882 лишається app-host compatibility service.
- Classic ASCOM GOTO логує координати та pier-side перед рухом; інверсія RA/DEC не додається.
- Focused EQDrive diagnostics охоплює всі DTR/RTS комбінації та кілька read-only ASTEP запитів.
- Застарілий COM binding Gemini може автоматично мігрувати після однозначного rediscovery, а ручний вибір COM refresh-ить лише Gemini.

## v0.2.10.20 — ручний discovery / J2000 / live native catalogue

- Backend catalogue передається у node state, тому remote GUI оновлює списки після native hot-plug/discovery.
- `synscan-wifi` — direct Motor Controller UDP/11880 до mount/EQDrive Wi-Fi adapter; `synscan-app` лишається UDP/11881 до SynScan Pro.
- `oal.eqdrive` тепер dual-protocol: спочатку EQMOD-сумісний Sky-Watcher Motor Controller через serial, потім офіційний EQDrive ASTEP fallback; наявний `oal.skywatcher` збережено. Direct `synscan-wifi` використовує той самий Motor Controller command set через UDP 11880.
- Native EQDrive GOTO потребує Sync і має HIL safety limit; park/pier/pulse-guide ще не кваліфіковані.

### Політика систем координат

- Канонічні екваторіальні координати OAL — **J2000**.
- Mount GOTO/Sync і session targets приймають `coordinateFrame: J2000|JNOW`; якщо поле відсутнє, використовується J2000.
- Поточна підтримка JNow/of-date виконує прецесію між J2000 і середнім екватором/рівноденням дати; нутація, річна аберація, атмосферна рефракція та повна topocentric apparent-place модель ще не входять у це перетворення.
- Classic ASCOM читає `EquatorialSystem` і виконує адаптацію на compatibility boundary; Stellarium трактується як J2000 catalogue interface.


### v0.2.10.29 — виправлення capture після hot-plug
Hard-recovery Canon повинен ініціалізувати EDSDK на довгоживучому Qt event-loop потоці застосунку. Не переносити Canon `restartDriver()` назад у короткоживучий native-discovery worker: у такому випадку enumeration і команди працюють, але після hot-plug губляться EOS object-transfer callback-и.

## v0.2.10.32 — фіналізація hardware control

- Canon EDSDK hot-remove: per-camera `kEdsStateEvent_Shutdown`, негайний `DEVICE_DISCONNECTED`, пробудження/скасування pending exposure, видалення активного native binding у controller зі збереженням auto-connect для наступного hot-add.
- Native long GOTO: тимчасовий HIL-envelope 15° знято. Raw/native GOTO використовує shortest-axis envelope до 180°; automatic meridian flip лишається вимкненим, а перші довгі переходи треба HIL-тестувати під наглядом.
- Manual mount control: двовісний OAL/REST/remote API зі швидкостями hand-controller 1..9; у GUI press-and-hold 3×3 pad; реалізація для native EQDrive, native SynScan, direct SynScan/EQDrive Wi-Fi та Classic ASCOM MoveAxis.
- Stellarium: immediate position packet при підключенні клієнта та live J2000 position stream кожні 500 мс. Raw EQDrive потребує одного Sync до появи валідних sky coordinates.

- Remote capture transport зберігає `saveRaw` / `savePath`, тому QHY FITS science spool працює і через remote GUI, а не лише in-process.

### Build infrastructure follow-up — build-fix10

- Виправлено припущення щодо QHY ARM ABI: фізична перевірка показала, що legacy `QHYCCD_Linux_New` `armv8` SDK є 32-bit ARM/EABI, а не AArch64.
- QHY cross staging тепер вибирає library за реальною ELF-архітектурою, підтримує прямий шлях до SDK archive, дозволяє ARM64 bootstrap без QHY за замовчуванням і має `--require-qhy` для strict full-vendor gate.
- Legacy QHY `armv8` archive тепер свідомо підтримується ARMHF staging path.


### Продовження build infrastructure — build-fix12

- Реальна AArch64 компіляція після bootstrap/sysroot fixes дійшла до source OAL.
- Додано локальну GCC-сумісність Linux headers Canon EDSDK (`__int64`, `WCHAR`) без патчу vendor files.
- Додано сумісність Qt HttpServer bind для Qt 6.4-6.7 (`void`) та Qt 6.8+ (`bool`).
- Додано `tests/linux_arm_portability_smoke.py`.
- Геометрія mount v9 не змінювалась.


### Продовження build infrastructure — build-fix13

- Реальна WSL -> AArch64 збірка доходить до 100% компіляції source; залишався лише фінальний link executable.
- Linker failure локалізовано у discovery транзитивних залежностей foreign sysroot: OpenCV/Armadillo/ARPACK/SuperLU потребують BLAS/LAPACK та Bookworm libc, але GNU ld не шукав `DT_NEEDED` closure у target multiarch/runtime directories.
- Cross-build тепер додає лише на link-time target-sysroot `-rpath-link` та `-L` для `/lib/<multiarch>`, `/usr/lib/<multiarch>`, `/lib`, `/usr/lib`. Install/runtime RPATH не додається.
- Raspberry Pi bootstrap явно встановлює та перевіряє `libblas.so.3` і `liblapack.so.3`.
- Додано `tests/cross_linker_sysroot_smoke.py`.
- Геометрія/runtime mount v9 не змінювались.

### Продовження build infrastructure — build-fix14
- Додано fetch/cache/staging офіційного QHYCCD 26.x Linux ARM64 SDK (`scripts/fetch_qhy_sdk.sh`).
- Від 26.06.04 підтримується нове пакування `sdk_linux_arm64_<version>.tar.gz`.
- QHY staging перевіряє реальну ELF-архітектуру та підтримує shared або static `libqhyccd.a`.
- `bootstrap_rpi_cross.sh --qhy-version 26.06.04 --require-qhy` вмикає повний ARM64 QHY path.
- Геометрія/runtime mount v9 не змінювались.

### Продовження build infrastructure — build-fix15

- Фізична WSL -> AArch64 перевірка підтвердила: офіційний QHYCCD SDK 26.06.04 завантажується/stage-иться як справжня ARM64 shared library, а `oal_driver_qhy.so` успішно збирається.
- Залишковий failure ізольовано у резолві Debian Bookworm BLAS/LAPACK alternatives на фінальному executable link.
- Нормалізація symlink у sysroot тепер запускається як root, тому absolute alternatives links реально переписуються замість `Permission denied`.
- Cross link search включає `usr/lib/<multiarch>/blas` та `usr/lib/<multiarch>/lapack`.
- Target `liblapack.so.3` і `libblas.so.3` перевіряються за ELF-архітектурою та явно лінкуються після OpenCV, закриваючи числовий dependency chain Armadillo/ARPACK/SuperLU/OpenCV.
- Runtime sysroot RPATH не вбудовується; mount v9 лишається frozen і не змінювався.

### build-fix16 delta
- Додано відтворюваний bootstrap нативних vendor SDK: офіційний QHYCCD для Linux/macOS/Windows x64 та pinned INDI-mirror ZWO ASI/EAF для Linux/macOS.
- Build scripts можуть використовувати staged SDK без ручного редагування глобальних шляхів системи.
- Додано `rpi4-cross-arm64-observatory-release` та Windows-host аналог з `OAS_BUILD_GUI=ON`; node-only cross presets навмисно лишаються headless.
- Геометрію/керування mount v9 не змінено.


### build-fix17 delta
- Фізично підтверджено 100% WSL -> AArch64 full-native build node/probe з QHYCCD 26.06.04, Canon EDSDK і ZWO ASI/EAF; BLAS/LAPACK closure із build-fix15 валідований.
- INDI compatibility тепер глобально opt-in: `OAS_ENABLE_INDI=OFF` за замовчуванням, звичайні observatory/node presets лишаються native-first.
- Явні `*-indi-release` presets зберігають compatibility для INDI-only обладнання без зміни пріоритету native drivers.
- Локальні user presets за замовчуванням native OAL і мають окремі opt-in INDI варіанти для Windows/Linux/macOS.
- Геометрія/керування mount v9 не змінювались.

### build-fix21 delta
- Виправлено UX bootstrap нативного Linux на Jammy: канонічний шлях `build_linux.sh --bootstrap-deps` ставить per-user Qt 6.8.3 через aqtinstall, якщо distro Qt має лише 6.2.4.
- Linux build wrapper визначає та видаляє застарілий `CMakeCache.txt`, скопійований між WSL `/mnt/c/...` і нативним Linux checkout `~/...`; також додано явний `--clean`.
- Відсутній/застарілий `OAS_QT_ROOT` більше не блокує пошук іншого валідного managed/user Qt.
- Linux example/user presets більше не hard-code-ять ще не встановлений Qt prefix і використовують relocatable sibling `CANON_EDSDK_ROOT=${sourceDir}/../edsdk` замість shell `~` paths.
- CMake error для Jammy Qt тепер прямо вказує команду автоматичного bootstrap і пояснює, що встановлення додаткових Jammy Qt 6.2.4 dev packages не може дати Qt HttpServer/Qt >= 6.4.
- Геометрія/керування mount v9 не змінювались.
