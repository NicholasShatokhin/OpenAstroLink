# Платформи збірки — v0.2.10.49 build-fix6

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

> **Linux baseline:** системний Qt повинен бути **6.4 або новішим**. Ubuntu 22.04/Jammy постачає Qt 6.2.4 і не має `qt6-httpserver-dev`, тому не є підтримуваним system-Qt build host. Використовуйте Ubuntu 24.04+ або Debian/Raspberry Pi OS 12 Bookworm+, або задайте власний Qt >= 6.4 через `CMAKE_PREFIX_PATH`/`Qt6_DIR`.

Типові Debian/Ubuntu залежності:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
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

Збірка (Linux presets тепер використовують `Unix Makefiles`, тому Ninja не є обов’язковим; Windows/MSVC presets і далі використовують Ninja):

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

## Raspberry Pi: native ARM та cross-compilation

### Native ARM64 на Raspberry Pi 4/5

На 64-бітному Raspberry Pi OS:

```bash
cmake --preset rpi4-node-release
cmake --build --preset rpi4-node-release -j"$(nproc)"
```

Для локального GUI використовуйте `rpi4-observatory-release`. Preset перевіряє, що target справді ARM64, і не дозволяє випадково зібрати x86_64 binary під назвою RPi. Для 32-бітного Raspberry Pi OS є `rpi-armhf-node-release`.

### Cross-build з Linux/WSL

У репозиторій додано:

```text
cmake/toolchains/linux-aarch64-gcc.cmake   -> aarch64-linux-gnu-g++
cmake/toolchains/linux-armhf-gcc.cmake     -> arm-linux-gnueabihf-g++
```

Самого cross compiler недостатньо: потрібні ARM-версії Qt6, OpenCV та всіх увімкнених SDK. Тому build отримує sysroot цільового Raspberry Pi:

Для Linux/WSL є автоматичний bootstrap:

```bash
./scripts/bootstrap_rpi_cross.sh arm64
```

За замовчуванням він створює Debian 12/Bookworm ARM sysroot, перевіряє QEMU/binfmt, ставить target Qt/OpenCV/libusb/JPEG, готує matching host Qt tools і staging QHY ARM SDK. На Ubuntu 22.04 distro-пакет `debian-archive-keyring` застарілий; build-fix6 не вимикає GPG verification, а завантажує офіційні Debian 12 archive/release/security keys через HTTPS, звіряє зафіксовані fingerprints і передає verified локальний keyring у `debootstrap --keyring`.

```bash
./scripts/build_rpi_cross.sh arm64 /opt/openastrolink-sysroots/rpi4-arm64
./scripts/build_rpi_cross.sh armhf /opt/openastrolink-sysroots/rpi-armhf
```

Базові cross presets навмисно вимикають QHY/ZWO/Canon/INDI, щоб не підхопити x86_64 `.so` з host. Vendor drivers треба вмикати у локальному preset лише після підстановки бібліотек правильної ARM-архітектури. Для Qt cross-build за потреби задайте `QT_HOST_PATH` до сумісних host-side `moc/uic/rcc`; target Qt6 при цьому має залишатися в sysroot.

### Cross-build з Windows

Підтримуються надані Arm GNU Toolchain layouts:

```text
cmake/toolchains/windows-to-linux-aarch64-gcc.cmake
cmake/toolchains/windows-to-linux-armhf-gcc.cmake
```

Приклад:

```powershell
.\scripts\build_rpi_cross.ps1 -Arch arm64 -Sysroot C:\toolchains\sysroots\rpi4-arm64
```

`win_mingw_*` не додаються як production Windows presets: наш Windows стек використовує MSVC ABI. `arm-none-eabi`/bare-metal toolchains також не підходять для OAL node/application, бо вони не мають Linux userspace, Qt та OpenCV.



## macOS (Apple Silicon та Intel)

Починаючи з build-fix3, macOS має окремі CMake presets для host-native збірки, Apple Silicon ARM64 та Intel x86_64. Використовуються Apple Clang, Qt >= 6.4, OpenCV, Canon EDSDK та macOS-бібліотеки ZWO. QHY на macOS лишається опційним, доки не вказано сумісну macOS SDK library. Classic ASCOM вимкнений, бо це Windows-only шлях.

```text
macos-core-release
macos-native-release
macos-observatory-release
macos-node-release
macos-arm64-observatory-release
macos-x86_64-observatory-release
```

Локальний приклад очікує sibling-layout `workspace/astro/{oal,zwo,edsdk}`. Для Apple Silicon ZWO використовує `mac_arm64`, для Intel — `mac`/`mac_x64`; CMake шукає всі ці варіанти. `OAS_CANON_TRANSPORT=AUTO` на macOS вибирає EDSDK.

```bash
brew install cmake qt opencv
./scripts/build_macos.sh my-macos-arm64-observatory
```

Підтримка macOS поки має статус source/configuration-qualified; реальна build/HIL-кваліфікація на Mac ще потрібна.


<!-- build-fix8-target-package-discovery -->
### build-fix8: target package discovery

Після bootstrap ARM sysroot CMake використовує target `Qt6Config.cmake` та `OpenCVConfig.cmake` з Debian multiarch directory, а `OAS_QT_HOST_PATH` — тільки для host `moc/rcc/uic`. Для ARM64 стандартні шляхи: `usr/lib/aarch64-linux-gnu/cmake/Qt6` і `usr/lib/aarch64-linux-gnu/cmake/opencv4` усередині sysroot.


### v0.2.10.49 build-fix9 — перевірка QHY staging

`bootstrap_rpi_cross.sh` більше не оголошує full vendor environment готовим після збою QHY staging. Якщо QHY увімкнений, staging має реально створити `include/qhyccd.h` і `lib/libqhy.so` з правильною target ELF-архітектурою. Для свідомо QHY-free environment використовується `--no-qhy`. `build_rpi_cross.sh` читає `OAS_QHY_SDK` з env record і явно передає include/library paths у CMake.

### v0.2.10.49 build-fix10 — назва QHY ARM archive не визначає ABI

Не можна визначати QHY ABI лише за `armv8` у назві archive. Legacy public `QHYCCD_Linux_New` v2.0.11 `armv8` package фізично містить 32-bit ARM/EABI shared library. Тому OAL перевіряє кожний QHY candidate через `file(1)` і використовує цей package лише для ARMHF. Для ARM64 потрібен справжній QHY `Arm_64`/`AARCH64` SDK archive.

Для поточної ARM64 збірки використовувати `my-rpi4-cross-arm64` (QHY OFF), доки такий SDK не staged. Після отримання:

```bash
./scripts/stage_qhy_cross_sdk.sh arm64 /path/to/current-qhy-arm64-sdk.tgz \
    "$HOME/.local/share/openastrolink/sdk/qhy-arm64"
./scripts/build_rpi_cross.sh arm64 --preset my-rpi4-cross-arm64-full
```

Для legacy cloned package валідним є ARMHF:

```bash
./scripts/stage_qhy_cross_sdk.sh armhf ../QHYCCD_Linux_New \
    "$HOME/.local/share/openastrolink/sdk/qhy-armhf"
```


### v0.2.10.49 build-fix12 — Canon Linux headers + сумісність Qt HttpServer 6.4

Перша реальна AArch64 компіляція дійшла до source layer і показала дві portability-деталі. Linux headers Canon EDSDK 13.20.x досі містять MSVC token `__int64` і використовують `WCHAR`; OAL тепер додає локальні Linux compatibility definitions перед `EDSDK.h`, не редагуючи vendor SDK. У Qt HttpServer `QAbstractHttpServer::bind(QTcpServer*)` повертав `void` у Qt 6.4-6.7 і `bool` у Qt 6.8+, тому node тепер компілюється з обома API shapes.


### v0.2.10.49 build-fix13 — транзитивне AArch64 linker closure у sysroot

Реальна ARM64 cross-build уже компілює Canon EDSDK, ZWO ASI/EAF, Gemini, SkyWatcher, EQDrive, `oas_core`, `openastrolink-node` та `oal-hardware-probe`; перший залишковий failure виник лише на фінальному executable link. GNU ld повідомив, що `liblapack.so.3` і `libblas.so.3`, потрібні target OpenCV, не знаходяться, після чого з'явилися сотні unresolved BLAS/LAPACK symbols та `GLIBC_2.36` mismatch із транзитивної target library. Cross link policy тепер передає target-sysroot multiarch/runtime directories через link-time-only `-rpath-link` і `-L`, тому `DT_NEEDED` closure має резолвитись із Bookworm sysroot, а не з Jammy host cross-runtime. Bootstrap також явно встановлює й перевіряє BLAS/LAPACK.

#### Debian BLAS/LAPACK alternatives у cross sysroot (build-fix15)

У Bookworm `libblas.so.3` і `liblapack.so.3` можуть експортуватися через Debian alternatives, тоді як concrete reference libraries лежать у `usr/lib/<multiarch>/blas` та `usr/lib/<multiarch>/lapack`. Foreign GNU linker не повинен покладатися на host-resolution таких absolute alternatives links. Тому OAL bootstrap нормалізує links у sysroot з root privileges, перевіряє concrete target libraries через `file`, а CMake cross-link policy передає обидва numerical subdirectories через link-time-only `-rpath-link`/`-L`. OAL також явно лінкує target LAPACK і BLAS runtime libraries після OpenCV, щоб детерміновано закрити numerical symbols OpenCV/Armadillo/ARPACK/SuperLU.
