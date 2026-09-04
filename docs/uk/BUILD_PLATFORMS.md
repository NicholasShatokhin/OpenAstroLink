## Поточна build-кваліфікація — v0.2.10.50

| Target | Build status | Примітка |
|---|---|---|
| Windows x64 / MSVC 2022 + Ninja | ✅ confirmed | Повний observatory build успішний після deterministic `cl.exe` selection. |
| Linux x86_64 | ✅ confirmed | Native build успішний з per-user Qt bootstrap на Ubuntu 22.04/Jammy. |
| Linux ARM64 / Raspberry Pi 4 | ✅ confirmed | WSL/Linux cross-build доходить до 100% для node, probe та native vendor drivers, включно з QHY 26.06.04. |
| Linux ARM64 / Raspberry Pi 5 | 🟡 ABI-supported | Той самий generic aarch64 target; physical Pi 5 runtime/HIL ще pending. |
| macOS Apple Silicon / Intel | 🟡 configured | Presets/vendor bootstrap є; physical Mac build/sign/package qualification pending. |

Native drivers — default. INDI — тільки opt-in. Сирий `cmake --preset` лишається side-effect-free; platform wrappers можуть шукати/bootstrap redistributable dependencies. Canon EDSDK — тільки manual download/local discovery.

## Вибір Windows compiler — build-fix20

Нативні Windows presets використовують generator Ninja з явним `CMAKE_CXX_COMPILER=cl.exe`. Так зберігається перевірений MSVC/Ninja шлях, Strawberry/MinGW `c++.exe` не може бути обраний, а CMake не мусить знаходити зареєстрований Visual Studio instance. Сирий `cmake --preset my-windows-observatory` запускайте з x64 MSVC Developer Command Prompt; `scripts/build_windows.ps1` сам завантажує `vcvars64`. Нативні Windows build directories мають суфікс `*-msvc-ninja`, щоб не змішувати старий GNU-Ninja та невдалий VS-generator cache. Windows-hosted Raspberry Pi cross presets лишаються GNU/Ninja, бо вони навмисно збирають Linux ARM.

# Платформи збірки — v0.2.10.50

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

ARM64 cross-link closure тепер фізично підтверджений. Попередній final-link failure був спричинений Bookworm BLAS/LAPACK alternatives у `usr/lib/<multiarch>/blas` та `.../lapack` і відсутніми target-only transitive search paths. Виправлена policy додає target-sysroot `-rpath-link`/`-L`, встановлює/перевіряє BLAS/LAPACK, нормалізує sysroot symlinks з потрібними privileges і резолвить `DT_NEEDED` із Bookworm target, а не Jammy host. Після цього build доходить до 100% для `openastrolink-node` і `oal-hardware-probe` з Canon EDSDK, ZWO ASI/EAF, QHY, Gemini, SkyWatcher та EQDrive.

#### Debian BLAS/LAPACK alternatives у cross sysroot (build-fix15)

У Bookworm `libblas.so.3` і `liblapack.so.3` можуть експортуватися через Debian alternatives, тоді як concrete reference libraries лежать у `usr/lib/<multiarch>/blas` та `usr/lib/<multiarch>/lapack`. Foreign GNU linker не повинен покладатися на host-resolution таких absolute alternatives links. Тому OAL bootstrap нормалізує links у sysroot з root privileges, перевіряє concrete target libraries через `file`, а CMake cross-link policy передає обидва numerical subdirectories через link-time-only `-rpath-link`/`-L`. OAL також явно лінкує target LAPACK і BLAS runtime libraries після OpenCV, щоб детерміновано закрити numerical symbols OpenCV/Armadillo/ARPACK/SuperLU.


### build-fix17 — native OAL є default; INDI лише opt-in

`OAS_ENABLE_INDI` тепер має default `OFF` і на рівні CMake option, і в усіх звичайних observatory/node presets. Для INDI-only обладнання слід використовувати явні `*-indi-release` presets або `-DOAS_ENABLE_INDI=ON`. Native OAL drivers ніколи не маршрутизуються через INDI. Vendor bootstrap на Linux/macOS може використовувати pinned revision `indi-3rdparty` лише як відтворюваний mirror ZWO SDK blobs; це не встановлює, не вмикає і не лінкує INDI compatibility backend.

## Автоматичний bootstrap залежностей native-збірки (build-fix18)

Рекомендовані native build-wrapper-и тепер спочатку шукають наявні залежності й лише за відсутності встановлюють або завантажують ті компоненти, які можна відтворювано розповсюджувати. Сам `cmake --preset ...` лишається без прихованих мережевих/системних side effects.

Linux/WSL:

```bash
./scripts/build_linux.sh my-linux-observatory
```

Перший запуск може доставити відсутні пакети Debian/Ubuntu, встановити per-user Qt через `aqtinstall`, якщо системний Qt старіший за 6.4, знайти/встановити OpenCV і staged QHY/ZWO SDK. Результат записується в `.oal/native-deps-linux-<arch>.env`. `--no-auto-deps` вимикає автоматизацію.

macOS:

```bash
./scripts/build_macos.sh my-macos-arm64-observatory
```

За наявності Homebrew він використовується для build tools/OpenCV; Qt спочатку шукається, а за відсутності встановлюється в per-user cache OpenAstroLink. QHY/ZWO також staged у цей cache.

Windows:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

Wrapper шукає Qt/OpenCV/vendor SDK, може встановити Qt через `aqtinstall`, OpenCV через per-user vcpkg, завантажує QHY з офіційного QHY SDK repository та шукає локальні ZWO/Canon SDK. `-NoAutoDeps` повертає повністю ручний режим.

Canon EDSDK навмисно лише **автовиявляється**, але не завантажується/перерозповсюджується OpenAstroLink через окремі умови Canon SDK. Для ZWO Linux/macOS дозволено pinned SDK mirror; Windows ZWO спочатку автовиявляється локально, а за відсутності завантажується через офіційний ZWO developer product-download endpoint; цей endpoint стежить за поточним vendor release, а не за pinned-версією.

Після одноразового bootstrap CMake також шукає managed cache напряму, тому наступні ручні `cmake --preset ...` можуть використовувати вже staged залежності.

### Примітка щодо Ubuntu 22.04 / Jammy і Qt

Jammy надає лише Qt 6.2.4. Встановлення додаткових пакетів `qt6-*`/`libqt6*-dev` з Jammy не задовольняє вимогу OpenAstroLink Qt >= 6.4 + Qt HttpServer. Використовуйте:

```bash
./scripts/build_linux.sh my-linux-observatory --bootstrap-deps
```

Скрипт встановлює повний per-user Qt через `aqtinstall` у `~/.local/share/openastrolink/qt` і не потребує GUI-інсталятора Qt або Qt Account. Не запускайте GUI-інсталятор Qt через `sudo`. Linux wrapper також автоматично видаляє застарілий CMake cache, якщо checkout було перенесено між WSL (`/mnt/c/...`) і нативним Linux (`~/...`).
