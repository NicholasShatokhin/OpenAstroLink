## v0.2.10.51 Raspberry Pi 4/5 ARM64 status

OpenAstroLink розглядає 64-bit Raspberry Pi як generic Linux `aarch64` target. Тому Pi 4 і Pi 5 мають спільний OAL ABI та ARM64 vendor SDK matrix. Історичні назви `rpi4-*` preset/sysroot лишаються для backward compatibility і не означають Cortex-A72-only binary. Поточний WSL/Linux→ARM64 build доходить до 100% для `openastrolink-node`, `oal-hardware-probe` та native QHY/Canon/ZWO/Gemini/Sky-Watcher/EQDrive drivers. Physical Pi 5 runtime/HIL і ARM64 `OpenAstroSuite` GUI runtime ще pending.

# Raspberry Pi 4/5 observatory node — v0.2.10.51

Канонічний документ: `../RPI_NODE.md`.

`openastrolink-node` запускається headless через systemd, володіє пристроями та виконує autofocus, solve, polar alignment, guiding і sessions. GUI на самому RPi та GUI на іншому комп'ютері є клієнтами одного core.

Stellarium bridge можна запускати з `--stellarium-port 10000` або settings/API. INDI compatibility можна додати без зміни native OAL конфігурації.

## Build presets у v0.2.10.51

Native 64-bit Raspberry Pi 4/5 node:

```bash
cmake --preset rpi4-node-release
cmake --build --preset rpi4-node-release -j"$(nproc)"
```

Cross-build з Linux/WSL:

```bash
./scripts/build_rpi_cross.sh arm64 /path/to/rpi-arm64-sysroot
```

Cross-build з Windows через встановлений Arm GNU Toolchain:

```powershell
.\scripts\build_rpi_cross.ps1 -Arch arm64 -Sysroot C:\path\to\rpi-arm64-sysroot
```

Cross presets спочатку збирають headless node і навмисно тримають QHY/ZWO/Canon/INDI вимкненими, доки не задані бібліотеки саме цільової ARM-архітектури. x86_64 Linux SDK із WSL не можна використовувати для ARM target.


## Автоматичний ARM64 cross bootstrap — build-fix7
> build-fix7: helper завантаження Bookworm signing keys сумісний з `set -u`; помилка `name: unbound variable` усунена.


На Linux/WSL target sysroot можна підготувати явно:

```bash
./scripts/bootstrap_rpi_cross.sh arm64
```

Скрипт створює Bookworm ARM64 sysroot, перевіряє QEMU/binfmt, ставить target Qt/OpenCV/libusb/JPEG, готує matching host Qt і staging QHY ARM SDK. На Ubuntu 22.04 він не використовує `--no-check-gpg`: офіційні Debian 12 signing keys завантажуються через HTTPS, перевіряються за pinned fingerprints і передаються в `debootstrap --keyring`.


<!-- build-fix8-target-package-discovery -->
### build-fix8: target package discovery

Після bootstrap ARM sysroot CMake використовує target `Qt6Config.cmake` та `OpenCVConfig.cmake` з Debian multiarch directory, а `OAS_QT_HOST_PATH` — тільки для host `moc/rcc/uic`. Для ARM64 стандартні шляхи: `usr/lib/aarch64-linux-gnu/cmake/Qt6` і `usr/lib/aarch64-linux-gnu/cmake/opencv4` усередині sysroot.


### v0.2.10.49 build-fix9 — QHY stage входить у readiness

Для full ARM64 vendor preset QHY staging більше не best-effort. Bootstrap завершується помилкою при відсутньому/невалідному QHY stage і записує `OAS_QHY_SDK` лише після перевірки `qhyccd.h`, `libqhy.so` та target ELF architecture. Базову ARM-збірку без QHY можна явно робити через `--no-qhy`.

### v0.2.10.49 build-fix10 — QHY ARM64 проти ARMHF

Legacy `QHYCCD_Linux_New` `armv8` SDK фізично є 32-bit ARM/EABI build. Його слід використовувати лише з ARMHF node. Для 64-bit Raspberry Pi node потрібен справжній QHY `Arm_64`/`AARCH64` SDK; доки його не staged, ARM64 node треба збирати з QHY OFF (`my-rpi4-cross-arm64`).

## Поточний native-first hardware path

Для Raspberry Pi default runtime тепер native-first: QHY/Canon/ZWO ASI cameras, Gemini/ZWO EAF focusers та EQDrive/Sky-Watcher mount paths використовуються напряму через OAL. INDI/LX200 вмикаються лише явно для compatibility hardware. Planetary SER, persistent DSO/planetary/mosaic scheduler і guided Polar Alignment foundations уже реалізовані; найближча Beta зосереджена на HIL autofocus, auto-exposure, scheduler, mosaic і Polar Alignment.

