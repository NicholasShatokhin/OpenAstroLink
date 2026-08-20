# OpenAstroSuite / OpenAstroLink

**v0.2.6 — Native OAL Driver Foundation**

Цей реліз змінює hardware architecture: **native OpenAstroLink drivers є основним шляхом**, а INDI, ASCOM Alpaca, LX200 та інші старі екосистеми лишаються compatibility layer. Node і GUI не повинні знати vendor-specific API, якщо пристрій має native OAL driver.

```text
GUI (локальний на RPi або віддалений ПК)
                    │
             OAL HTTP / WebSocket
                    │
          openastrolink-node / oas_core
                    │
          Native OAL driver registry
           ABI v2 + capabilities/events
              │                 │
      oal_driver_qhy      oal_driver_simulated
              │
          QHYCCD SDK
              │
             QHY

Compatibility path (коли native driver ще немає):
INDI / Alpaca / LX200 / vendor compatibility adapter
```

## Головне у v0.2.6

- **Native driver ABI v2** у `include/oal/driver_api.h`:
  - manifest/identity;
  - typed capability document;
  - health;
  - cancellation hook;
  - push events;
  - deadlines/operation correlation fields;
  - окремий frame publish callback — RAW/science pixels не передаються Base64 через driver JSON.
- **Manifest-based registry/discovery**: `*.manifest.json` + shared library; `OAL_DRIVER_PATH` і стандартні install paths.
- **Native device adapters** `NativeOalCamera`, `NativeOalMount`, `NativeOalFocuser`: `ApplicationController` працює через `ICamera/IMount/IFocuser`, але hardware implementation живе в plug-in.
- **Reference native simulated driver**: camera + mount + focuser через ABI v2. Це conformance/reference path, а не спеціальний код у GUI.
- **Native QHY driver `oal.qhy`**: QHYCCD SDK винесений з `oas_core` у окремий OAL plug-in. Single-frame acquisition, ROI/binning/gain/offset, abort і host-frame transfer працюють без INDI.
- Старий in-core `QhyCamera` видалений; збережене старе `backend=qhy` binding автоматично мігрується на `oal.qhy`, якщо native driver і камера знайдені.
- API discovery:
  - `GET /api/v1/drivers`;
  - `GET /api/v1/drivers/devices`;
  - `GET /api/v1/drivers/{driverId}/devices/{deviceId}/capabilities`.
- GUI розділяє **Native OpenAstroLink** і **Compatibility / embedded** backends та має `Refresh native device discovery`.
- На RPi native drivers встановлюються в `/usr/local/lib/openastrolink/drivers`; `openastrolink-node` знаходить їх автоматично.

## Що входить у core

- `oas_core` — ядро без Qt Widgets.
- `openastrolink-node` — headless hardware owner для RPi/systemd.
- `OpenAstroSuite` — той самий GUI для localhost node, remote node або embedded developer mode.
- Async `OperationManager`: autofocus (`camera+focuser`), mount slew (`mount`), exposure (`camera`).
- ASTAP adapter, prototype catalog solver, autofocus, guiding baseline, polar-alignment RA-axis estimator, scheduler state model.
- Compatibility backends: INDI, ASCOM Alpaca, serial LX200, Gemini EAF via INDI/Alpaca, UVC/OpenCV, remote OAL client.

## Build

Потрібні CMake 3.24+, Qt 6.4+ і OpenCV 4.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

При default build native reference simulator з'явиться в:

```text
build/drivers/oal_driver_simulated[.dll|.so]
build/drivers/oal_driver_simulated.manifest.json
```

Node сканує `drivers/` поруч із executable, стандартні install paths і `OAL_DRIVER_PATH`.

### Native QHY

```bash
cmake -S . -B build \
  -DOAS_ENABLE_QHY=ON \
  -DQHYCCD_ROOT=/path/to/qhyccd-sdk
cmake --build build
```

`OAS_ENABLE_QHY` тепер **будує native `oal_driver_qhy`**, а не додає QHY implementation у core.

### Compatibility backends

```bash
cmake -S . -B build \
  -DOAS_ENABLE_INDI=ON \
  -DOAS_ENABLE_GPHOTO2=ON
```

INDI потрібен для mount/Gemini лише доки для конкретного hardware немає native OAL driver. Він не є обов'язковою частиною OAL architecture.

## Raspberry Pi 4

```bash
sudo ./scripts/bootstrap_rpi_observatory.sh
cmake --preset rpi4-observatory-release -DQHYCCD_ROOT=/opt/qhyccd
cmake --build --preset rpi4-observatory-release -j$(nproc)
./build/rpi4-observatory/oal-hardware-probe
sudo ./scripts/install_rpi_node.sh build/rpi4-observatory
```

Після install:

```text
/usr/local/bin/openastrolink-node
/usr/local/lib/openastrolink/drivers/oal_driver_simulated.so
/usr/local/lib/openastrolink/drivers/oal_driver_qhy.so       # якщо QHY enabled
/usr/local/lib/openastrolink/drivers/*.manifest.json
```

GUI на RPi підключається до `http://127.0.0.1:8080`; GUI на іншому ПК — до LAN/VPN адреси Pi. Закриття GUI не зупиняє node або hardware operations.

## Поточна межа

v0.2.6 робить native OAL driver path реальним, але ще не завершує всю нову driver platform:

- QHY planetary live/ring-buffer/SER — наступний data-plane increment;
- native Gemini driver потребує підтвердженого low-level protocol; до цього використовується INDI/Alpaca compatibility path;
- native mount driver треба реалізувати під конкретний фізичний протокол монтування;
- third-party `out-of-process` manifests у v0.2.6 **не запускаються in-process**; sandbox driver host ще треба реалізувати;
- permissions у manifest зараз описуються і перевіряються структурно, але OS sandbox enforcement ще попереду;
- TLS/auth/safety, idempotency, RFC 9457, reliable event replay і science FITS/RAW persistence залишаються P0.

Деталі:

- `docs/NATIVE_DRIVER_SDK.md`
- `docs/ARCHITECTURE.md`
- `docs/RPI_FIRST_HARDWARE.md`
- `docs/STATUS.md`
- `docs/ROADMAP_P0_P1_IMPLEMENTATION.md`
