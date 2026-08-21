# OpenAstroSuite / OpenAstroLink

**v0.2.7 — Native Telescope Hardware Pack**

Цей реліз робить native OpenAstroLink reference path для цілого телескопа, а не лише для камери:

```text
OpenAstroSuite GUI (локально на RPi або віддалено)
                         │
                  OAL HTTP / WebSocket
                         │
               openastrolink-node
                         │
              Native OAL ABI v2 registry
                  ┌──────┼──────────┐
                  │      │          │
               oal.qhy oal.gemini oal.skywatcher
                  │      │          │
               QHY SDK  USB serial  SynScan serial
                  │      │          │
                 QHY  Gemini EAF  Sky-Watcher mount

Compatibility / migration layer (optional):
INDI · ASCOM Alpaca · LX200 · other existing device ecosystems
```

Native OAL є основною архітектурою. Compatibility backends залишаються першокласним способом підключити обладнання, для якого native driver ще не написаний, але вони не визначають capabilities або семантику OAL.

## Головне у v0.2.7

- **Native QHY `oal.qhy`** — direct QHYCCD SDK, без INDI у reference path.
- **Native Gemini EAF `oal.gemini`** — direct 9600-baud serial implementation сімейства MyFocuserPro2, з protocol probe, position/moving/temperature/max-position, absolute/relative move та capability discovery.
- **Native Sky-Watcher `oal.skywatcher`** — direct SynScan hand-controller serial protocol v3.3: precise J2000 RA/DEC, GOTO, abort, sync, tracking, alignment state, pier-side where available and fixed-rate pulse guiding on equatorial models.
- Native serial drivers тримають **persistent serial session in a dedicated I/O thread**. Це уникає повторного open/DTR reset для MCU-based focusers і не порушує Qt QObject thread affinity, коли OAL operations виконуються на різних worker threads.
- Sky-Watcher `mount.slew` тепер лише приймає GOTO і повертає керування. Довгий lifecycle належить OAL `mount.slew` operation, тому `ABORT MOUNT MOTION` лишається responsive.
- Для Sky-Watcher нижчий Motor Controller Command Set уже має окремий codec foundation, але direct-axis RA/DEC driver **не оголошується готовим**, доки не буде OAL alignment/coordinate model. Ніяких вигаданих axis→sky перетворень.
- Gemini `halt` **не рекламується як supported**, доки точна stop-команда не буде підтверджена на цільовій firmware. Це свідомий safety choice.
- Доданий `oal-native-protocol-smoke` без Qt/hardware для перевірки wire-format кодеків.
- `oal-hardware-probe --require-native-telescope` перевіряє, що знайдені QHY + Gemini + Sky-Watcher native devices.

## INDI: легко увімкнути, але не обов'язково використовувати

Звичайні desktop/observatory builds мають INDI compatibility compiled in за замовчуванням. Сам `indiserver` не потрібно запускати, доки не з'явиться INDI-only обладнання.

На Raspberry Pi є два явні presets:

```bash
# Доказ/мінімальна система: лише native telescope path
cmake --preset rpi4-native-release

# Рекомендована обсерваторна збірка: native-first + INDI available
cmake --preset rpi4-observatory-release
```

Встановити INDI runtime пізніше:

```bash
sudo ./scripts/enable_indi_compat.sh
```

Або відразу при bootstrap:

```bash
sudo ./scripts/bootstrap_rpi_observatory.sh --with-indi
```

Таким чином INDI може бути повністю відсутнім у native-only build, але для іншого обладнання його підключення — одна опція CMake + один installer script.

## Build

Потрібні CMake 3.24+, Qt 6.4+ і OpenCV 4.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Native QHY із точними SDK paths

Для RPi/Linux:

```bash
cmake --preset rpi4-observatory-release \
  -DQHYCCD_INCLUDE_DIR=/opt/qhyccd/include \
  -DQHYCCD_LIBRARY=/opt/qhyccd/lib/libqhy.so
cmake --build --preset rpi4-observatory-release -j$(nproc)
```

Підтримується й `QHYCCD_ROOT` як fallback.

Для наявного Windows-каталогу SDK є helper:

```powershell
.\scripts\copy_qhy_sdk_to_rpi.ps1 -RemoteHost <RPI-IP>
```

За замовчуванням він бере headers з `C:\workspace\astro\QHYCCD_Linux` і Linux library з `C:\workspace\astro\qhysdk\lib\libqhy.so.0.1.8`; `stage_qhy_sdk.sh` на Pi перевіряє архітектуру бібліотеки перед install.

## RPi first native telescope build

```bash
sudo ./scripts/bootstrap_rpi_observatory.sh --with-indi   # --with-indi optional

# stage/install QHY ARM64 SDK + udev rules, install ASTAP/database

cmake --preset rpi4-observatory-release \
  -DQHYCCD_INCLUDE_DIR=/opt/qhyccd/include \
  -DQHYCCD_LIBRARY=/opt/qhyccd/lib/libqhy.so
cmake --build --preset rpi4-observatory-release -j$(nproc)

./build/rpi4-observatory/oal-hardware-probe --require-native-telescope
sudo ./scripts/install_rpi_node.sh build/rpi4-observatory
```

Для стабільного serial binding рекомендується `/etc/openastrolink/node.env`:

```bash
OAL_GEMINI_PORT=/dev/serial/by-id/<gemini>
OAL_SKYWATCHER_PORT=/dev/serial/by-id/<mount>
```

Без цих змінних native drivers роблять protocol probing доступних serial ports.

## Native hardware feature boundary

### Gemini EAF

Реалізовано: discovery/handshake, firmware, controller position, moving state, max position, temperature when present, absolute/relative move, limits, persistent serial session.

Не оголошено готовим: hardware halt. До підтвердження точної команди capability повертає `supported:false`. Перший HIL тест має починатися з малих переміщень і механічно безпечного діапазону.

### Sky-Watcher/SynScan

Реалізовано: serial v3.3 handshake, mount model/firmware/alignment, precise J2000 RA/DEC, GOTO, abort, sync, tracking, GOTO progress, pointing/pier state where supported, fixed-rate pulse guide для EQ mounts.

`park` повертає `NOT_SUPPORTED`, тому що SynScan serial protocol v3.3 не має нормативної park-команди. Park workflow буде доданий на OAL service layer або через direct motor-controller profile після окремої hardware qualification.

Direct Sky-Watcher Motor Controller protocol має codec groundwork (і офіційно може працювати serial/USB або через SynScan Wi-Fi UDP 11880), але RA/DEC direct-axis control потребує alignment/model math; він не маскується під готовий driver.

## Поточні наступні P0/P1 кроки

- HIL qualification native Gemini + native Sky-Watcher на конкретному телескопі;
- async ASTAP + closed-loop GOTO/recenter;
- automatic polar-alignment workflow поверх native mount/camera;
- QHY live/ring-buffer/SER;
- FITS/RAW durable data plane;
- capability-driven GUI (не показувати Park/Halt як доступні, якщо driver їх не підтримує);
- out-of-process sandbox driver host;
- idempotency, RFC 9457 errors, replayable WebSocket events, TLS/auth/safety.

Деталі: `docs/RPI_FIRST_HARDWARE.md`, `docs/NATIVE_DRIVER_SDK.md`, `docs/STATUS.md`, `docs/VALIDATION.md`.
