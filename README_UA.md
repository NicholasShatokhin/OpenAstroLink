# OpenAstroSuite / OpenAstroLink


**Реліз сумісності mount 0.2.10.16:** додано експериментальний native `oal.eqdrive` ASTEP driver зі збереженням наявного Sky-Watcher/EqMount driver, Windows Classic ASCOM backend з ASCOM Chooser та ізольованим helper-процесом, мережеві SynScan/SynScan Pro backends (UDP 11881 і TCP 11882 compatibility), а також коректне збереження/міграцію вибраного native serial-порту для Gemini/EQDrive/Sky-Watcher.

**Поточний реліз: v0.2.10.16 — EQDrive + Classic ASCOM + SynScan network**

Англійська документація є канонічною. Українські дзеркала знаходяться у `README_UA.md` та `docs/uk/`.

OpenAstroLink (OAL) — local-first стек керування обсерваторією. Основний шлях до обладнання — **нативні OAL-драйвери**; INDI, ASCOM Alpaca та LX200 залишаються опційними шарами сумісності для обладнання, для якого ще немає native OAL driver.

## Поточне покриття обладнання

Код native OAL driver вже є для:

- камер QHY (`oal.qhy`, QHYCCD SDK);
- Canon EOS (`oal.canon`: Canon EDSDK на Windows, libgphoto2/PTP на Linux);
- камер ZWO ASI (`oal.zwo.asi`, ASI SDK);
- фокусерів ZWO EAF (`oal.zwo.eaf`, EAF SDK);
- Gemini EAF (`oal.gemini`, прямий serial protocol);
- Sky-Watcher/SynScan (`oal.skywatcher`, наявний direct serial/EqMount path);
- EQDrive (`oal.eqdrive`, експериментальний direct ASTEP serial path).

Драйвери реалізовані в коді, але ще потребують HIL-кваліфікації на конкретній ОС, CPU-архітектурі, пристрої та firmware перед позначкою production-ready.


## Сумісність mount у v0.2.10.16

- **Native EQDrive:** `oal.eqdrive` є окремим драйвером і не замінює `oal.skywatcher`. Discovery використовує лише read-only команди; початкова RA/Dec-модель консервативна (`sync-anchor`) і потребує одного Sync перед нативним celestial GOTO. Див. `docs/uk/EQDRIVE.md`.
- **Classic ASCOM (Windows):** backend `ascom-classic` працює через `oas-ascom-host.exe`, встановлену ASCOM Platform і зареєстрований Telescope driver; GUI має **ASCOM Chooser...** та **ASCOM Properties...**. Це compatibility path для EQMOD-подібної роботи. Див. `docs/uk/ASCOM_CLASSIC.md`.
- **SynScan network:** `synscan-app` використовує багатший SynScan App Protocol через UDP 11881; `synscan-wifi` — serial-protocol compatibility server через TCP 11882. Host — телефон/ПК із запущеним SynScan Pro. Див. `docs/uk/SYNSCAN_NETWORK.md`.
- **Вибір Gemini COM:** вибір порту в **Native serial discovery** тепер зберігає override та мігрує старий native Gemini backend binding на пристрій, знайдений на новому порту. CLI `--gemini-port` лишається пріоритетним для поточного процесу.

## Runtime-модель

```text
QHY / Canon / ZWO / Gemini / Sky-Watcher / EQDrive
                    │ USB / serial
                    ▼
             openastrolink-node
               ┌────┴─────┐
          local GUI   remote GUI
                         │
                     Stellarium

Опційний compatibility path:
INDI / Classic ASCOM / ASCOM Alpaca / SynScan network / LX200
```

Node володіє обладнанням і довгими операціями. Закриття GUI не повинно зупиняти node або від'єднувати пристрої.

---

# Швидкий старт збірки

## CMake та сумісність presets

У v0.2.10.11 використовується **CMake preset schema v2**, щоб presets читалися CMake 3.20+ і не вимагали CMake 3.25+ лише через формат JSON.

Перевір:

```bash
cmake --version
```

Потрібно:

```text
CMake >= 3.20
```

Якщо зі старого checkout залишився `CMakeUserPresets.json` з:

```json
"version": 6
```

старіший Linux CMake може завершитися помилкою:

```text
Unrecognized "version" field
```

Для v0.2.10.11 створіть user preset заново:

```bash
rm -f CMakeUserPresets.json
cp CMakeUserPresets.example.json CMakeUserPresets.json
```

Потім змінюються лише локальні SDK paths.

Preflight:

```bash
./scripts/check_build_environment.sh
```

Windows PowerShell:

```powershell
.\scripts\check_build_environment.ps1
```

---

# Windows x64

## Єдиний підтримуваний ABI для цієї збірки

```text
MSVC 2022 x64
+ Ninja
+ Qt 6 MSVC2022_64
+ OpenCV для MSVC
+ Windows x64 vendor .lib/.dll
```

Не змішуйте MinGW/Strawberry GCC з Qt `msvc2022_64` або vendor `.lib`.

`scripts/build_windows.ps1` намагається сам завантажити `vcvars64.bat`, тому зазвичай можна запускати зі звичайного PowerShell.

## SDK

### QHY

Приклад поточного layout:

```text
C:/Program Files/QHYCCD/AllInOne/sdk/include/qhyccd.h
C:/Program Files/QHYCCD/AllInOne/sdk/x64/qhyccd.lib
C:/Program Files/QHYCCD/AllInOne/sdk/x64/*.dll
```

OAL ізолює QHY headers від CRT/STL include path, бо деякі QHY SDK містять власні `stdint.h`/`stdint_windows.h`.

### ZWO ASI

Потрібні `ASICamera2.h`, Windows x64 import library та runtime DLL.

### ZWO EAF

Потрібні `EAF_focuser.h`, Windows x64 import library та runtime DLL.

### Canon EDSDK

EDSDK опційний. Без нього використовуйте `my-windows-observatory`, де:

```json
"OAS_ENABLE_NATIVE_CANON": "OFF"
```

Після встановлення EDSDK використовуйте `my-windows-observatory-edsdk`.

## Налаштування і збірка

```powershell
Copy-Item CMakeUserPresets.example.json CMakeUserPresets.json
```

Відредагуйте локальні шляхи і запустіть:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

Або вручну з MSVC x64 environment:

```powershell
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
```

З Canon EDSDK:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory-edsdk -Clean
```

## Packaging

```powershell
.\scripts\package_windows.ps1 `
  -BuildDir build/windows-observatory `
  -QtBin C:/Qt/6.10.0/msvc2022_64/bin `
  -OpenCvBin C:/opencv/opencv/build/x64/vc16/bin `
  -VendorRuntimeDirs @(
    "C:/Program Files/QHYCCD/AllInOne/sdk/x64",
    "C:/SDK/ZWO/ASI SDK/lib/x64",
    "C:/SDK/ZWO/EAF/lib/Windows/x64/Release"
  ) `
  -Zip
```

---

# Native Linux (x86_64 або ARM64/RPi)

Той самий node і GUI можуть напряму володіти обладнанням на desktop Linux, mini-PC або Raspberry Pi.

WSL зручний для compile/test, але для реальної обсерваторії краще native Linux, якщо USB/serial passthrough у WSL не налаштовано спеціально.

## Базові залежності Debian/Ubuntu

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  libopencv-dev libgphoto2-dev libjpeg-dev
```

Назви пакетів можуть трохи відрізнятися між дистрибутивами.

## Canon на Linux

Default transport — **libgphoto2**, тому шлях до EDSDK не потрібен:

```text
Canon EOS → USB/PTP → libgphoto2 → oal.canon → OAL ABI v2
```

Перевірка:

```bash
pkg-config --modversion libgphoto2
```

## QHY Linux SDK

Рекомендований layout:

```text
/opt/openastrolink-sdk/qhy/include/qhyccd.h
/opt/openastrolink-sdk/qhy/lib/libqhy.so
```

Після розпакування SDK:

```bash
sudo mkdir -p /opt/openastrolink-sdk/qhy/include /opt/openastrolink-sdk/qhy/lib
sudo cp /path/to/qhy/include/*.h /opt/openastrolink-sdk/qhy/include/
sudo cp /path/to/qhy/lib/libqhy.so* /opt/openastrolink-sdk/qhy/lib/
```

Якщо є лише versioned `.so`:

```bash
cd /opt/openastrolink-sdk/qhy/lib
sudo ln -sf libqhy.so.0.1.8 libqhy.so
```

Обов'язково:

```bash
uname -m
file /opt/openastrolink-sdk/qhy/lib/libqhy.so
```

Архітектури повинні збігатися.

Для WSL compile test можна використовувати `/mnt/c/...`, але `.so` має бути Linux x86_64, якщо `uname -m` показує `x86_64`.

Для поточних каталогів QHY у WSL приклад:

```json
"QHYCCD_INCLUDE_DIR": "/mnt/c/workspace/astro/QHYCCD_Linux",
"QHYCCD_LIBRARY": "/mnt/c/workspace/astro/qhysdk/lib/libqhy.so.0.1.8"
```

Перед збіркою:

```bash
uname -m
file /mnt/c/workspace/astro/qhysdk/lib/libqhy.so.0.1.8
```

## ZWO ASI/EAF Linux SDK

Рекомендовані layouts:

```text
/opt/openastrolink-sdk/zwo/asi/include/ASICamera2.h
/opt/openastrolink-sdk/zwo/asi/lib/libASICamera2.so

/opt/openastrolink-sdk/zwo/eaf/include/EAF_focuser.h
/opt/openastrolink-sdk/zwo/eaf/lib/libEAFFocuser.so
```

Перевіряйте архітектуру бібліотек через `file`.

## Права на USB/serial

```bash
sudo usermod -aG dialout "$USER"
```

Після цього потрібен logout/login. Також встановіть vendor udev rules для QHY/ZWO, якщо вони входять до SDK/driver package.

## Збірка

```bash
cp CMakeUserPresets.example.json CMakeUserPresets.json
./scripts/build_linux.sh my-linux-observatory
```

Або:

```bash
cmake --preset my-linux-observatory
cmake --build --preset my-linux-observatory -j"$(nproc)"
```

Встановлення:

```bash
sudo cmake --install build/linux-observatory
sudo ldconfig
```

Headless node:

```bash
cmake --preset linux-node-release
cmake --build --preset linux-node-release -j"$(nproc)"
```

---

# INDI compatibility

INDI легко вмикається для іншого обладнання, але не є залежністю native OAL drivers.

```text
*-native-release       тільки native OAL
*-observatory-release  native OAL + INDI compatibility client
```

`indiserver` потрібен лише тоді, коли реально використовується INDI-пристрій.

# Перший HIL

Перевіряйте поетапно:

1. чиста збірка/package;
2. `oal-hardware-probe`;
3. mount: status → малий GOTO → abort → tracking → sync → guide pulse;
4. focuser: position → невеликі move → limits → reconnect;
5. camera: короткі exposure → settings → cancel → повторні capture → reconnect;
6. ASTAP;
7. autofocus;
8. closed-loop pointing і polar alignment;
9. guiding та DSO/planetary workflows.

Див. `docs/uk/VALIDATION.md` та `docs/uk/OAL_SPECIFICATION.md`.

# Поточні межі

Проєкт готовий до подальшої supervised HIL-кваліфікації, але ще не до unattended Internet-facing observatory. Потрібно завершити replayable WebSocket events, повний RFC 9457 HTTP error model, idempotency, durable FITS/RAW/SER data plane, production guiding, durable session recovery, safety/weather/roof policy, TLS/auth/scopes/audit, out-of-process driver sandboxing та conformance suite.


## v0.2.10.11 — reset-aware пошук Gemini EAF у Windows

HIL-перевірка Gemini EAF через USB-UART CH340 у Windows показала, що після відкриття `COMx` контролеру може бути потрібно близько двох секунд, перш ніж команда `:02#` стабільно повертає `EOK#`. Нативний драйвер `oal.gemini` тепер після відкриття COM-порту витримує повний quiet-open інтервал (`openSettleMs`, зараз 2200 мс) до першої команди `:02#`. Якщо відповіді все ще немає — **не закриває порт**, чекає додаткове recovery-вікно (`resetRecoveryMs`, зараз 1200 мс) і повторює handshake один раз. Це працює і для `--gemini-port COMx`, і для автоматичного сканування COM-портів. Повторне відкриття між спробами навмисно не використовується, бо воно може знову перезапустити контролер через USB-UART.

## v0.2.10.11: примітки Windows HIL щодо runtime/discovery

Цей hotfix закриває три проблеми, знайдені під час першого Windows hardware-in-the-loop запуску:

1. Discovery нативних пристроїв тепер кешується. Звичайні запити `/api/v1/node/backends`, `/api/v1/state` та старт GUI **не** запускають повторне сканування USB/serial обладнання. Повний scan виконується один раз до повідомлення node ready, а надалі лише явною дією **Refresh native device discovery** (`POST /api/v1/drivers/refresh`). Це прибирає ситуацію, коли serial probing перевищував 3-секундний timeout remote GUI.
2. Manifests ZWO ASI/EAF приведені до ABI-v2 contract (`schema` та `permissions` як масив).
3. На Windows `QHYCCD_RUNTIME_DIR`, `ZWO_ASI_RUNTIME_DIR`, `ZWO_EAF_RUNTIME_DIR` та `CANON_EDSDK_RUNTIME_DIR` тепер реально використовуються для staging у build tree, а не лише як packaging hints. DLL із цих каталогів копіюються поруч із executable та native drivers під час configure, тому development build не потребує ручного розширення `PATH`.

Після зміни шляхів до vendor SDK повторно виконай CMake configure, щоб staging DLL оновився.

Якщо `CMakeUserPresets.json` був скопійований зі старої версії, постав верхній schema field `"version": 2` для сумісності з CMake 3.20+ та старішими WSL-дистрибутивами. Канонічний шаблон — `CMakeUserPresets.example.json` у репозиторії.

## v0.2.10.11 — ізоляція Windows CRT runtime

Не можна копіювати весь vendor SDK runtime-каталог у каталог програми. Деякі Windows-пакети QHY містять старі `msvcr90.dll` / `msvcp90.dll`; app-local копії можуть конфліктувати з runtime MSVC 2022 і викликати `R6034` ще до нормального старту node.

У v0.2.10.11 копіюються vendor/device DLL, але Microsoft CRT/UCRT redistributables відфільтровуються. Microsoft runtimes потрібно встановлювати штатним redistributable package, а не брати з SDK-каталогів.

Після оновлення з v0.2.10.4 один раз зробіть clean build:

```bat
rmdir /s /q build\windows-observatory
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
powershell -ExecutionPolicy Bypass -File scripts\diagnose_windows_runtime.ps1 -BuildDir build/windows-observatory
```

### v0.2.10.11: виправлення Gemini manifest timing

HIL Gemini/CH340 показав, що manifest драйвера перезаписував C++ reset-settle default значенням `openSettleMs=150`. Runtime defaults тепер синхронізовані: 2200 мс quiet-open settle та додатковий 1200 мс retry на тому самому відкритому порту. Hardware probe друкує ефективні значення таймінгів.


### v0.2.10.11 — коректне завершення node у Windows

Windows HIL виявив race під час завершення `openastrolink-node` через `Ctrl+C`: queued wakeup із worker thread міг прийти вже під час руйнування прихованого message window `QEventDispatcherWin32`, через що з'являлося `QEventDispatcherWin32::wakeUp: Failed to post a message (Invalid window handle.)`, а процес інколи зависав.

Node тепер встановлює мінімальний console control handler. Він не викликає Qt, а лише фіксує interrupt. Qt-таймер у main thread кожні 50 мс перетворює перший `Ctrl+C`/`Ctrl+Break` на `QCoreApplication::quit()`. Cleanup підключений до `aboutToQuit`, тому listeners, operation workers, device connections, native-driver worker threads і serial sessions зупиняються, поки Qt event dispatcher ще валідний. Другий `Ctrl+C` навмисно передається стандартному Windows handler як аварійний force-terminate.

Очікуваний лог завершення:

```text
Console interrupt received; starting graceful shutdown. Press Ctrl+C again to force termination.
OpenAstroLink node shutdown: stopping listeners and active work
OpenAstroLink node shutdown complete
```

Windows HIL: на реальному обладнанні вже підтверджені native discovery/connection Gemini EAF, фактичний рух фокусера та рух під час autofocus. Окремо ще треба кваліфікувати збіжність і повторюваність autofocus на реальній оптичній цілі.


## v0.2.10.16 — адаптивний plate solving для міського неба

Для міської засвітки та недостатньо точної полярки node більше не залежить від схеми `один довгий кадр -> один запуск ASTAP`. Нова operation `solver.adaptive` починає з короткого кадру, а за потреби переходить до серій коротких експозицій, вирівнює їх за зорями, складає, прибирає великомасштабний градієнт фону та повторює solve. Остання спроба завжди запускає solver навіть якщо локальна оцінка кількості зір песимістична.

У GUI є кнопка **Adaptive urban capture + solve**. Типові параметри: binning 2x2, 3 кадри для проміжного stack, 5 для фінального, максимум 3 с на один кадр і ціль 20 локально детектованих зір. Базова експозиція та gain беруться зі звичайних Capture-полів. Якщо mount підключений, його поточні RA/Dec автоматично використовуються як hint, а radius пошуку розширюється 5° -> 10° -> до заданого максимуму.

REST: `POST /api/v1/solve/adaptive`. Результат operation містить діагностику кожної спроби та `solverFrameId`, який можна відкрити через звичайний frame preview.

### v0.2.10.16 — виправлення HIL discovery/state

- Перед кожною повторною спробою auto-connect виконується повторний native discovery, тому QHY, що з’явилася після старту node, може підключитися без перезапуску.
- Native Gemini move тепер асинхронний на межі драйвера: `focuser.status` доступний під час руху й повертає поточну позицію та `moving`. Autofocus окремо чекає завершення фізичного руху перед експозицією.
- GUI опитує стан фокусера під час ручного руху.
- Native Sky-Watcher discovery тепер детально логує кожен COM-порт і SynScan handshake `KO -> O#`, а також робить retry на тому самому відкритому порту. Поточний RA/DEC native mount device усе ще орієнтований на протокол SynScan hand controller; direct USB/EQDIR motor-controller transport залишається окремим незавершеним шляхом.
