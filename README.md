# OpenAstroSuite / OpenAstroLink

Версія 0.2.5 додає перший практичний Raspberry Pi hardware path поверх асинхронного node: production-adapter до ASTAP, hardened INDI mount/focuser transport, direct QHY SDK single-frame path і `oal-hardware-probe`. `openastrolink-node` володіє обладнанням і алгоритмами; той самий Qt GUI керує ним локально (`127.0.0.1`) або віддалено через OAL HTTP/WebSocket. Autofocus тепер є cancellable operation з lock-ами `camera+focuser`, mount slew — cancellable operation з lock-ом `mount`, а окремі camera/mount/focuser можна disconnect незалежно.

## Що входить

- `oas_core` — окрема бібліотека ядра без Qt Widgets.
- `openastrolink-node` — headless `QCoreApplication` для RPi/systemd.
- Qt 6 GUI: локальний node client, remote node client або embedded developer core; кадр, астрометричний overlay, карта зірок, профіль.
- Камери:
  - симулятор;
  - UVC/OpenCV;
  - віддалена OAL-камера;
  - QHY SDK — опційно; у v0.2.5 підтримує exact camera ID, 16-bit where available, ROI/binning/gain/offset та exposure abort;
  - Canon DSLR через libgphoto2 — опційно.
- Монтування:
  - симулятор;
  - LX200/SynScan-подібний serial через `QSerialPort`;
  - ASCOM Alpaca;
  - OAL;
  - INDI XML client — опційний backend для реального RPi mount/focuser path; використовує standard INDI properties і thread-safe per-call client sockets.
- Фокусери:
  - симулятор;
  - **GeminiAstro EAF / Automatic Astro Focuser Pro** — окремий compatibility profile через vendor-supported ASCOM/Alpaca або INDI transport;
  - ASCOM Alpaca;
  - OAL;
  - INDI — опційно.
- Виділення зірок, CSV-каталог, прототип triangle plate solver та ASTAP CLI adapter (автоматично preferred, якщо ASTAP встановлений).
- Motion estimator між наборами зірок.
- Autofocus:
  - `stars` — зоряна sharpness/HFR-подібна метрика;
  - `planet` — auto-ROI планети + Tenengrad;
  - `bahtinov` — дифракційна high-frequency метрика (не точний spike-offset solver).
  - coarse + fine scan, медіана кількох кадрів.
- Guiding engine з розрахунком похибки та pulse-guide через бекенд монтування.
- Polar alignment за реальною оцінкою осі RA з кількох solved orientations.
- Scheduler/session model.
- P0 operation manager: `queued/running/succeeded/failed/cancelled`, progress, cancellation, resource locks; у v0.2.4 мігровані autofocus, mount slew та camera exposure/capture. Exposure повертає 202 operation, тримає lock `camera`, а preview забирається окремим ресурсом після завершення.
- OAL REST API + WebSocket events, включно з node bootstrap/config endpoints, operation resources, cancellation та resource-lock telemetry для thin GUI client.
- Стабільний C ABI для майбутніх OAL plug-in драйверів і приклад драйвера.

## Швидкий старт

Потрібні:

- CMake 3.24+;
- Qt 6.4+ з модулями Core, Gui, Widgets, Network, SerialPort, WebSockets, HttpServer, Concurrent;
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

- `OAS_ENABLE_QHY`: потрібні `qhyccd.h` та бібліотека QHYCCD SDK; для нестандартного шляху задайте `QHYCCD_ROOT`.
- `OAS_ENABLE_GPHOTO2`: потрібен `libgphoto2` через pkg-config.
- `OAS_ENABLE_INDI`: використовує вбудований XML/TCP client для standard mount/focuser properties; runtime працює через `indiserver`, без libindi client ABI dependency.

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
- INDI mount/focuser: `host:7624/Exact Device Name`. Не вгадуйте назву: використайте `oal-hardware-probe`.
- QHY: scan index (`0`) або, краще для постійної інсталяції, exact QHY camera ID.

## Важлива чесна межа

Вбудований triangle solver лишається прототипом. У v0.2.5 доданий ASTAP adapter; якщо `astap_cli`/`astap` доступний на node, він стає preferred solver. Для ASTAP треба встановити сам program і одну star database.

`config/stars_example.csv` — демонстраційний яскравий каталог, а не повний науковий каталог.

Дивись:

- `docs/ARCHITECTURE.md`
- `docs/STATUS.md`
- `docs/OAL_API.md`
- `docs/BUILD_PLATFORMS.md`
- `docs/GEMINI_EAF.md`
- `docs/RPI_NODE.md`
- `docs/RPI_FIRST_HARDWARE.md`
- `docs/ROADMAP_P0_P1_IMPLEMENTATION.md`

## Перевірка цього пакета

Структурні перевірки, парсинг OpenAPI та окрема компіляція прикладу native OAL plug-in пройшли. Повну збірку GUI в середовищі консолідації виконати не вдалося через відсутність Qt 6/OpenCV development packages. Деталі: `docs/VALIDATION.md`.
