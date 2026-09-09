# Sky Map — offline-навігація та кадрування

> Поточний synchronized snapshot: **v0.2.10.58-buildfix9 (2026-09-09)**. Див. `STATUS.md`, `CURRENT_CHECKLIST.md` та `RELEASE_0.2.10.58.md` щодо current qualification boundaries.


**Додано:** v0.2.10.51  
**Наведення у довільну точку:** v0.2.10.52  
**Рамки камер / mosaic framing / export у Stellarium:** v0.2.10.53

OpenAstroSuite має легку повністю offline **Sky Map** у лівій робочій області. Це native OAL поверхня для навігації телескопа й планування кадру без необхідності запускати повний planetarium. Stellarium і далі підтримується через стандартний Telescope Control bridge та окремий опційний Remote Control export рамки.

## Навігація

- horizon/all-sky проєкція за observer site і поточним UTC;
- каталог яскравих зір, selected Messier/DSO та прості constellation guides;
- N/E/S/W і altitude guides;
- pan/zoom/search;
- click по catalogue object **або довільній видимій точці неба**;
- double-click по будь-якому target — GOTO;
- live marker телескопа, solved marker, J2000 та Alt/Az вибраного target;
- controller-backed **Slew**, **Sync**, **Abort slew**, **Park**, **Unpark**, **Use in Scheduler**.

Усі telescope actions і далі проходять через `ObservatoryController` та active OAL mount backend. Sky Map не має другої mount geometry. HIL-qualified direct-MC v9 лишається frozen.

## v0.2.10.53 — рамки камер

Sky Map тепер має три незалежні footprint layers:

1. **Solved frame** — виміряна рамка останнього успішного main-camera plate solve. OAL зберігає `lastSolvedFrame`: J2000 center, width/height у градусах, solver rotation/PA, pixel dimensions solved image та прапорець measured/predicted. Розмір береться з фактичних dimensions кадру й solver pixel scale, тому ROI/binning solved image враховуються природно.
2. **Main planned frame** — прогноз до solve з main optical profile (`focalLength`, pixel size, sensor width/height) та editable main-camera position angle.
3. **Guide planned frame** — окрема прогнозована рамка guide optical train і guide camera.

Кожен layer можна окремо показати/сховати й підписати angular size та PA. Замість старого приблизного ellipse FOV малюється реально повернутий прямокутний footprint на небесній сфері.

Rotation convention спільний із `SolveResult` та `MosaicFitsBlock`: **sensor +X повернутий на `rotationDeg` від local celestial east у бік celestial north**.

## Mosaic planner

Опція `Mosaic planner grid` використовує налаштування Scheduler:

- columns / rows;
- overlap percentage;
- main-camera rotation.

Selected Sky Map target є центром мозаїки. Якщо target не вибраний, current telescope pointing використовується як preview anchor. Tile centers обчислюються в local tangent plane і малюються тією самою rotated camera footprint. Sky Map main PA та Scheduler mosaic rotation синхронізовані, а **Use in Scheduler** переносить і J2000 target, і current main-camera framing angle.

Це лише planning visualization; execution лишається в existing node-side Scheduler/mosaic engine.

## Export рамки у Stellarium

Стандартний Stellarium Telescope Control TCP protocol як і раніше передає лише telescope position/GOTO і не має packet для camera footprint.

Якщо в Stellarium увімкнений опційний **Remote Control** plugin, Sky Map може передати один вибраний OAL footprint у built-in rectangular FOV marker Stellarium. Джерела:

- solved frame;
- main planned frame;
- guide planned frame;
- mosaic envelope.

Default Remote Control URL: `http://127.0.0.1:8090`. OpenAstroSuite знаходить реальні `SpecialMarkersMgr` property IDs через `/api/stelproperty/list`, задає width/height/rotation/visibility через `/api/stelproperty/set`, центрує Stellarium через `/api/main/view` і підбирає viewport FOV через `/api/main/fov`.

Stock Stellarium має один built-in rectangular FOV marker, тому наступний export замінює попередню OAL рамку. Для кількох одночасно persistent OAL rectangles у Stellarium потрібен окремий Stellarium plugin; це не входить у поточну Beta-фічу.

## Coordinate contract

Catalogue positions і free-point selections зберігаються як J2000. Free-point click інвертується у Az/Alt і один раз переводиться в J2000, тому Scheduler/GOTO отримують inertial coordinate.

Кути camera frame будуються навколо J2000 center у local east/north tangent plane, повертаються на PA й лише для відображення конвертуються в horizontal coordinates. У Stellarium передається той самий J2000 center.

## Validation

Перед Beta перевірити:

1. catalogue/free-point GOTO не змінився й збігається з Mount tab;
2. solved rectangle center збігається із solved marker;
3. measured width/height дорівнюють `scaleArcsecPerPx × image dimensions`;
4. навмисно повернута камера має той самий знак orientation на Sky Map і в solve result;
5. main/guide predicted sizes відповідають optical profile;
6. main PA синхронізується із Scheduler mosaic rotation в обидва боки;
7. 2×2 і 3×2 planner grid мають правильний overlap і центр;
8. **Use in Scheduler** зберігає J2000 target та framing rotation;
9. з увімкненим Stellarium Remote Control усі чотири source frame коректно експортуються за center/size/PA;
10. Hide frame не впливає на стандартний Telescope Control bridge.

Catalogue-object → Scheduler transfer уже підтверджений у running GUI. v0.2.10.53 додає framing path без змін mount v9 geometry.


## HIL checkpoint 2026-09-06

Free-point Sky Map GOTO фізично підтверджений на реальному монтуванні, а Sky Map target transfer у Scheduler підтверджений у running GUI. Current code також дзеркалить selected target у Mount-tab J2000 fields. Camera-footprint і Stellarium-export overlays ще HIL-pending.
