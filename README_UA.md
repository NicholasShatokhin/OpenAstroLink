# OpenAstroSuite / OpenAstroLink

## v0.2.10.49 — persistent календар спостережень, мозаїки та опційно обмежений рух Polar Alignment

- Кожен `ObservationBlock` тепер має власний `startAtUtc`, тому один календар node може містити незалежні події на місяці або рік уперед, а не один час старту на весь план.
- Календар, armed-state і cursor наступного блока зберігаються на node. Restart між блоками продовжує з першого незавершеного блока; restart посеред блока запускає цей блок заново. Resume всередині FITS/SER лишається завданням OAL 1.0.
- Кожен block має `parkAfter` та `autoUnparkBefore`: телескоп може ставати в Park між віддаленими подіями й автоматично прокидатися перед наступною.
- Додано `mosaic-fits`: scheduler обчислює центри тайлів із optical profile/FOV головного сенсора, rows/columns, overlap та rotation, проходить їх serpentine і застосовує звичайні DSO slew/solve/recenter/autofocus/FITS policies до кожного тайла.
- Рух Polar Alignment по RA-offset тепер можна обмежити дозволеною Az/Alt ділянкою. OAL перевіряє весь шлях slew і відхиляє його, якщо будь-яка проміжна точка виходить за safe region.

## v0.2.10.48 — безпечний Scene AF, збіжна експозиція та lifecycle scheduler

- Етап HIL монтування закрито: native EQDrive, direct SynScan/EQDrive Wi-Fi та Classic ASCOM/EQMOD на тесті 2026-09-02 наводяться однаково правильно з direct-MC coordinate model v9 (`Axis1=+1`, `Axis2=-1`).
- Scene autofocus тепер окремо підбирає AF-витримку, пробує обидва напрями та bracket'ить локальний максимум контрасту, а при ненадійному/невдалому фокусуванні **повертає фокусер у стартову позицію**, замість залишати телескоп у кінці сліпого sweep.
- Remote 16-bit preview використовує ту саму фіксовану шкалу сенсора, що й node: пересвічені AF-кадри більше не повинні перетворюватися на чорний екран, а histogram Auto не втрачає абсолютну яскравість. Auto exposure має explicit convergence/lock і розблоковується лише після стійкої зміни освітлення.
- Scheduler має camera-resource preflight: активний інтерактивний Live View спочатку зупиняється/фіналізується, і лише потім починається автономна зйомка. GUI дозволяє оновлювати, видаляти, очищати й переставляти блоки, копіювати поточний напрям монтування в ціль та стартувати зараз або у вибрані дату/час. Відкладений старт у v0.2.10.48 зберігається в пам'яті; durable restart/resume лишається планом OAL 1.0.


## v0.2.10.47 build-fix 3 — виправлення дзеркальності DEC/polar-distance direct-MC

- Повторний HIL 2026-09-02 показав точне дзеркало схід↔захід навіть після гіпотези build-fix 2 про Axis1. Перерахунок записаної цілі за raw encoder deltas для всіх чотирьох комбінацій знаків показує: інверсія Axis1 не може створити таке дзеркало, а DEC/polar-distance mapping — може.
- Coordinate model v9 зберігає v7 EQMOD HA/Dec branch formulas і використовує HIL-qualified physical mapping direct-MC `Axis1=+1`, `Axis2=-1`. Для записаної цілі потрібен raw рух приблизно `Axis1=-11.116°`, `Axis2=+67.847°`; спростована v8-команда `+11.116°,-67.847°` відповідає спостережуваному дзеркальному наведенню на схід.
- Serial EQDrive і UDP/11880 SynScan Wi-Fi лишаються transport peers. Існуючі v7/v8 profiles мігрують автоматично без втрати валідних Home/Park; профілі старші за v7 і далі проходять legacy one-time Home/Park reset.

## v0.2.10.47 — змішаний DSO + автономний planetary SER executor

- `ObservationPlan` тепер виконує в одному node-side плані і `dso-fits`, і `planetary-ser` blocks.
- Планетарний сценарій: **GOTO → full-frame acquisition/detection → опційний planetary autofocus → reacquisition → hardware ROI → finite SER**.
- Швидкий tracker може рухати ROI незмінного розміру без руху монтування. Кожна зміна origin ROI записується поруч із SER у `<basename>.roi.jsonl`; людський `.txt` sidecar також зберігається.
- Опційний повільний mount loop сам калібрує орієнтацію камери малими RA/DEC наведеннями, інвертує виміряну 2×2 pixel-response matrix і коригує великий дрейф. За замовчуванням він вимкнений і потребує HIL-кваліфікації для конкретного mount/backend.
- Native QHY і ZWO ASI live paths приймають hardware ROI. ZWO ASI реалізований, але HIL на реальному залізі ще не виконаний.
- Durable restart scheduler, weather/roof safety, meridian-flip recovery та thermal focus compensation під час експозиції лишаються roadmap OAL 1.0.

**Поточний пакет: v0.2.10.49 — persistent per-block календар, DSO/planetary/mosaic execution, опційно обмежений рух Polar Alignment + HIL-кваліфікований direct-MC v9 mapping**

## v0.2.10.46 — ObservationPlan та supervised DSO executor

- Основну flat-модель `SessionTarget` замінено на `ObservationPlan` / `ObservationBlock`; legacy session targets лишилися API compatibility wrapper.
- Додано перший реальний node-side DSO workflow executor: **slew → adaptive plate solve/recenter → autofocus → FITS/RAW science capture × N**. Усі довгі дії використовують наявні `OperationManager` resource locks.
- Recenter можна запускати перед першим кадром і кожні N science frames, із tolerance в arcmin та retry limit. Використовується solved field center, `Sync`, correction slew та повторний solve до збіжності. Autofocus має аналогічну before-first/every-N policy.
- Session telemetry тепер показує active block, step, operation ID, per-block/global progress і failure reason. GUI створює DSO blocks напряму.
- `planetary-ser` вже є у plan/API model, але autonomous ROI/centroid/SER-run execution навмисно лишено наступним scheduler-етапом. Restart durability, meridian/weather recovery та unattended safety — робота OAL 1.0.

**Попередній пакет: v0.2.10.46 — supervised DSO scheduler executor**

## v0.2.10.45 — EQMOD GEM geometry, histogram control, SER metadata та observatory stubs

- Direct Motor Controller model v6 із вільним вибором «найкоротшої» еквівалентної GEM-гілки замінено на **v7 EQMOD-style механічну модель HA/Dec та фізичних pointing states**. Для північної установки Home лишається `Axis1=0°, Axis2=0°`, але явно означає `HA=-6h, Dec=+90°`, `pier=west`; гілка цілі визначається hour angle, а не мінімальною математичною дельтою. Native EQDrive serial і direct SynScan Wi-Fi й далі використовують одну й ту саму Core geometry та GOTO plan.
- Виправлено histogram auto exposure для 16-bit камер: preview conversion тепер зберігає фіксовану шкалу сенсора 0..65535 замість min/max normalization кожного кадру. Корекція стала proportional/damped, має P99 highlight ceiling і контроль saturation; Live View більше не накопичує багато auto-correction для фактично незміненої експозиції потоку.
- Поруч із кожним SER тепер фіналізується FireCapture-style `.txt` з тією самою базовою назвою: requested/actual exposure, gain, offset, binning, FPS, raw format/CFA, optical profile, site і UTC metadata. У Live View додано offset.
- Зарезервовано first-class OAL placeholders для filter wheel, rotator, dome/roof, weather, GPS/GNSS, power/switch, cover/calibrator і safety monitor. Поки це навмисно неактивні заглушки без backend.

**Попередній пакет: v0.2.10.45 — EQMOD GEM geometry / capture metadata HIL follow-up**

## v0.2.10.44 — виправлення паритету SynScan Wi-Fi/native Motor Controller

- Прибрано Wi-Fi-специфічний шар polarity з v0.2.10.43. UDP/11880 і нативний EQDrive через serial звертаються до тих самих осей Motor Controller, тому однакові controller counts тепер мають однаковий механічний зміст на обох transport.
- Serial і Wi-Fi GOTO тепер використовують спільний `skywatcher_mc::makeGotoPlan()` для перетворення механічної дельти на напрямок, кількість кроків і brake count. Відрізняється лише I/O transport.
- Додано HIL diagnostics Wi-Fi: остання дельта кожної осі, raw count increment, forward/reverse та firmware payload обох осей.
- Збережено захист UDP shutdown із v0.2.10.43.

**Попередній пакет: v0.2.10.44 — SynScan Wi-Fi/native Motor Controller parity fix**

## v0.2.10.43 — hotfix polarity DEC для SynScan Wi-Fi

- Геометрія v6 у native `oal.eqdrive` не змінюється: serial HIL вже наводить правильно.
- Direct `synscan-wifi` тепер відокремлює **polarity controller counts transport-рівня** від спільних axis signs sky-геометрії. Для поточного EQDrive Wi-Fi HIL-профілю (`9216000` counts/rev на обох осях, timer `53694`) застосовується `Axis1=+1`, `Axis2=-1`. Це відповідає фактичному напрямку DEC і не повертає вигадану 90° поправку в небесну модель.
- Для звичайного Sky-Watcher UDP/11880 лишається стандарт `+1/+1`, бо офіційний протокол визначає Wi-Fi як той самий Motor Controller command set, що й serial. Обидва signs можна явно перевизначити через `OAL_SYNSCAN_WIFI_AXIS1_SIGN` / `OAL_SYNSCAN_WIFI_AXIS2_SIGN`.
- GOTO, manual slew, tracking direction та live axis decoding використовують одну й ту саму transport polarity, тому фізичний рух і координати в status/Stellarium більше не розходяться. Park як і раніше повертає до startup Home/Park counts.
- UDP disconnect захищено від виклику `QUdpSocket::hasPendingDatagrams()` після виходу socket зі стану `BoundState`.

**Попередній пакет: v0.2.10.43 — SynScan Wi-Fi DEC transport polarity hotfix**

## v0.2.10.42 — полярна telescope-frame геометрія direct-MC

- Native `oal.eqdrive` serial та direct `synscan-wifi` UDP/11880 використовують **фактичні Motor Controller counts у момент connect** як фізичний session Home/Park. Ця стартова поза показується як `Axis1=0°, Axis2=0°`; жодної вигаданої фіксованої 90° DEC-поправки немає.
- Виправлено залишкову помилку наведення east/west. Coordinate-model ABI **v6** тепер використовує перевірену SkyWatcher/INDI telescope-direction-vector геометрію: JNow sky -> horizontal direction vector -> polar-aligned mount frame. `Axis1` — азимут у полярній системі монтування, `Axis2` — signed polar distance. Це замінює помилкове v5 припущення `H+90°`.
- Обидві еквівалентні GEM-гілки як і раніше перевіряються, і обирається та, що потребує найкоротшого фізичного руху контролера. HIL-регресія 2026-08-31, де східна ціль біля Денеба відправляла трубу на північ, тепер мапиться на близьку flipped-гілку.
- Точне обернене перетворення виконується для кожного live encoder sample, тому Stellarium має бачити RA/DEC, що змінюються протягом native slew.
- Стартовий direct-MC Home автоматично відновлює валідну sky model; ручний Sync на Polaris для звичайного старту не потрібен. Plate-solve Sync лишається як пізніше уточнення pointing.
- High-level SynScan backends лишаються окремими: `oal.skywatcher` hand-controller і `synscan-app` використовують власну RA/DEC alignment model SynScan. Лише direct Motor Controller Wi-Fi (`synscan-wifi`, UDP/11880) ділить v6 із native EQDrive serial.
- Classic ASCOM як і раніше має default endpoint `EQMOD.Telescope`, а валідний EQMOD site за замовчуванням є authoritative.


## v0.2.10.38 — нормалізація site для монтування, sky-angle safety та спільний persistent Park

- Виправлено safety semantics native GOTO біля полюса: користувацький ліміт тепер означає **реальну кутову відстань по небу**, а не raw-обертання моторної RA/DEC осі. Поблизу небесного полюса зміщення на 3–5° по небу може коректно вимагати >100° обертання RA; transport має окремий mechanical hard cap 180°. `maxGotoSkyDeltaDeg` — нова бажана назва API, `maxGotoAxisDeltaDeg` лишається compatibility alias.
- Classic ASCOM автоматично порівнює власні `SiteLatitude/SiteLongitude` з observatory profile OAL при connect і перед кожним GOTO. Якщо driver дозволяє site writes, OAL автоматично передає site/time. Якщо EQMOD блокує запис, GOTO не виконується і лог прямо просить увімкнути **ASCOM Options → Allow Site Writes** або задати ті самі координати в EQMOD Properties.
- У ASCOM diagnostics додані Azimuth, Altitude та SiderealTime самого driver поряд із RA/DEC, pier side, tracking, site та UTC — East/West mismatch тепер видно без здогадок.
- `EquatorialSystem` декодується за ASCOM (`Topocentric=1`, `J2000=2`). Старий EQMOD часто рекламує `equOther=0`; для `EQMOD.*` OAL явно застосовує compatibility assumption topocentric/JNow і пише це в diagnostics. Все одно рекомендовано явно вибрати JNOW або J2000 у EQMOD Setup.
- Додано backend-neutral дію **Calibrate current physical pose as persistent Home / Park**. Native raw-axis зберігає цю позу як OAL Home+Park і вмикає auto-Home restore; Classic ASCOM використовує стандартні `CanSetPark/SetPark`. Достатньо один раз відкалібрувати кожен backend у тій самій фізичній позі, не рухаючи телескоп між перемиканнями; надалі Park зберігається.
- Інверсія Axis1/Axis2 явно native-only і блокується для Classic ASCOM, бо EQMOD/ASCOM сам володіє motor↔sky transform.
- Лишається поведінка v0.37: repeatable Home відновлює native sky model при connect, operational profile changes не скидають Sync, GOTO може auto-unpark, Mount tab має вертикальний scroll.

**Поточний пакет: v0.2.10.38 — site normalization, sky-angle GOTO safety, спільний persistent Home/Park**

## v0.2.10.36.1 package hotfix — Qt 6.10 QTimer signal call

**Hotfix пакета:** виправляє Windows Qt 6.10/MSVC build failure через явний виклик `QTimer::timeout()`. Версія core/protocol лишається `0.2.10.36`; runtime-поведінка та схема протоколу не змінювались.


## v0.2.10.36 — діагностика монтування, SER та tracking-rate HIL

- Live/Finder може опційно записувати сирий потік до SER до debayer/stretch, тому science Bayer/mono пікселі не змінюються.
- Додано опційні Mil-Dot та кутову вимірювальну сітки в Live View.
- Tracking тепер має Sidereal, Lunar і Solar режими. Для утримання Місяця першим треба тестувати Lunar; Sidereal веде зоряне небо, а не сам Місяць.
- Ліміт raw-axis GOTO native EQDrive зроблено налаштовуваним 0.1–180° і винесено на Mount. Консервативний default 15° лишається, доки знаки/геометрія установки не кваліфіковані.
- GOTO/Sync/connect/tracking пишуть у лог UTC, local time, site, LST, J2000, JNow, Az/Alt, реальну кутову відстань по небу, pier side, raw axes та backend diagnostics. Sync поблизу полюса окремо попереджає, що RA там погано обумовлена.
- Classic ASCOM RA/DEC тепер позначаються як валідні для live-позиції у Stellarium. Додано діагностику ASCOM site/time/alignment/tracking-rate та кнопку явного застосування OAL site/UTC до writable ASCOM driver.
- Remote Live View при 10 fps бере останній доступний preview замість застарілих frame ID, усуваючи race з preview-cache.



## v0.2.10.35.1 — MSVC build hotfix для ZWO ASI

- Виправлено C++ most-vexing-parse у новому ZWO ASI Live View: MSVC трактував `std::vector<unsigned char> buffer(size_t(bytes));` як декларацію функції, тому повідомлення про `.data()` та кількість аргументів `ASIGetVideoData` були вторинними й оманливими.
- Буфер Live View тепер створюється однозначно через default construction + `resize(static_cast<std::size_t>(bytes))`.
- Додано `tools/zwo_asi_sdk_compile_check.py`, який реально syntax-компілює весь native ZWO ASI driver проти API-shape stub із чотириаргументним контрактом `ASIGetVideoData`.

## v0.2.10.35 — preview автофокуса, опційний debayer та синхронні координати цілі

- Під час autofocus основна панель зображення отримує operational preview для кожної позиції фокусера. На вкладку Focus додано ручний jog `-- / - / STOP / + / ++` із налаштовуваним кроком.
- Scene autofocus використовує метрику без нормалізації кожного кадру min/max, відкидає пласкі криві, розширює coarse scan якщо максимум потрапив на край і не замінює сильний coarse-пік слабшим fine-піком.
- Debayer у Live View опційний і працює лише для preview. `AUTO` бере CFA-патерн із metadata драйвера; RGGB/BGGR/GRBG/GBRG можна задати вручну для будь-якого одноканального Bayer-джерела. QHY читає Bayer sequence через SDK `CAM_COLOR`, ZWO ASI — через `BayerPattern`. Science FITS/RAW не змінюється.
- Пересвічений або недоекспонований кадр вважається валідним кадром; це лише warning якості експозиції, а не помилка камери/USB.
- На Mount додані синхронні editable-поля J2000, JNow, Az/Alt і Galactic l/b. Зміна будь-якої системи перераховує всі інші для поточного UTC та координат обсерваторії; GOTO/Sync всередині канонічно використовують J2000.
- Додано preset Полярної та явний workflow Park → відома небесна точка/plate solve → Sync → Stellarium/GOTO. Mechanical Park сам по собі не є sky Sync.

## v0.2.10.34 — стабілізація QHY Live View та false-disconnect fix

- QHY Live View тепер використовує справжній continuous-stream шлях SDK (`SetQHYCCDStreamMode(...,1)` / `BeginQHYCCDLive` / `GetQHYCCDLiveFrame` / `StopQHYCCDLive`) замість псевдовідео через серійні single-frame exposure. Після Stop драйвер перевідкриває камеру у single-frame mode, тому звичайний Capture/Autofocus має працювати одразу після Live View.
- Фоновий health polling тепер пропускає ресурси, зайняті operation. Для QHY прибрано періодичний `GetQHYCCDChipInfo`; використовується легший idle-only probe, а `device.disconnected` генерується лише після трьох послідовних помилок, щоб transient SDK failure не виглядав як фізичне USB-від'єднання.
- Геометрія сенсора native-камери кешується під час connect, тому після кожного readout більше немає зайвого vendor capability call.
- Денний стартовий профіль Live/Finder: 1 ms, gain 0, 2x2, 5 fps. GUI окремо попереджає, якщо кадр майже повністю білий або чорний.
- Скасування QHY Live View більше не викликає single-frame `camera.abortExposure`; stream сам виходить із циклу та коректно виконує `StopQHYCCDLive`.
- Remote GUI тепер передає `saveRaw` / `savePath` через HTTP, тому звичайний користувацький QHY Capture на віддаленій node знову створює FITS. Live View та autofocus кадри залишаються preview-only і на диск автоматично не пишуться.

## v0.2.10.33 — Live View, Scene Autofocus та юстування шукача

- Додано node-local operation `camera.live-view`, доступну через OAL HTTP/WebSocket. Вона безперервно отримує короткі preview-кадри під camera lock і ніколи не зберігає їх як science-файли. Типові GUI-параметри: 50 ms, 2x2 bin, 5 fps для QHY/ASI-подібних камер.
- Додано вкладку **Live / Finder** з auto-stretch, центральним перехрестям, підсвічуванням найяскравішої області та показом її зміщення від центра в пікселях.
- Додано п'ятикроковий **Finder Alignment wizard**: далекий денний об'єкт -> Scene autofocus -> центрування головної труби -> регулювання шукача -> перевірка.
- Додано **Scene autofocus** для наземних, місячних і планетних структур. Він використовує градієнтну/edge-energy метрику та окремі exposure/gain параметри, тому зорі не потрібні.
- Star autofocus тепер має мінімальну кількість зір і повертає `No suitable stars detected`, а не вибирає максимум шуму при порожньому полі.
- Для remote Live View додано невеликий RAM-cache preview-кадрів, щоб GUI не втрачав кадри через гонку з наступним frameReady.
- Серійні DSLR-знімки Canon не використовуються як псевдо-Live View, щоб не зношувати затвор. Для Canon потрібен окремий EDSDK EVF transport.
- Додано початковий статичний сайт у `site/` для `openastro.link`: англійська канонічна сторінка та українське дзеркало. DNS/deploy застосунок не змінює.

## v0.2.10.32 — safety-first HIL-фікси обладнання

- Для активних native QHY, Gemini EAF та EQDrive додано легкі фонові health-probe. Фізичне від’єднання USB/serial тепер генерує `device.disconnected`, скасовує operation, прибирає stale connected-об’єкт зі state та оновлює каталог лише відповідного драйвера. Persisted binding для повторного auto-connect зберігається.
- Звичайний користувацький Capture тепер просить зберігати science-файл. Для камер, що публікують лише host-frame (зокрема QHY), OAL Core записує FITS у `Pictures/OpenAstroLink/<VENDOR>/`; Canon як і раніше зберігає native CR2/CR3. Тимчасові кадри autofocus/adaptive solve автоматично не пишуться.
- EQDrive `ABORT` використовує instant-stop Motor Controller (`:L`) із fallback на звичайний stop та перевіряє фактичну зупинку обох осей. Explicit disconnect також спершу намагається зупинити рух.
- Mechanical Park для native mount вимкнений, доки користувач явно не збереже поточні безпечні фізичні осі як Park. Зняття прапорця Park під час паркування тепер викликає фізичний abort. Небезпечну кнопку відновлення універсального `90°,0°` прибрано.
- UI Sync прямо пояснює, що телескоп вже має фізично дивитися у введені координати; додано `Sync mount to last successful plate solve` і швидкі кнопки реверсу Axis 1/Axis 2 для HIL-калібрування.
- Після першого long-slew HIL, який показав некваліфіковану орієнтацію осей конкретної установки, тимчасово повернуто 15° qualification envelope для sky-GOTO. Mechanical Park має окрему явно відкалібровану raw-axis ціль.

HIL EOS 550D показав, що EDSDK може надіслати `camera-added` раніше, ніж камера з’явиться у `EdsGetCameraList()`. Тому v0.2.10.29 замінює миттєвий одноразовий scan на обмежену debounced-послідовність повторів лише для Canon. Якщо звичайні повтори й далі бачать нуль пристроїв, останній fallback перезавантажує тільки неактивний `oal.canon` EDSDK driver і сканує його знову. Періодичного vendor polling немає. ISO/gain, Bulb, повнорозмірний operational preview та збереження CR2/CR3 не змінені.

Англійська документація є канонічною. Українські дзеркала знаходяться у `README_UA.md` та `docs/uk/`.

OpenAstroLink (OAL) — local-first стек керування обсерваторією. Основний шлях до обладнання — **нативні OAL-драйвери**; INDI, ASCOM Alpaca та LX200 залишаються опційними шарами сумісності для обладнання, для якого ще немає native OAL driver.




## v0.2.10.30 — стабільність adaptive solve + помічник гістограми

- Adaptive plate solving тепер гарантовано застосовує запитаний bin програмно, якщо камера (зокрема Canon DSLR/EDSDK) не підтримує апаратний binning. Canon-кадр 5184×3456 при запиті 2×2 одразу зменшується приблизно до 2592×1728 ще до видалення градієнта, реєстрації зір і stack, що різко зменшує пікове навантаження RAM/CPU.
- Capture-фаза adaptive solve має обмежений wall-clock budget (`maxCapturePhaseSec`, типово 120 с) і невелику паузу між серійними Canon-кадрами.
- Зміна витримки adaptive solve тепер враховує якість кадру: фон, p99, частку saturation та кількість знайдених зір. ISO/gain автоматично не змінюється.
- Progress operation більше не спричиняє повну перебудову й broadcast hardware state на кожному tick; повний state надсилається на старті та при завершенні, щоб HTTP/WebSocket залишалися чутливими під час важкої обробки.
- На вкладці Capture & Solve з’явилася опційна логарифмічна гістограма прев’ю. Вона показує P1/median/P99 та clipping, пропонує наступну витримку під заданий рівень фону й може автоматично застосувати її до наступного кадру, не змінюючи ISO/gain. Для Canon це гістограма embedded JPEG, тобто практичний operational-помічник, а не лінійна RAW-фотометрія.

## v0.2.10.29 — settle/retry відновлення hot-plug Canon

- `device.discoveryHint` Canon запускає debounced перепошук лише `oal.canon` приблизно через 0,7 / 2,2 / 4,5 / 8 с після останньої hot-plug події.
- Новіша hot-plug подія скасовує попередню retry-серію; після появи Canon у cache решта спроб нічого не робить.
- Остання спроба після нульового scan може hard-reload-нути тільки неактивний `oal.canon`, не торкаючись QHY та serial drivers.
- Focused hard-recovery тепер не губить selected driver scope, навіть якщо в момент запиту вже триває інший native discovery.
- Збережено виправлення Qt 6.10/MSVC `QJsonValue` із v0.2.10.27 та всю поведінку ISO/science-file з v0.2.10.26.

## v0.2.10.26 — автоматичний rediscovery Canon та ISO gain

- EDSDK `camera-added` надсилає `device.discoveryHint`; core асинхронно перепошукує лише `oal.canon`, після чого node використовує звичайний persisted auto-connect restore.
- OAL `gain` у Windows Canon тепер означає ISO. `0` залишає поточне ISO камери, додатне значення мапиться на найближче ISO, яке рекламує EDSDK; помилка запису ISO тепер явно валить exposure.
- Результат Canon capture містить `scienceFilePath`; GUI після успішної експозиції показує `Science/original file: ...`.
- Plate solving і далі працює з оперативним `CameraFrame.image` (для перевіреного RAW EOS 550D це embedded JPEG 5184×3456). CR2 зберігається окремо для калібрування/stacking/photometry.

## v0.2.10.22 — виправлення лінкування Windows GUI

- Повернуто відсутню реалізацію `MainWindow::refreshFocuserStatus()`, через яку MSVC падав з `LNK2019/LNK1120` під час лінкування `OpenAstroSuite.exe`.
- Додано `tools/gui_link_contract_check.py`, який перевіряє, що звичайні методи `MainWindow`, оголошені в header, мають відповідні out-of-class визначення.
- `OperationManager` переведено з невикористаного результату `QtConcurrent::run()` на `QThreadPool::start()`, що прибирає попередження MSVC/Qt C4858 без зміни семантики планування operations.


## v0.2.10.21 — геометрія монтування, mechanical Park та QHY recovery

- Native hardware сканується один раз після старту node. **Періодичного vendor polling більше немає.** Пристрій, підключений пізніше, шукається кнопкою **Refresh native device discovery** або `POST /api/v1/drivers/refresh`.
- Remote refresh асинхронний, тому QHY/serial/vendor discovery не блокує GUI та HTTP control plane.
- Refresh vendor-neutral: native registry опитує всі увімкнені драйвери, не припускаючи, що користувач має саме QHY.
- **J2000 — канонічна екваторіальна система OAL.** GOTO/Sync API приймає `coordinateFrame: J2000|JNOW`; якщо поле відсутнє — використовується J2000.
- На вкладці Mount можна вводити/показувати `J2000 / catalog` або `JNow / of-date`; node переводить JNow у J2000.
- Stellarium bridge трактується як J2000. Classic ASCOM читає `EquatorialSystem`; J2000 передається напряму, а topocentric/of-date ASCOM driver наразі наближено трактується як JNow із прецесією на compatibility boundary (повна apparent/topocentric корекція ще не реалізована).
- `MountGeometry` профілі охоплюють GEM, fork-equatorial, Alt-Az, Alt-Az+derotator, equatorial platform та custom two-axis; sky geometry більше не зашивається в raw-axis driver.
- Home/Park зберігаються у механічних координатах окремо від RA/DEC; default direct-MC Home/Park — Axis1=0°, Axis2=0°, а GUI вміє зберегти поточні осі як Park.
- Native EQDrive і direct SynScan/EQDrive Wi-Fi використовують одну Core geometry model; Sync на відому точку визначає encoder offsets/signs установки.
- Явний **Refresh native device discovery** після нульового QHY scan може hard-reload `oal.qhy` DLL разом із QHYCCD dependency, але тільки коли QHY не відкрита. Періодичного vendor polling немає.
- Classic ASCOM під час slew тепер логуватиме RA/DEC/pier/tracking, щоб відокремити поведінку EQMOD від OAL coordinate conversion.
- Див. `docs/uk/COORDINATE_FRAMES.md` та `docs/uk/MOUNT_GEOMETRY.md`.

## v0.2.10.19 — HIL-виправлення старту, каталогу та mount transport

- **Server-first node startup:** HTTP/WebSocket стартують до hardware enumeration; Gemini/EQDrive/QHY discovery працює у background thread.
- **Live native catalogue:** прибрано помилковий фільтр device records за driver-only полем `native`; пристрої, знайдені після старту, з’являються у combobox без перезапуску GUI.
- **Async serial selector:** застосування COM-порту не блокує GUI та не пере-відкриває вже закешований Gemini без потреби.
- **Direct Wi-Fi:** `synscan-wifi` напряму керує адаптером/монтуванням через UDP 11880; `synscan-app` окремо працює через SynScan Pro/App UDP 11881.
- **Виправлено Motor Controller semantics:** status/direction bits та GOTO motion mode узгоджені з Sky-Watcher/EQMOD; GOTO використовує `G -> H -> M -> J` і перевіряє реальний рух encoder-а.
- **Native EQDrive dual protocol:** спочатку пробується EQMOD-сумісний Motor Controller protocol, після нього — офіційний ASTEP fallback. Busy COM одразу припиняє probe, не конфліктуючи з EQMOD/ASCOM.
- **HIL safety:** direct native GOTO поки потребує Sync і обмежений невеликими тестовими переміщеннями до кваліфікації напрямків/геометрії pier.

## Поточне покриття обладнання

Код native OAL driver вже є для:

- камер QHY (`oal.qhy`, QHYCCD SDK);
- Canon EOS (`oal.canon`: Canon EDSDK на Windows, libgphoto2/PTP на Linux);
- камер ZWO ASI (`oal.zwo.asi`, ASI SDK);
- фокусерів ZWO EAF (`oal.zwo.eaf`, EAF SDK);
- Gemini EAF (`oal.gemini`, прямий serial protocol);
- Sky-Watcher/SynScan (`oal.skywatcher`, наявний direct serial/EqMount path);
- EQDrive (`oal.eqdrive`, експериментальний dual-protocol serial path: EQMOD-сумісний Motor Controller спочатку, official ASTEP fallback).

Драйвери реалізовані в коді, але ще потребують HIL-кваліфікації на конкретній ОС, CPU-архітектурі, пристрої та firmware перед позначкою production-ready.


## Сумісність mount у v0.2.10.17

- **Native EQDrive:** `oal.eqdrive` є окремим драйвером і не замінює `oal.skywatcher`. Discovery використовує лише read-only команди; початкова RA/Dec-модель консервативна (`sync-anchor`) і потребує одного Sync перед нативним celestial GOTO. Див. `docs/uk/EQDRIVE.md`.
- **Classic ASCOM (Windows):** backend `ascom-classic` працює через `oas-ascom-host.exe`, встановлену ASCOM Platform і зареєстрований Telescope driver; GUI має **ASCOM Chooser...** та **ASCOM Properties...**. Це compatibility path для EQMOD-подібної роботи. Див. `docs/uk/ASCOM_CLASSIC.md`.
- **SynScan network:** `synscan-wifi` напряму працює з mount/EQDrive Wi-Fi adapter через Motor Controller protocol UDP 11880 без SynScan Pro. `synscan-app` підключається до запущеного SynScan App/Pro через UDP 11881. Див. `docs/uk/SYNSCAN_NETWORK.md`.
- **Вибір Gemini COM:** вибір порту в **Native serial discovery** тепер зберігає override та мігрує старий native Gemini backend binding на пристрій, знайдений на новому порту. CLI `--gemini-port` лишається пріоритетним для поточного процесу.

## Runtime-модель

```text
QHY / Canon / ZWO / Gemini / Sky-Watcher / EQDrive
                    │ USB / serial
                    ▼
             openastrolink-node
               ┌────┴─────┐
          local GUI   remote GUI
                         │
                     Stellarium

Опційний compatibility path:
INDI / Classic ASCOM / ASCOM Alpaca / SynScan network / LX200
```

Node володіє обладнанням і довгими операціями. Закриття GUI не повинно зупиняти node або від'єднувати пристрої.

---

# Швидкий старт збірки

## CMake та сумісність presets

У v0.2.10.11 використовується **CMake preset schema v2**, щоб presets читалися CMake 3.20+ і не вимагали CMake 3.25+ лише через формат JSON.

Перевір:

```bash
cmake --version
```

Потрібно:

```text
CMake >= 3.20
```

Якщо зі старого checkout залишився `CMakeUserPresets.json` з:

```json
"version": 6
```

старіший Linux CMake може завершитися помилкою:

```text
Unrecognized "version" field
```

Для v0.2.10.11 створіть user preset заново:

```bash
rm -f CMakeUserPresets.json
cp CMakeUserPresets.example.json CMakeUserPresets.json
```

Потім змінюються лише локальні SDK paths.

Preflight:

```bash
./scripts/check_build_environment.sh
```

Windows PowerShell:

```powershell
.\scripts\check_build_environment.ps1
```

---

# Windows x64

## Єдиний підтримуваний ABI для цієї збірки

```text
MSVC 2022 x64
+ Ninja
+ Qt 6 MSVC2022_64
+ OpenCV для MSVC
+ Windows x64 vendor .lib/.dll
```

Не змішуйте MinGW/Strawberry GCC з Qt `msvc2022_64` або vendor `.lib`.

`scripts/build_windows.ps1` намагається сам завантажити `vcvars64.bat`, тому зазвичай можна запускати зі звичайного PowerShell.

## SDK

### QHY

Приклад поточного layout:

```text
C:/Program Files/QHYCCD/AllInOne/sdk/include/qhyccd.h
C:/Program Files/QHYCCD/AllInOne/sdk/x64/qhyccd.lib
C:/Program Files/QHYCCD/AllInOne/sdk/x64/*.dll
```

OAL ізолює QHY headers від CRT/STL include path, бо деякі QHY SDK містять власні `stdint.h`/`stdint_windows.h`.

### ZWO ASI

Потрібні `ASICamera2.h`, Windows x64 import library та runtime DLL.

### ZWO EAF

Потрібні `EAF_focuser.h`, Windows x64 import library та runtime DLL.

### Canon EDSDK

EDSDK опційний. Без нього використовуйте `my-windows-observatory`, де:

```json
"OAS_ENABLE_NATIVE_CANON": "OFF"
```

Після встановлення EDSDK використовуйте `my-windows-observatory-edsdk`.

## Налаштування і збірка

```powershell
Copy-Item CMakeUserPresets.example.json CMakeUserPresets.json
```

Відредагуйте локальні шляхи і запустіть:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

Або вручну з MSVC x64 environment:

```powershell
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
```

З Canon EDSDK:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory-edsdk -Clean
```

## Packaging

```powershell
.\scripts\package_windows.ps1 `
  -BuildDir build/windows-observatory `
  -QtBin C:/Qt/6.10.0/msvc2022_64/bin `
  -OpenCvBin C:/opencv/opencv/build/x64/vc16/bin `
  -VendorRuntimeDirs @(
    "C:/Program Files/QHYCCD/AllInOne/sdk/x64",
    "C:/SDK/ZWO/ASI SDK/lib/x64",
    "C:/SDK/ZWO/EAF/lib/Windows/x64/Release"
  ) `
  -Zip
```

---

# Native Linux (x86_64 або ARM64/RPi)

Той самий node і GUI можуть напряму володіти обладнанням на desktop Linux, mini-PC або Raspberry Pi.

WSL зручний для compile/test, але для реальної обсерваторії краще native Linux, якщо USB/serial passthrough у WSL не налаштовано спеціально.

## Базові залежності Debian/Ubuntu

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  libopencv-dev libgphoto2-dev libjpeg-dev
```

Назви пакетів можуть трохи відрізнятися між дистрибутивами.

## Canon на Linux

Default transport — **libgphoto2**, тому шлях до EDSDK не потрібен:

```text
Canon EOS → USB/PTP → libgphoto2 → oal.canon → OAL ABI v2
```

Перевірка:

```bash
pkg-config --modversion libgphoto2
```

## QHY Linux SDK

Рекомендований layout:

```text
/opt/openastrolink-sdk/qhy/include/qhyccd.h
/opt/openastrolink-sdk/qhy/lib/libqhy.so
```

Після розпакування SDK:

```bash
sudo mkdir -p /opt/openastrolink-sdk/qhy/include /opt/openastrolink-sdk/qhy/lib
sudo cp /path/to/qhy/include/*.h /opt/openastrolink-sdk/qhy/include/
sudo cp /path/to/qhy/lib/libqhy.so* /opt/openastrolink-sdk/qhy/lib/
```

Якщо є лише versioned `.so`:

```bash
cd /opt/openastrolink-sdk/qhy/lib
sudo ln -sf libqhy.so.0.1.8 libqhy.so
```

Обов'язково:

```bash
uname -m
file /opt/openastrolink-sdk/qhy/lib/libqhy.so
```

Архітектури повинні збігатися.

Для WSL compile test можна використовувати `/mnt/c/...`, але `.so` має бути Linux x86_64, якщо `uname -m` показує `x86_64`.

Для поточних каталогів QHY у WSL приклад:

```json
"QHYCCD_INCLUDE_DIR": "/mnt/c/workspace/astro/QHYCCD_Linux",
"QHYCCD_LIBRARY": "/mnt/c/workspace/astro/qhysdk/lib/libqhy.so.0.1.8"
```

Перед збіркою:

```bash
uname -m
file /mnt/c/workspace/astro/qhysdk/lib/libqhy.so.0.1.8
```

## ZWO ASI/EAF Linux SDK

Рекомендовані layouts:

```text
/opt/openastrolink-sdk/zwo/asi/include/ASICamera2.h
/opt/openastrolink-sdk/zwo/asi/lib/libASICamera2.so

/opt/openastrolink-sdk/zwo/eaf/include/EAF_focuser.h
/opt/openastrolink-sdk/zwo/eaf/lib/libEAFFocuser.so
```

Перевіряйте архітектуру бібліотек через `file`.

## Права на USB/serial

```bash
sudo usermod -aG dialout "$USER"
```

Після цього потрібен logout/login. Також встановіть vendor udev rules для QHY/ZWO, якщо вони входять до SDK/driver package.

## Збірка

```bash
cp CMakeUserPresets.example.json CMakeUserPresets.json
./scripts/build_linux.sh my-linux-observatory
```

Або:

```bash
cmake --preset my-linux-observatory
cmake --build --preset my-linux-observatory -j"$(nproc)"
```

Встановлення:

```bash
sudo cmake --install build/linux-observatory
sudo ldconfig
```

Headless node:

```bash
cmake --preset linux-node-release
cmake --build --preset linux-node-release -j"$(nproc)"
```

---

# INDI compatibility

INDI легко вмикається для іншого обладнання, але не є залежністю native OAL drivers.

```text
*-native-release       тільки native OAL
*-observatory-release  native OAL + INDI compatibility client
```

`indiserver` потрібен лише тоді, коли реально використовується INDI-пристрій.

# Перший HIL

Перевіряйте поетапно:

1. чиста збірка/package;
2. `oal-hardware-probe`;
3. mount: status → малий GOTO → abort → tracking → sync → guide pulse;
4. focuser: position → невеликі move → limits → reconnect;
5. camera: короткі exposure → settings → cancel → повторні capture → reconnect;
6. ASTAP;
7. autofocus;
8. closed-loop pointing і polar alignment;
9. guiding та DSO/planetary workflows.

Див. `docs/uk/VALIDATION.md` та `docs/uk/OAL_SPECIFICATION.md`.

# Поточні межі

Проєкт готовий до подальшої supervised HIL-кваліфікації, але ще не до unattended Internet-facing observatory. Потрібно завершити replayable WebSocket events, повний RFC 9457 HTTP error model, idempotency, durable FITS/RAW/SER data plane, production guiding, durable session recovery, safety/weather/roof policy, TLS/auth/scopes/audit, out-of-process driver sandboxing та conformance suite.


## v0.2.10.11 — reset-aware пошук Gemini EAF у Windows

HIL-перевірка Gemini EAF через USB-UART CH340 у Windows показала, що після відкриття `COMx` контролеру може бути потрібно близько двох секунд, перш ніж команда `:02#` стабільно повертає `EOK#`. Нативний драйвер `oal.gemini` тепер після відкриття COM-порту витримує повний quiet-open інтервал (`openSettleMs`, зараз 2200 мс) до першої команди `:02#`. Якщо відповіді все ще немає — **не закриває порт**, чекає додаткове recovery-вікно (`resetRecoveryMs`, зараз 1200 мс) і повторює handshake один раз. Це працює і для `--gemini-port COMx`, і для автоматичного сканування COM-портів. Повторне відкриття між спробами навмисно не використовується, бо воно може знову перезапустити контролер через USB-UART.

## v0.2.10.11: примітки Windows HIL щодо runtime/discovery

Цей hotfix закриває три проблеми, знайдені під час першого Windows hardware-in-the-loop запуску:

1. Discovery нативних пристроїв тепер кешується. Звичайні запити `/api/v1/node/backends`, `/api/v1/state` та старт GUI **не** запускають повторне сканування USB/serial обладнання. Vendor-neutral scan запускається один раз **після** того, як HTTP/WebSocket control plane вже повідомив node ready, а надалі лише явною дією **Refresh native device discovery** (`POST /api/v1/drivers/refresh`). Це прибирає ситуацію, коли serial probing перевищував 3-секундний timeout remote GUI.
2. Manifests ZWO ASI/EAF приведені до ABI-v2 contract (`schema` та `permissions` як масив).
3. На Windows `QHYCCD_RUNTIME_DIR`, `ZWO_ASI_RUNTIME_DIR`, `ZWO_EAF_RUNTIME_DIR` та `CANON_EDSDK_RUNTIME_DIR` тепер реально використовуються для staging у build tree, а не лише як packaging hints. DLL із цих каталогів копіюються поруч із executable та native drivers під час configure, тому development build не потребує ручного розширення `PATH`.

Після зміни шляхів до vendor SDK повторно виконай CMake configure, щоб staging DLL оновився.

Якщо `CMakeUserPresets.json` був скопійований зі старої версії, постав верхній schema field `"version": 2` для сумісності з CMake 3.20+ та старішими WSL-дистрибутивами. Канонічний шаблон — `CMakeUserPresets.example.json` у репозиторії.

## v0.2.10.11 — ізоляція Windows CRT runtime

Не можна копіювати весь vendor SDK runtime-каталог у каталог програми. Деякі Windows-пакети QHY містять старі `msvcr90.dll` / `msvcp90.dll`; app-local копії можуть конфліктувати з runtime MSVC 2022 і викликати `R6034` ще до нормального старту node.

У v0.2.10.11 копіюються vendor/device DLL, але Microsoft CRT/UCRT redistributables відфільтровуються. Microsoft runtimes потрібно встановлювати штатним redistributable package, а не брати з SDK-каталогів.

Після оновлення з v0.2.10.4 один раз зробіть clean build:

```bat
rmdir /s /q build\windows-observatory
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
powershell -ExecutionPolicy Bypass -File scripts\diagnose_windows_runtime.ps1 -BuildDir build/windows-observatory
```

### v0.2.10.11: виправлення Gemini manifest timing

HIL Gemini/CH340 показав, що manifest драйвера перезаписував C++ reset-settle default значенням `openSettleMs=150`. Runtime defaults тепер синхронізовані: 2200 мс quiet-open settle та додатковий 1200 мс retry на тому самому відкритому порту. Hardware probe друкує ефективні значення таймінгів.


### v0.2.10.11 — коректне завершення node у Windows

Windows HIL виявив race під час завершення `openastrolink-node` через `Ctrl+C`: queued wakeup із worker thread міг прийти вже під час руйнування прихованого message window `QEventDispatcherWin32`, через що з'являлося `QEventDispatcherWin32::wakeUp: Failed to post a message (Invalid window handle.)`, а процес інколи зависав.

Node тепер встановлює мінімальний console control handler. Він не викликає Qt, а лише фіксує interrupt. Qt-таймер у main thread кожні 50 мс перетворює перший `Ctrl+C`/`Ctrl+Break` на `QCoreApplication::quit()`. Cleanup підключений до `aboutToQuit`, тому listeners, operation workers, device connections, native-driver worker threads і serial sessions зупиняються, поки Qt event dispatcher ще валідний. Другий `Ctrl+C` навмисно передається стандартному Windows handler як аварійний force-terminate.

Очікуваний лог завершення:

```text
Console interrupt received; starting graceful shutdown. Press Ctrl+C again to force termination.
OpenAstroLink node shutdown: stopping listeners and active work
OpenAstroLink node shutdown complete
```

Windows HIL: на реальному обладнанні вже підтверджені native discovery/connection Gemini EAF, фактичний рух фокусера та рух під час autofocus. Окремо ще треба кваліфікувати збіжність і повторюваність autofocus на реальній оптичній цілі.


## v0.2.10.17 — адаптивний plate solving для міського неба

Для міської засвітки та недостатньо точної полярки node більше не залежить від схеми `один довгий кадр -> один запуск ASTAP`. Нова operation `solver.adaptive` починає з короткого кадру, а за потреби переходить до серій коротких експозицій, вирівнює їх за зорями, складає, прибирає великомасштабний градієнт фону та повторює solve. Остання спроба завжди запускає solver навіть якщо локальна оцінка кількості зір песимістична.

У GUI є кнопка **Adaptive urban capture + solve**. Типові параметри: binning 2x2, 3 кадри для проміжного stack, 5 для фінального, максимум 3 с на один кадр і ціль 20 локально детектованих зір. Базова експозиція та gain беруться зі звичайних Capture-полів. Якщо mount підключений, його поточні RA/Dec автоматично використовуються як hint, а radius пошуку розширюється 5° -> 10° -> до заданого максимуму.

REST: `POST /api/v1/solve/adaptive`. Результат operation містить діагностику кожної спроби та `solverFrameId`, який можна відкрити через звичайний frame preview.

### v0.2.10.17 — виправлення HIL discovery/state

- Перед кожною повторною спробою auto-connect виконується повторний native discovery, тому QHY, що з’явилася після старту node, може підключитися без перезапуску.
- Native Gemini move тепер асинхронний на межі драйвера: `focuser.status` доступний під час руху й повертає поточну позицію та `moving`. Autofocus окремо чекає завершення фізичного руху перед експозицією.
- GUI опитує стан фокусера під час ручного руху.
- Native Sky-Watcher discovery тепер детально логує кожен COM-порт і SynScan handshake `KO -> O#`, а також робить retry на тому самому відкритому порту. Поточний RA/DEC native mount device усе ще орієнтований на протокол SynScan hand controller; direct USB/EQDIR motor-controller transport залишається окремим незавершеним шляхом.

### v0.2.10.25 — наступний HIL Canon EOS 550D

Для EOS 550D HIL уже підтверджено non-AF експозиції 1–30 с та transfer оригінального файла. Цей patch закриває решту спостережених проблем: explicit hot-plug Refresh чекає delayed EDSDK enumeration, CR2/JPEG preview відновлюється зі збереженого файла/вбудованого JPEG замість failure через порожній EDSDK thumbnail, а >30 с Bulb використовує утримання `Completely_NonAF` з `BulbStart/BulbEnd` лише як fallback. Наступний HIL має перевірити hot-plug Refresh, preview у GUI, 45 с Bulb і cancellation.

## v0.2.10.29 — виправлення thread-affinity EDSDK після Canon hot-plug

- Виправлено регресію Canon EOS hot-plug: фінальний fallback знаходив і підключав камеру, ISO та shutter-команди працювали, але експозиція завершувалась timeout в очікуванні `kEdsObjectEvent_DirItemRequestTransfer`.
- Причина: у v0.2.10.28 hard-reload викликав `EdsInitializeSDK()` з короткоживучого discovery `QThread`. Доставка callback/event у Canon EDSDK залежить від потоку; після завершення worker transfer callback більше не приходив.
- Тепер hard restart Canon DLL/EDSDK синхронно виконується на довгоживучому Qt event-loop потоці `ApplicationController`, тобто так само, як у вже робочому cold-start path. Саме enumeration залишається асинхронним.
- Логіка QHY та serial-драйверів не змінена.
