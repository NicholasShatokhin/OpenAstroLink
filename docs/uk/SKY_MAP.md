# Sky Map — offline-навігація монтування

**Додано:** v0.2.10.51

OpenAstroSuite має легку повністю offline **Sky Map** у лівій робочій області. Це базова навігація телескопа без запущеного Stellarium; Stellarium bridge лишається для користувачів, яким потрібен повноцінний planetarium.

## Поточний MVP

- horizon/all-sky проєкція за observer site та поточним UTC;
- каталог яскравих зір і компактний набір Messier/DSO targets;
- допоміжні лінії кількох відомих сузір’їв;
- N/E/S/W та altitude circles 30°/60°;
- drag мишкою для pan, wheel для zoom;
- click — select, double-click — GOTO;
- пошук за назвою (`Vega`, `Polaris`, `M31`, `M42`, ...);
- RA/DEC (J2000), Alt/Az та magnitude вибраного об’єкта;
- live marker телескопа з active OAL mount state;
- marker plate-solved position;
- приблизний FOV main camera з active optical profile;
- дії **Slew**, **Sync**, **Abort slew**, **Park**, **Unpark**, **Use in Scheduler**.

Усі telescope actions проходять через `ObservatoryController` і active OAL mount backend. Sky Map не має окремої mount geometry і не обходить operation/safety policy.

## Coordinate contract

Catalogue positions — J2000. Для рендерингу використовується існуючий `equatorial_frames`: precession у JNow і conversion у horizontal coordinates за observer site та UTC. HIL-qualified direct-MC coordinate model v9 Sky Map не змінює.

## Межа scope

Це навігаційний MVP, а не заміна повного planetarium. Для найближчої Beta не потрібні photographic surveys, повний DSO catalogue, planet ephemerides, Milky Way textures або Smart Telescope recommendations. Це пізніша UI/OAL 1.0 робота.

## Validation

Перед Beta перевірити на real hardware:

1. mount marker відповідає reported mount coordinates;
2. відома зоря має правдоподібні Alt/Az для site/time;
3. Sky Map GOTO і Mount-tab GOTO ведуть у ту саму sky coordinate;
4. double-click GOTO можна перервати через Sky Map;
5. Sync змінює pointing model тільки через active backend;
6. solved marker відповідає plate-solve result;
7. `Use in Scheduler` переносить J2000 coordinate без зміни.
