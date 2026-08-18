# Архітектура

```text
┌──────────────────────────────────────────────────────────────┐
│ Qt GUI                                                       │
│ Devices │ Capture/Solve │ Mount/Guide │ Focus │ Polar │ OAL │
└──────────────────────────────┬───────────────────────────────┘
                               │
                     ApplicationController
                               │
        ┌──────────────────────┼───────────────────────┐
        │                      │                       │
     ICamera                 IMount                 IFocuser
        │                      │                       │
  Sim/OpenCV/QHY/       Sim/Serial/Alpaca/      Sim/Alpaca/OAL/
  Canon/OAL              OAL/INDI                INDI
        │                      │                       │
        └─────────────── algorithms ──────────────────┘
          StarDetector, CatalogSolver, Autofocus,
          Guiding, PolarAlignment, Scheduler
                               │
               ┌───────────────┴───────────────┐
               │                               │
          OAL HTTP API                   OAL WebSocket
```

## Головний принцип

GUI, OAL HTTP server і внутрішні алгоритми не мають власних копій класів пристроїв. Вони викликають `ApplicationController`, який володіє єдиними активними `ICamera`, `IMount` та `IFocuser`.

## Шари

### `core`

- спільні типи;
- інтерфейси пристроїв;
- налаштування;
- `ApplicationController`.

### `backends`

Перекладають стандартизовані операції ядра у конкретний транспорт/SDK.

### `algorithms`

Не знають про GUI або HTTP. Отримують кадри та інтерфейси пристроїв.

### `oal`

- REST API;
- WebSocket events;
- plug-in ABI/loader.

### `gui`

Виключно presentation layer.

## Полярне вирівнювання

Кожен plate solve задає орієнтацію камери у небесній системі. Відносна матриця орієнтацій після повороту лише RA має власний вектор — реальну механічну вісь RA. Вісь усереднюється за кількома проходами, порівнюється з NCP і переводиться у локальні Alt/Az поправки.


## Hardware compatibility profiles

`gemini-eaf` є першим named hardware compatibility profile поверх `IFocuser`. Він делегує vendor-supported Alpaca/INDI transport, але зберігає власну OAL/backend identity. Це тимчасовий міст до P0 typed capabilities: клієнт не повинен виводити можливості пристрою лише з назви профілю. Деталі: `GEMINI_EAF.md`.
