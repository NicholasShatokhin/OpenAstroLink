# Маніфест проєкту — OpenAstroSuite / OpenAstroLink v0.2.10.44

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
- Cross-platform Windows/Linux/RPi presets/scripts.

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
