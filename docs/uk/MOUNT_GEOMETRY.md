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

Generic/legacy equatorial backends зберігають власні coordinate model. Direct Sky-Watcher Motor Controller backends (`oal.eqdrive` serial і `synscan-wifi` UDP/11880) використовують **coordinate model v7**. Обидва transport мають одну механічну систему: controller counts, зняті у підготовленій фізичній Home-позі, означають `Axis1=0°, Axis2=0°`; raw step counts лишаються лише diagnostics.

v7 повторює conventional EQMOD-style механіку GEM hour-angle/declination pointing states. У північній півкулі counterweight-down polar Home визначений як sky `HA=-90° (-6 h), Dec=+90°`, `pier=west`. Для `HA <= 0°` west branch має `Axis1 = HA + 90°`, `Axis2 = +(90°-Dec)`; для `HA > 0°` протилежний фізичний pointing state має `Axis1 = HA - 90°`, `Axis2 = -(90°-Dec)`. Для південної півкулі відповідна pole/axis convention дзеркальна.

Це замінює експериментальну v6 polar-telescope-frame model. v6 правильно зробила serial і UDP transport однаковими, але вважала `(A,+P)` та `(A+180°,-P)` вільно взаємозамінними й обирала найкоротший рух моторів. Реальний HIL 2026-08-31 показав, що так можна вибрати неправильний **фізичний GEM pointing state**: serial і Wi-Fi рухаються однаково, але обидва промахуються, тоді як EQMOD Classic ASCOM на тому самому залізі наводиться правильно. У v7 фізична гілка визначається hour angle/side of pier, а не shortest mathematical representation.

Sky-Watcher Motor Controller position packets декодуються shared codec з протокольним signed reference `0x800000`. При connect OAL зберігає нормалізовані counts як session Home/Park; додаткового quarter-turn/90° transport offset немає. Native EQDrive serial і direct Wi-Fi після Core geometry використовують той самий `skywatcher_mc::makeGotoPlan()`.

Профілі direct-MC v6 одноразово мігрують у v7: legacy custom Home/Park очищуються, Home/Park стають `0°,0°`, axis signs — `+1/+1`, auto-Home вмикається. Automatic meridian flip поки не вважається HIL-qualified для unattended use.

High-level SynScan paths лишаються окремими: `oal.skywatcher` hand-controller та `synscan-app` працюють із celestial RA/DEC через модель SynScan. Лише direct Motor Controller Wi-Fi (`synscan-wifi`, UDP/11880) ділить v7 з native EQDrive serial.

## Механічні Home та Park

Для native direct-MC нормальний запуск такий: до connect/power-on поставити GEM у повторювану полярну Home-позу (вісь противаг вниз, DEC/труба вздовж полярної осі). Controller counts у момент connect стають session Home/Park reference, а `autoHomeSync` одразу відновлює sky model без ручного Sync біля полюса. Пізніший plate-solve Sync можна використовувати лише для уточнення pointing.

Використовуй **Calibrate current physical pose as persistent Home / Park** у бажаній фізичній позі. Native EQDrive зберігає поточні механічні осі одночасно як OAL Home і Park. Classic ASCOM не має OAL mechanical-axis interface, тому та сама дія викликає стандартний ASCOM `SetPark`. Щоб native й ASCOM паркувались однаково, один раз відкалібруй кожен backend у тій самій позі, не рухаючи монтування між перемиканнями.

## Безпека

Починаючи з v0.2.10.38, налаштовуваний native GOTO envelope — це **реальна кутова відстань по небу**. Це прибирає сингулярність біля полюса: ціль лише за кілька градусів на небі може вимагати дуже великого повороту RA-мотора при Dec ≈ ±90°. Direct transport має окремий fixed mechanical hard cap 180° на вісь.

Бажана назва API/profile — `maxGotoSkyDeltaDeg`; `maxGotoAxisDeltaDeg` лишається backward-compatible alias. Automatic meridian flip поки вимкнений за замовчуванням, тому великі slew треба HIL-підтвердити перед unattended use.

Перетворення координат Alt-Az уже є, але production two-axis sidereal tracking та керування field derotator ще залишаються наступними задачами. Fork-equatorial та equatorial-platform використовують екваторіальний hour-angle foundation без GEM pier-flip геометрії.
