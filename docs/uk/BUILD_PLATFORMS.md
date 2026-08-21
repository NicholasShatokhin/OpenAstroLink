# Платформи збірки — v0.2.10

Канонічна версія документа: `docs/BUILD_PLATFORMS.md`. Українська версія є перекладом.

OpenAstroSuite більше не прив'язаний до Raspberry Pi. Windows x64 ПК, Linux x86_64 ПК або Linux ARM64/Raspberry Pi можуть самі бути **observatory node**, якщо обладнання під'єднане безпосередньо до цього комп'ютера. Той самий GUI може працювати локально або підключатися до іншого node через OAL HTTP/WebSocket.

## Першокласні цілі

| Платформа | GUI | Node | Native drivers | INDI compatibility |
|---|---:|---:|---|---:|
| Windows x64 / MSVC 2022 | так | так | QHY, Canon EDSDK, ZWO ASI/EAF, Gemini, Sky-Watcher | опційно |
| Linux x86_64 | так | так | QHY, Canon libgphoto2, ZWO ASI/EAF, Gemini, Sky-Watcher | опційно |
| Linux ARM64 / Raspberry Pi | так | так | ті самі Linux drivers | опційно |
| Linux headless | ні | так | вибірково | опційно |

Native OAL є основним шляхом. INDI легко вмикається для обладнання, для якого ще немає native OAL driver.

## Presets

`CMakePresets.json` містить лише portable presets без локальних шляхів SDK. Локальні шляхи тримаємо у `CMakeUserPresets.json`, який ігнорується Git. Приклад: `CMakeUserPresets.example.json`.

Основні presets:

- `windows-core-release` — Windows без vendor camera SDK;
- `windows-native-release` — усі native drivers, INDI off;
- `windows-observatory-release` — native + INDI;
- `linux-native-release` — усі native drivers, INDI off;
- `linux-observatory-release` — native + INDI;
- `linux-node-release` — headless Linux node;
- `rpi4-native-release`, `rpi4-observatory-release` — ARM64/RPi;
- `node-sim-release` — лише simulator.

## Windows x64

Рекомендовано MSVC 2022 x64 + Qt 6 MSVC2022_64 + CMake/Ninja + сумісний OpenCV. Не змішувати MinGW бібліотеки з MSVC.

```powershell
Copy-Item CMakeUserPresets.example.json CMakeUserPresets.json
# Відредагувати my-windows-observatory.
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
```

Для Canon `AUTO` на Windows означає Canon EDSDK. Потрібні `CANON_EDSDK_INCLUDE_DIR`, `CANON_EDSDK_LIBRARY`, а для packaging — `CANON_EDSDK_RUNTIME_DIR`.

Для QHY/ZWO використовуються саме Windows x64 `.lib/.dll`; Linux `.so` під MSVC не підходить.

## Linux x86_64

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  libopencv-dev libgphoto2-dev libjpeg-dev
```

Після встановлення x86_64 QHY/ZWO SDK:

```bash
cmake --preset my-linux-observatory
cmake --build --preset my-linux-observatory -j$(nproc)
sudo cmake --install build/linux-observatory
```

На Linux `OAS_CANON_TRANSPORT=AUTO` вибирає libgphoto2. Якщо встановлено Canon Linux EDSDK, можна явно вибрати `EDSDK`.

## RPi / Linux ARM64

RPi тепер використовує ту саму модель, що й desktop Linux; різняться лише архітектура SDK та systemd deployment. Перевіряти vendor libraries командою `file`: для 64-bit RPi потрібен AArch64/ARM64.

## Якщо залізо під'єднане прямо до ПК

```text
QHY / ZWO / Canon / Gemini / Sky-Watcher
                 │ USB / serial
                 ▼
          openastrolink-node
             ┌────┴─────┐
       local GUI    remote GUI
                       + Stellarium
```

Тобто RPi — один із deployment modes, а не обов'язковий посередник.

## Packaging

Windows:

```powershell
./scripts/package_windows.ps1 -BuildDir build/windows-observatory \
  -QtBin C:/Qt/6.10.0/msvc2022_64/bin \
  -OpenCvBin C:/opencv/opencv/build/x64/vc16/bin \
  -VendorRuntimeDirs @("C:/SDK/QHY/bin","C:/SDK/ZWO/ASI/bin","C:/SDK/ZWO/EAF/bin","C:/SDK/Canon/EDSDK/Dll") -Zip
```

Linux:

```bash
./scripts/package_linux.sh build/linux-observatory dist/linux-observatory
```

Vendor SDK libraries не копіюються на Linux автоматично через різні ліцензійні умови.

## Доступ до пристроїв у Linux

```bash
sudo usermod -aG dialout "$USER"
```

Також встановити udev rules QHY/ZWO та не дозволяти desktop photo manager захоплювати Canon EOS раніше за OAL.

`build_features.json` генерується при configure і дозволяє точно бачити, які hardware transports увійшли в конкретну збірку.
