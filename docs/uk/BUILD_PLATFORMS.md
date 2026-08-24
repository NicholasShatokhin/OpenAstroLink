# Платформи збірки — v0.2.10.5

## Підтримувані observatory hosts

Windows x64, Linux x86_64, Linux ARM64/RPi та headless Linux. Обладнання може бути підключене напряму до desktop host; RPi не є обов'язковим.

## CMake

Presets тепер мають **schema version 2** і потребують **CMake 3.20+**.

Помилка:

```text
Unrecognized "version" field
```

зазвичай означає старіший CMake або старий локальний `CMakeUserPresets.json` із `"version": 6`.

Після переходу на v0.2.10.5:

```bash
cmake --version
rm -f CMakeUserPresets.json
cp CMakeUserPresets.example.json CMakeUserPresets.json
```

## Windows

Офіційний toolchain:

```text
MSVC 2022 x64 + Ninja + Qt MSVC2022_64
```

Не змішувати MinGW/Strawberry GCC з MSVC Qt/vendor `.lib`.

Рекомендовано:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

Configure має показати `The CXX compiler identification is MSVC`.

QHY: Windows x64 SDK. OAL ізолює QHY include directory, щоб vendor `stdint.h` не затіняв MSVC CRT/STL.  
ZWO: Windows x64 ASI/EAF `.lib` + runtime DLL.  
Canon: без EDSDK використовуйте `my-windows-observatory`; з EDSDK — `my-windows-observatory-edsdk`.

Packaging: `scripts/package_windows.ps1`.

## Linux x86_64 / ARM64

Типові Debian/Ubuntu залежності:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  libopencv-dev libgphoto2-dev libjpeg-dev
```

Рекомендований vendor SDK layout:

```text
/opt/openastrolink-sdk/qhy/include/qhyccd.h
/opt/openastrolink-sdk/qhy/lib/libqhy.so
/opt/openastrolink-sdk/zwo/asi/include/ASICamera2.h
/opt/openastrolink-sdk/zwo/asi/lib/libASICamera2.so
/opt/openastrolink-sdk/zwo/eaf/include/EAF_focuser.h
/opt/openastrolink-sdk/zwo/eaf/lib/libEAFFocuser.so
```

Перевірити:

```bash
uname -m
file /opt/openastrolink-sdk/qhy/lib/libqhy.so
file /opt/openastrolink-sdk/zwo/asi/lib/libASICamera2.so
file /opt/openastrolink-sdk/zwo/eaf/lib/libEAFFocuser.so
```

Canon Linux за замовчуванням використовує libgphoto2, тому EDSDK path не потрібен.

Збірка:

```bash
./scripts/check_build_environment.sh
./scripts/build_linux.sh my-linux-observatory
```

Install:

```bash
sudo cmake --install build/linux-observatory
sudo ldconfig
```

## WSL

`/mnt/c/...` — лише WSL path. Windows `.lib/.dll` не підходять Linux build. Linux `.so` має відповідати архітектурі WSL. Для реального hardware direct USB/serial passthrough у WSL треба налаштовувати окремо; для observatory host рекомендується native Linux.

## INDI

`OAS_ENABLE_INDI=ON` вмикає compatibility client. Native OAL drivers не залежать від INDI. `indiserver` потрібен лише для фактичних INDI-only devices.
