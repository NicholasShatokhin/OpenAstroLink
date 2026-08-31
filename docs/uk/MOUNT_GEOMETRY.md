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

Для generic/legacy equatorial backend OAL може й надалі використовувати стару пряму hour-angle/declination модель. Direct Sky-Watcher Motor Controller backends (`oal.eqdrive` serial та `synscan-wifi` UDP/11880) використовують **coordinate model v6**. OAL має одну систему механічних осей: фізична стартова Home/Park поза — це `Axis1=0°, Axis2=0°`. Низькорівневі controller step counts — лише diagnostics, а не друга кутова система.

v6 використовує telescope-direction-vector підхід, аналогічний зрілій реалізації INDI SkyWatcher. OAL переводить JNow target у локальні horizontal coordinates, будує вектор напрямку труби, повертає його в polar-aligned mount frame і перетворює результат у сферичні координати монтування. Публічний `Axis1` — azimuth у mount frame, а `Axis2` — `90° - mount-frame altitude` зі знаком для flipped GEM branch. Тому Home лишається рівно `0°,0°` без вигаданої hour-angle фази.

Одна sky target має дві механічно еквівалентні GEM-позиції: `(A,+P)` та `(A+180°,-P)`. OAL порівнює обидві з поточним положенням контролера і обирає найкоротший допустимий фізичний маршрут. Для кожного encoder sample виконується точне обернене telescope-frame перетворення, тому RA/DEC можуть безперервно передаватися в Stellarium під час slew.

На direct-MC connect OAL зчитує поточні counts обох осей у підготовленій користувачем polar Home позі. Ці counts стають session Home/Park reference і мапляться в `Axis1=0°, Axis2=0°`. Фіксована quarter-turn/90° DEC-поправка не використовується.

Профілі до v0.2.10.42 одноразово мігрують для direct Motor Controller backends на model v6. Legacy custom Home/Park очищається, бо міг бути записаний у несумісній старій coordinate model; standard Home/Park стає `0°,0°`, signs скидаються на `+1/+1`, auto-Home вмикається. Automatic meridian flip поки вимкнений до HIL-кваліфікації.

High-level SynScan шляхи навмисно окремі: native hand-controller (`oal.skywatcher`) і SynScan App (`synscan-app`, UDP/11881) обмінюються RA/DEC із власною alignment model SynScan і не проходять через direct Motor Controller transform. Лише direct Wi-Fi Motor Controller (`synscan-wifi`, UDP/11880) використовує ту саму v6 модель, що й native EQDrive serial.

## Механічні Home та Park

Для native direct-MC нормальний запуск такий: до connect/power-on поставити GEM у повторювану полярну Home-позу (вісь противаг вниз, DEC/труба вздовж полярної осі). Controller counts у момент connect стають session Home/Park reference, а `autoHomeSync` одразу відновлює sky model без ручного Sync біля полюса. Пізніший plate-solve Sync можна використовувати лише для уточнення pointing.

Використовуй **Calibrate current physical pose as persistent Home / Park** у бажаній фізичній позі. Native EQDrive зберігає поточні механічні осі одночасно як OAL Home і Park. Classic ASCOM не має OAL mechanical-axis interface, тому та сама дія викликає стандартний ASCOM `SetPark`. Щоб native й ASCOM паркувались однаково, один раз відкалібруй кожен backend у тій самій позі, не рухаючи монтування між перемиканнями.

## Безпека

Починаючи з v0.2.10.38, налаштовуваний native GOTO envelope — це **реальна кутова відстань по небу**. Це прибирає сингулярність біля полюса: ціль лише за кілька градусів на небі може вимагати дуже великого повороту RA-мотора при Dec ≈ ±90°. Direct transport має окремий fixed mechanical hard cap 180° на вісь.

Бажана назва API/profile — `maxGotoSkyDeltaDeg`; `maxGotoAxisDeltaDeg` лишається backward-compatible alias. Automatic meridian flip поки вимкнений за замовчуванням, тому великі slew треба HIL-підтвердити перед unattended use.

Перетворення координат Alt-Az уже є, але production two-axis sidereal tracking та керування field derotator ще залишаються наступними задачами. Fork-equatorial та equatorial-platform використовують екваторіальний hour-angle foundation без GEM pier-flip геометрії.
