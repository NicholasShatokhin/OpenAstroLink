# Сумісність із Classic ASCOM

OpenAstroLink тепер має Windows-backend **Classic ASCOM** для mount на додачу до ASCOM Alpaca.

Це compatibility path у стилі N.I.N.A.: OpenAstroLink вибирає зареєстрований ASCOM Telescope driver (наприклад EQMOD), а вже цей драйвер володіє фізичним COM-з'єднанням із монтуванням/контролером.

```text
OpenAstroLink node / GUI
        |
        | локальний JSON IPC
        v
oas-ascom-host.exe
        |
        | Windows COM / IDispatch
        v
ASCOM Telescope driver (наприклад EQMOD)
        |
        v
serial/network transport монтування
```

## Навіщо окремий helper-процес?

Classic ASCOM є Windows-специфічним, а сторонні драйвери мають власні COM threading/UI lifecycle. Тому OAL не тримає COM-об'єкт у головному процесі node. `oas-ascom-host.exe` — невеликий native C++ helper, який працює з установленою ASCOM Platform через COM automation; для його збірки не потрібен .NET SDK.

Це межа ізоляції, а не автоматизація GUI: OpenAstroLink звертається до API ASCOM driver, а не натискає кнопки в його вікні.

## Робота в GUI

1. Вибрати mount backend **ascom-classic**.
2. Натиснути **ASCOM Chooser...**.
3. Вибрати Telescope driver (наприклад встановлений EQMOD).
4. За потреби натиснути **ASCOM Properties...** для setup dialog драйвера.
5. Натиснути **Connect / reconnect**.

Вибраний ProgID зберігається як endpoint mount.

У remote GUI chooser навмисно вимкнений, бо ASCOM Platform і його UI працюють на машині node. ProgID можна налаштувати локально на node, після чого керувати ним віддалено.

## Реалізовані Telescope calls

Helper використовує Classic ASCOM Telescope automation для:

- `Connected`;
- `RightAscension`, `Declination`;
- `Tracking`, `Slewing`, `AtPark`, `AtHome`, `SideOfPier`;
- `SlewToCoordinatesAsync` (або synchronous fallback);
- `AbortSlew`;
- `SyncToCoordinates`;
- `Park` / `Unpark` та persistent `SetPark`, якщо `CanSetPark=true`;
- `PulseGuide`;
- `SiteLatitude`, `SiteLongitude`, `SiteElevation`, `UTCDate`;
- driver-reported `Azimuth`, `Altitude`, `SiderealTime`, `EquatorialSystem` та capabilities для diagnostics.

Непідтримані виклики та COM exceptions повертаються в OAL як явні помилки.

## Збірка/runtime

Опція:

```text
OAS_ENABLE_ASCOM_CLASSIC=ON
```

У Windows збирається:

```text
build/<preset>/oas-ascom-host.exe
```

Для runtime на машині node мають бути встановлені ASCOM Platform і вибраний Telescope driver. ASCOM Alpaca лишається окремим backend (`ascom-alpaca`).

## Site, epoch та сумісність з EQMOD

Observatory profile OpenAstroLink є канонічним місцем спостереження. При Classic ASCOM connect і безпосередньо перед GOTO OAL порівнює site самого driver із profile. Неправильний site може фізично віддзеркалити East/West slew навіть тоді, коли RA/DEC команда правильна. Якщо driver дозволяє site writes, OAL автоматично записує profile site/time і перевіряє результат.

У EQMOD опція **Allow Site Writes** часто вимкнена за замовчуванням. Якщо site різний і запис відхилений, OAL навмисно блокує GOTO. Відкрий **ASCOM Properties...**, задай ті самі latitude/longitude, що й в OAL, та/або ввімкни **ASCOM Options → Allow Site Writes**, після чого перепідключися.

`EquatorialSystem` читається за стандартом ASCOM: Topocentric=1, J2000=2. Старі/default конфігурації EQMOD можуть рекламувати `equOther=0`; лише для `EQMOD.*` OAL явно застосовує compatibility assumption default topocentric/JNow-style stream і пише його в diagnostics. Для відтворюваної роботи краще явно вибрати JNOW або J2000 в EQMOD Setup.

Axis-sign inversion у Mount tab стосується лише native raw-axis drivers і не змінює Classic ASCOM, бо motor↔sky transform належить EQMOD/ASCOM.

## Persistent спільний Park

Дія **Calibrate current physical pose as persistent Home / Park** має одну фізичну семантику, хоча зберігання backend-specific. Native EQDrive зберігає поточні raw axes як OAL Home+Park і вмикає automatic Home alignment. Classic ASCOM викликає стандартний `SetPark`, якщо driver рекламує `CanSetPark`.

Щоб native і EQMOD/ASCOM паркувалися в одну фізичну позу, один раз постав монтування в неї, відкалібруй native, перемкни backend без руху монтування і один раз відкалібруй Classic ASCOM. Обидві калібровки persistent; робити це кожної сесії не потрібно.
