# OpenAstroSuite / OpenAstroLink

Версія 0.2.2 вводить окремий headless observatory core для Raspberry Pi. `openastrolink-node` володіє обладнанням і алгоритмами, а той самий Qt GUI може керувати вузлом локально (`127.0.0.1`) або віддалено через OAL HTTP/WebSocket. Старий embedded-core режим збережено як developer fallback.

## Що входить

- `oas_core` — окрема бібліотека ядра без Qt Widgets.
- `openastrolink-node` — headless `QCoreApplication` для RPi/systemd.
- Qt 6 GUI: локальний node client, remote node client або embedded developer core; кадр, астрометричний overlay, карта зірок, профіль.
- Камери:
  - симулятор;
  - UVC/OpenCV;
  - віддалена OAL-камера;
  - QHY SDK — опційно;
  - Canon DSLR через libgphoto2 — опційно.
- Монтування:
  - симулятор;
  - LX200/SynScan-подібний serial через `QSerialPort`;
  - ASCOM Alpaca;
  - OAL;
  - INDI XML client — опційний експериментальний бекенд.
- Фокусери:
  - симулятор;
  - **GeminiAstro EAF / Automatic Astro Focuser Pro** — окремий compatibility profile через vendor-supported ASCOM/Alpaca або INDI transport;
  - ASCOM Alpaca;
  - OAL;
  - INDI — опційно.
- Виділення зірок, CSV-каталог і прототип triangle plate solver.
- Motion estimator між наборами зірок.
- Autofocus:
  - `stars` — зоряна sharpness/HFR-подібна метрика;
  - `planet` — auto-ROI планети + Tenengrad;
  - `bahtinov` — дифракційна high-frequency метрика (не точний spike-offset solver).
  - coarse + fine scan, медіана кількох кадрів.
- Guiding engine з розрахунком похибки та pulse-guide через бекенд монтування.
- Polar alignment за реальною оцінкою осі RA з кількох solved orientations.
- Scheduler/session model.
- OAL REST API + WebSocket events, включно з node bootstrap/config endpoints для thin GUI client.
- Стабільний C ABI для майбутніх OAL plug-in драйверів і приклад драйвера.

## Швидкий старт

Потрібні:

- CMake 3.24+;
- Qt 6.4+ з модулями Core, Gui, Widgets, Network, SerialPort, WebSockets, HttpServer;
- OpenCV 4.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Для Raspberry Pi рекомендований запуск — окремий node service плюс GUI-клієнт. Першу перевірку все одно роби з трьома `simulated` backends.

```bash
./build/openastrolink-node --http-port 8080 --ws-port 8090
./build/OpenAstroSuite --node http://127.0.0.1:8080
```

На іншому комп'ютері запускається той самий GUI з адресою RPi. Якщо до RPi підключено монітор і клавіатуру, GUI підключається до локального node через `127.0.0.1`, тому закриття GUI не зупиняє ядро або обладнання.

## Опційні бекенди

```bash
cmake -S . -B build \
  -DOAS_ENABLE_QHY=ON \
  -DOAS_ENABLE_GPHOTO2=ON \
  -DOAS_ENABLE_INDI=ON
```

- `OAS_ENABLE_QHY`: потрібні `qhyccd.h` та бібліотека QHYCCD SDK.
- `OAS_ENABLE_GPHOTO2`: потрібен `libgphoto2` через pkg-config.
- `OAS_ENABLE_INDI`: використовує вбудований мінімальний XML/TCP клієнт, без libindi.

## Формати endpoint у GUI

- OpenCV camera: `0`.
- Serial LX200: `COM3`, `/dev/ttyUSB0`, `/dev/ttyACM0`.
- Alpaca mount: `http://host:port/api/v1/telescope/0`.
- Alpaca focuser: `http://host:port/api/v1/focuser/0`.
- GeminiAstro EAF через ASCOM Remote/Alpaca: backend `gemini-eaf`, endpoint `alpaca:http://host:port/api/v1/focuser/0`.
- GeminiAstro EAF через INDI: backend `gemini-eaf`, endpoint `indi:host:7624/Exact Device Name` (потрібен `OAS_ENABLE_INDI=ON`).
- OAL mount: `http://host:8080/api/v1/mounts/default`.
- OAL focuser: `http://host:8080/api/v1/focusers/default`.
- OAL camera: `http://host:8080/api/v1/cameras/main`.
- INDI: `host:7624/Exact Device Name`.

## Важлива чесна межа

Вбудований triangle solver є прототипом для невеликого каталогу або підказаного поля. Він не замінює astrometry.net, ASTAP чи індекси повного неба. Для виробничої blind-astrometry потрібен індексований quad solver або адаптер до зовнішнього солвера.

`config/stars_example.csv` — демонстраційний яскравий каталог, а не повний науковий каталог.

Дивись:

- `docs/ARCHITECTURE.md`
- `docs/STATUS.md`
- `docs/OAL_API.md`
- `docs/BUILD_PLATFORMS.md`
- `docs/GEMINI_EAF.md`
- `docs/RPI_NODE.md`
- `docs/ROADMAP_P0_P1_IMPLEMENTATION.md`

## Перевірка цього пакета

Структурні перевірки, парсинг OpenAPI та окрема компіляція прикладу native OAL plug-in пройшли. Повну збірку GUI в середовищі консолідації виконати не вдалося через відсутність Qt 6/OpenCV development packages. Деталі: `docs/VALIDATION.md`.
