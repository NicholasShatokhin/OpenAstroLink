# Специфікація scheduler та автономної зйомки OpenAstroLink

**Канонічна мова:** англійська; цей файл є українським дзеркалом.  
**Ціль:** поетапна реалізація від першої supervised beta до OAL 1.0.  
**Стан реалізації:** у v0.2.10.48 node має mixed-mode non-durable executor. DSO blocks виконують `slew -> adaptive solve/recenter -> autofocus -> FITS/RAW`. Planetary blocks уже виконують `GOTO -> full-frame acquisition/detection -> planetary autofocus -> hardware ROI -> finite SER`, із fixed-size ROI tracking, `.roi.jsonl` provenance координат сенсора та опційним повільним mount recentering, який калібрується за реальним RA/DEC image response. `SessionTarget` лишився legacy DSO compatibility wrapper. Durable checkpoints/restart, weather/roof safety, meridian recovery та hardening до unattended OAL 1.0 ще заплановані.

### v0.2.10.48 — життєвий цикл та редагування scheduler

Supervised scheduler тепер підтримує негайний або відкладений у пам'яті старт, редагування/видалення/перестановку блоків у GUI, копіювання поточного J2000-напрямку монтування в блок і preflight `waiting-camera`. Якщо при старті плану main-camera lock утримує інтерактивний Live View, node спочатку скасовує/фіналізує Live View і лише після звільнення камери починає автономну зйомку. Це прибирає ситуацію, коли science exposure виглядає завислою, хоча насправді стоїть у черзі за безперервним preview.


## 1. Мета

Scheduler OpenAstroLink має бути не таймером і не простим списком експозицій, а workflow engine обсерваторії. Він повинен оркеструвати наведення, фокусування, plate solving, центрування, guiding/tracking, science capture, recovery та calibration-aware metadata, при цьому node лишається authoritative owner обладнання.

Потрібні два рівноправні класи зйомки:

1. **DSO:** послідовності FITS/RAW, переважно довгі експозиції, за потреби з фільтрами, guiding і dithering.
2. **Місячна/планетарна/high-speed:** серії SER з короткими експозиціями, високим FPS, ROI та безперервним утриманням об'єкта в кадрі.

Один нічний план може містити обидва класи.

## 2. Поточна межа реалізації

Починаючи з v0.2.10.48, node має одну `ObservationPlan` / `ObservationBlock` state machine для DSO та planetary acquisition. Усі довгі кроки виконуються асинхронно через `OperationManager` і використовують ті самі resource locks, що й інтерактивні команди. DSO шлях:

```text
PREPARE_BLOCK -> SLEW -> SOLVE/RECENTER -> AUTOFOCUS -> CAPTURE -> [periodic corrections] -> next frame/block
```

Solve/recenter loop plate-solve-ить реальне поле, вимірює pointing error до target block, робить `Sync + correction slew` і повторює цикл до заданої tolerance в arcmin або ліміту спроб. Recenter та autofocus можна виконувати перед першим science frame і/або кожні N завершених кадрів. Science capture примусово просить durable FITS/RAW output, якщо backend камери це підтримує.

`SessionTarget` ще приймається GUI/API для сумісності й конвертується в DSO blocks. Planetary SER blocks тепер виконуються: після GOTO робиться full-frame acquisition, знаходиться яскравий planet/lunar target, опційно запускається planet-mode autofocus, формується fixed-size hardware ROI і записуються finite SER runs. Під час SER origin ROI може рухатися без зміни width/height; кожна зміна пишеться в `.roi.jsonl`. Опційна mount correction використовує виміряну двовісну калібровку, а не припущення про орієнтацію камери. Scheduler поки **non-durable**: restart/resume node, weather hold, meridian-flip recovery та unattended safety лишаються роботою OAL 1.0.

## 3. Модель плану

Persisted `ObservationPlan` містить впорядковані або priority-scheduled `ObservationBlock`.

Кожен block має містити:

- стабільний ID та назву;
- target resolver input і resolved coordinates/ephemeris;
- режим `dso-fits` або `planetary-ser`;
- прив'язки optical train, camera, mount, focuser;
- опційний guide train;
- time window;
- minimum altitude / maximum airmass;
- обмеження за Sun altitude/twilight;
- Moon separation/illumination за потреби;
- meridian/side-of-pier policy;
- weather/safety вимоги після реалізації відповідних device profiles;
- retry/recovery policy;
- completion policy: frame count, total integration, SER count, duration або time window.

Після кожної суттєвої зміни стану обсерваторії чи science progress має писатися durable checkpoint.

## 4. Загальний execution block

```text
WAIT_CONSTRAINTS
 -> RESOLVE_TARGET
 -> PREPARE_DEVICES
 -> SLEW
 -> SETTLE
 -> SOLVE_AND_RECENTER
 -> AUTOFOCUS
 -> START_TRACKING/GUIDING
 -> ACQUIRE
 -> PERIODIC_CORRECTIONS
 -> COMPLETE_BLOCK
```

Recovery states:

```text
RETRY_CAPTURE
REACQUIRE_TARGET
RECENTER
REFOCUS
REGUIDE
MERIDIAN_FLIP
WEATHER_HOLD
SAFE_PARK
RESUME_FROM_CHECKPOINT
FAILED
```

Усі довгі дії мають виконуватися через OAL async operations і resource locks.

## 5. DSO FITS

DSO sequence має підтримувати exposure, frame count/total integration, gain/ISO, offset, binning, readout mode, filter/sequence, FITS/RAW policy, guiding/dither та provenance/checksum.

### 5.1 Recenter

Solve/recenter можна запускати:

- перед першим science frame;
- після кожного frame;
- кожні N frames;
- кожні N хвилин;
- при перевищенні guide/solve error;
- після autofocus;
- після зміни filter;
- після meridian flip;
- після recovery/reconnect.

Отже політика `recenter after every exposure` має бути штатною.

### 5.2 Autofocus

Full autofocus можна запускати:

- перед першим frame;
- після кожного frame;
- кожні N frames;
- кожні N хвилин;
- після filter change;
- після зміни температури на задану delta;
- при деградації HFR/FWHM/focus score;
- після meridian flip або reacquisition.

Потрібен rollback до останнього валідного focus position при невдалому autofocus.

## 6. Temperature focus compensation прямо під час експозиції

OpenAstroLink має опційно підтримувати **in-exposure thermal focus compensation** для optical train, де це дозволяє focuser/camera configuration.

Це не full autofocus. Використовується калібрована модель `focuser steps / °C` (за потреби piecewise/filter-specific), а під час довгої експозиції виконуються малі обмежені корекції.

Потрібні:

- enable/disable на optical train;
- calibrated temperature-focus model і confidence;
- temperature source: focuser/weather/інший sensor;
- minimum temperature delta;
- max steps per correction;
- max accumulated motion per exposure/per minute;
- minimum interval;
- capability check на focus motion during exposure;
- fallback з відкладенням до межі frame, якщо рух під час інтеграції небажаний.

Кожна корекція записується з UTC, temperature, old/new focus position та exposure/frame ID.

## 7. Планетарна/місячна SER-зйомка

Planetary block має бути окремим scheduler operation, а не емульованою DSO експозицією.

Параметри:

- SER duration/frame count;
- кількість SER runs;
- exposure/gain/offset/high-speed readout;
- target FPS або maximum-rate;
- bit depth і Bayer/raw policy;
- ROI width/height та початковий sensor origin;
- пауза між SER;
- опційний refocus/recenter між кожними SER;
- refocus/recenter за часом або кількістю runs.

## 8. ROI та автоматичне центрування планети

**Реалізація v0.2.10.48:** full-frame bright-object detection, старт hardware ROI, fixed-size ROI shifts під час SER, ROI provenance та опційна двовісно калібрована mount micro-slew correction уже реалізовані. Mount loop за замовчуванням вимкнений до HIL-кваліфікації конкретного backend; ROI-only tracking є стандартним режимом.

### 8.1 ROI tracking

Якщо камера має movable hardware ROI, система може тримати об'єкт у центрі, пересуваючи ROI по сенсору без руху mount.

- Розмір ROI у межах одного SER незмінний; зміна розміру закриває файл і починає новий.
- Кожна зміна sensor origin записується з timestamp/frame range.
- SER sidecar містить initial ROI і history рухів.
- Calibration tools мають уміти повторно застосувати конкретний записаний ROI/segment.
- Для великої кількості змін бажаний `*.roi.jsonl` або еквівалент.

### 8.2 Mount recentering

Centroid об'єкта вимірюється у live planetary stream. Корекція mount запускається, якщо:

- centroid error перевищив threshold;
- об'єкт наближається до краю ROI;
- ROI дійшов до sensor boundary;
- drift через вітер/люфт/tracking error не слід компенсувати лише ROI.

Можна використовувати pulse guide, bounded micro-slew або інший low-disturbance mount operation. Потрібні hysteresis і settle window, щоб ROI та mount не боролися між собою. Усі корекції логуються.

## 9. Повністю автономна планетарна зйомка

```text
resolve topocentric ephemeris
 -> перевірити altitude/time/safety constraints
 -> unpark
 -> slew to planet
 -> знайти об'єкт у full frame/finder
 -> center
 -> planetary autofocus
 -> встановити ROI
 -> start SER
 -> безперервно контролювати centroid
 -> рухати ROI і/або коригувати mount
 -> за policy refocus/recenter між SER
 -> повторити потрібну кількість runs
 -> park або перейти до наступного block
```

Для початкового захоплення можна використовувати великий ROI/full frame, guide/finder camera або короткий star-field solve. Plate solving кожного планетарного кадру не потрібен.

## 10. Meridian handling

`Near-meridian target` — це об'єкт з **hour angle близьким до 0°**, тобто він майже перетинає локальний меридіан спостерігача. Для GEM це критичний тест і scheduler condition, бо саме там може змінюватися side-of-pier.

У майбутньому scheduler має підтримувати:

- не починати block, якщо він не встигає до flip margin;
- завершити поточну exposure і виконати flip;
- за hard safety limit перервати/restart exposure;
- перейти на протилежний GEM pointing state;
- solve/recenter;
- відновити guiding;
- за потреби відновити focus;
- продовжити точно з checkpoint.

Automatic meridian flip належить до autonomous/OAL 1.0 і сьогодні не production-qualified.

## 11. Metadata та відтворюваність

Для кожного artifact треба зберігати plan/block/run/frame IDs, target/ephemeris, UTC, mount/pier, solve/recenter history, focus/temperature, guiding quality, exposure/gain/offset/binning/readout, filter, ROI geometry/history, driver identity/version та safety/weather state.

## 12. Durability/restart

Scheduler зберігає plan revision, current block/phase, completed FITS/SER, active run identity, last valid focus, last solved center, ROI state, flip state, retry counters та suspended/unsafe reason.

Після restart node система спочатку rediscover/reconcile hardware state і лише тоді resume, вимагає operator intervention або safe park. Небезпечну апаратну дію не можна повторювати наосліп.

## 13. Залежності OAL 1.0

Для unattended scheduler заплановані, але ще не завершені:

- TLS, authentication, roles/scopes, audit;
- idempotent commands і durable operations;
- replayable WebSocket stream;
- production guiding/dither/post-flip recovery;
- weather/safety, dome/roof, power interlocks;
- emergency stop;
- durable science store/checksums/download recovery;
- driver isolation та public conformance suite.

У supervised beta можна показувати scheduler building blocks раніше, але не називати систему unattended-safe до завершення цих вимог.
