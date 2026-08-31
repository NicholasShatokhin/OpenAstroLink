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

Геометрія є конфігурацією обсерваторії, а не hard-coded властивістю EQDrive чи іншого transport driver. Native raw-axis driver відповідає за положення/рух осей, а астрономічну геометрію виконує OAL Core.

## Модель GEM

Для GEM OAL переводить J2000 у координати поточної епохи, обчислює local sidereal time з UTC та довготи обсерваторії, отримує hour angle, обирає pier branch і переводить результат у механічні осі. Один Sync на відому точку неба визначає encoder offset та знаки осей конкретної установки. Автоматичний meridian flip за замовчуванням вимкнений до HIL-кваліфікації.

Поля знаку осей описують орієнтацію конкретної установки. Їх не треба замінювати випадковими інверсіями RA/DEC у hardware driver. Поточна HIL-установка EQDrive після Sync узгоджується з профілем `+1/+1`, але інші установки можуть мати інший напрямок будь-якої осі.

## Механічні Home та Park

Для native raw-axis mount Home/Park навмисно не зберігається як небесні RA/DEC. Повторювана GEM Home-поза (вісь противаг вниз, DEC/труба вздовж полярної осі) задає persistent mechanical reference і, якщо `autoHomeSync` увімкнено, дозволяє відновити sky model при старті без ручного Sync біля полюса.

Використовуй **Calibrate current physical pose as persistent Home / Park** у бажаній фізичній позі. Native EQDrive зберігає поточні raw axes одночасно як OAL Home і Park. Classic ASCOM не має OAL raw axes, тому та сама дія викликає стандартний ASCOM `SetPark`. Щоб native й ASCOM паркувались однаково, один раз відкалібруй кожен backend у тій самій позі, не рухаючи монтування між перемиканнями.

## Безпека

Починаючи з v0.2.10.38, налаштовуваний native GOTO envelope — це **реальна кутова відстань по небу**. Це прибирає сингулярність біля полюса: ціль лише за кілька градусів на небі може вимагати дуже великого повороту RA-мотора при Dec ≈ ±90°. Raw transport має окремий fixed mechanical hard cap 180° на вісь.

Бажана назва API/profile — `maxGotoSkyDeltaDeg`; `maxGotoAxisDeltaDeg` лишається backward-compatible alias. Automatic meridian flip поки вимкнений за замовчуванням, тому великі slew треба HIL-підтвердити перед unattended use.

Перетворення координат Alt-Az уже є, але production two-axis sidereal tracking та керування field derotator ще залишаються наступними задачами. Fork-equatorial та equatorial-platform використовують екваторіальний hour-angle foundation без GEM pier-flip геометрії.
