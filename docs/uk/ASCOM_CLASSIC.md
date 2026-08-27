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
- `Tracking`, `Slewing`, `AtPark`, `SideOfPier`;
- `SlewToCoordinatesAsync` (або synchronous fallback);
- `AbortSlew`;
- `SyncToCoordinates`;
- `Park` / `Unpark`;
- `PulseGuide`.

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
