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

Park навмисно не зберігається як небесні RA/DEC. Це фіксоване положення механічних осей. Значення за замовчуванням:

```
Axis 1 = 90 deg
Axis 2 = 0 deg
```

У Profile можна задати Home/Park, а на вкладці Mount є **Set current mechanical axes as Park** та **Restore default mechanical Park (90°, 0°)**. Native EQDrive і direct SynScan/EQDrive Wi-Fi використовують цей mechanical park. Classic ASCOM продовжує використовувати власний Park/Unpark ASCOM-драйвера.

## Безпека

Для celestial GOTO native EQDrive та direct Wi-Fi тимчасовий HIL-limit 15° знято. Використовується найкоротший шлях механічної осі (до 180°). Automatic meridian flip все ще вимкнений, тому довгі переходи треба виконувати під наглядом до повної HIL-кваліфікації pier-side та фізичних limits.

Перетворення координат Alt-Az уже є, але production two-axis sidereal tracking та керування field derotator ще залишаються наступними задачами. Fork-equatorial та equatorial-platform використовують екваторіальний hour-angle foundation без GEM pier-flip геометрії.
