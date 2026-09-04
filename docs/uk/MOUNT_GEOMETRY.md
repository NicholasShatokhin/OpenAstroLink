# Геометрія монтування та механічні координати

OpenAstroLink розділяє **небесні координати** і **механічні осі монтування**. J2000 є канонічною екваторіальною системою обміну; модель геометрії монтування перетворює ціль на небі у координати осей конкретного типу монтування.

## Підтримувані профілі геометрії

Foundation v0.2.10.25 визначає:

- German Equatorial Mount (GEM);
- fork equatorial;
- alt-azimuth;
- alt-azimuth із derotator;
- equatorial platform;
- custom two-axis.

Геометрія є конфігурацією обсерваторії, а не hard-coded властивістю EQDrive чи іншого transport driver. Native direct-axis driver відповідає за положення/рух осей, а астрономічну геометрію виконує OAL Core.

## Німецька екваторіальна модель

Generic/legacy equatorial backends зберігають власні coordinate model. Direct Sky-Watcher Motor Controller backends (`oal.eqdrive` serial і `synscan-wifi` UDP/11880) використовують **coordinate model v9**. Обидва transport мають одну механічну систему: controller counts, зняті у підготовленій фізичній Home-позі, означають `Axis1=0°, Axis2=0°`; raw step counts лишаються лише diagnostics.

v9 зберігає введену у v7 conventional EQMOD-style механіку GEM hour-angle/declination pointing states. Повний перебір чотирьох комбінацій знаків за HIL 2026-09-02 показав, що точне дзеркало схід↔захід створює mapping DEC/polar-distance axis, тому кваліфікований direct-MC mapping — `Axis1Sign=+1`, `Axis2Sign=-1`. У північній півкулі counterweight-down polar Home визначений як sky `HA=-90° (-6 h), Dec=+90°`, `pier=west`. Для `HA <= 0°` west branch має `Axis1 = HA + 90°`, `Axis2 = +(90°-Dec)`; для `HA > 0°` протилежний фізичний pointing state має `Axis1 = HA - 90°`, `Axis2 = -(90°-Dec)`. Для південної півкулі відповідна pole/axis convention дзеркальна.

Це замінює експериментальну v6 polar-telescope-frame model. v6 правильно зробила serial і UDP transport однаковими, але вважала `(A,+P)` та `(A+180°,-P)` вільно взаємозамінними й обирала найкоротший рух моторів. Реальний HIL 2026-08-31 показав, що так можна вибрати неправильний **фізичний GEM pointing state**: serial і Wi-Fi рухаються однаково, але обидва промахуються, тоді як EQMOD Classic ASCOM на тому самому залізі наводиться правильно. У v7 фізична гілка почала визначатися hour angle/side of pier, а не shortest mathematical representation. Реальний HIL 2026-09-02 показав другу незалежну проблему: branch/side-of-pier вже збігався з правильним Classic EQMOD ASCOM, але фізичний RA-рух direct-MC був дзеркальним схід↔захід. v8 перевірила гіпотезу про Axis1 polarity, але повторний HIL лишився дзеркальним. v9 виправляє фактичну polarity між canonical Axis2 та controller-positive рухом; EQMOD branch formulas і raw serial/UDP protocol не змінюються.

Sky-Watcher Motor Controller position packets декодуються shared codec з протокольним signed reference `0x800000`. При connect OAL зберігає нормалізовані counts як session Home/Park; додаткового quarter-turn/90° transport offset немає. Native EQDrive serial і direct Wi-Fi після Core geometry використовують той самий `skywatcher_mc::makeGotoPlan()`.

Профілі старші за v7 і далі проходять одноразове очищення legacy Home/Park. Будь-який direct-MC profile нижче v9 потім мігрує на `Axis1Sign=+1`, `Axis2Sign=-1`, зберігає стандартний Home/Park `0°,0°` та вмикає auto-Home. Для вже v7/v8-профілю Home/Park не скидаються — до v9 мігрує qualified Axis2/DEC-polar-distance mapping. Automatic meridian flip поки не вважається HIL-qualified для unattended use.

High-level SynScan paths лишаються окремими: `oal.skywatcher` hand-controller та `synscan-app` працюють із celestial RA/DEC через модель SynScan. Лише direct Motor Controller Wi-Fi (`synscan-wifi`, UDP/11880) ділить v9 з native EQDrive serial.

## Механічні Home та Park

Для native direct-MC нормальний запуск такий: до connect/power-on поставити GEM у повторювану полярну Home-позу (вісь противаг вниз, DEC/труба вздовж полярної осі). Controller counts у момент connect стають session Home/Park reference, а `autoHomeSync` одразу відновлює sky model без ручного Sync біля полюса. Пізніший plate-solve Sync можна використовувати лише для уточнення pointing.

Використовуй **Calibrate current physical pose as persistent Home / Park** у бажаній фізичній позі. Native EQDrive зберігає поточні механічні осі одночасно як OAL Home і Park. Classic ASCOM не має OAL mechanical-axis interface, тому та сама дія викликає стандартний ASCOM `SetPark`. Щоб native й ASCOM паркувались однаково, один раз відкалібруй кожен backend у тій самій позі, не рухаючи монтування між перемиканнями.

## Безпека — v0.2.10.51

Coordinate model v9 HIL-qualified і frozen. Тимчасовий 15°/`maxNativeGotoDeg` qualification gate усередині `oal.eqdrive` видалено. Це **не** змінює v9 equations, axis signs, Home/Park convention або transport direction logic.

Normal native GOTO safety policy лишається в OAL Core/profile і використовує **реальну кутову відстань по небу**. Бажана назва — `maxGotoSkyDeltaDeg`; `maxGotoAxisDeltaDeg` лишається backward-compatible alias. Значення контролює оператор: його можна лишити консервативним для supervised qualification або підняти до 180° для normal full-range GOTO.

Raw mechanical `mount.gotoAxes` команди мають окремий явний per-request `maxAxisDeltaDeg` guard (default 180°). Це важливо біля небесного полюса, де невеликий sky displacement може коректно вимагати значно більшого обертання RA-axis; normal sky GOTO не повинен блокуватися другим прихованим transport-driver limit.

Automatic meridian-flip orchestration лишається disabled/unqualified для unattended use. Великі slews варто лишати supervised, доки observatory safety stack не завершений.

Перетворення координат Alt-Az уже є, але production two-axis sidereal tracking та керування field derotator ще залишаються наступними задачами. Fork-equatorial та equatorial-platform використовують екваторіальний hour-angle foundation без GEM pier-flip геометрії.
