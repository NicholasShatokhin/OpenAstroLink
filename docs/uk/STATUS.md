# Матриця зрілості модулів — v0.2.10

Канонічна англійська версія: `../STATUS.md`.

| Модуль | Статус | Коментар |
|---|---|---|
| Core/GUI separation | Реалізовано | `openastrolink-node` володіє hardware/workflows; GUI локальний або віддалений |
| Native OAL ABI v2 | Foundation реалізовано | Manifest, lifecycle, discovery, capabilities, health, events, frame publication |
| Native driver registry | Реалізовано | Manifest validation, search paths, device enumeration, generic adapters |
| Native reference simulator | Реалізовано | ABI-v2 camera/mount/focuser |
| Native QHY `oal.qhy` | Реалізовано, HIL pending | Direct QHYCCD SDK; exact hardware ID, capture, ROI/bin/gain/offset, abort, frame publication |
| Native Canon EOS `oal.canon` | Реалізовано, HIL pending | Native ABI-v2; Canon EDSDK на Windows, libgphoto2/PTP на Linux; original-file spool + preview |
| Native ZWO ASI `oal.zwo.asi` | Реалізовано, HIL pending | Direct ASI SDK; multi-camera enumeration, exposure, ROI/binning, gain/offset, cancel, native frame publication |
| Native ZWO EAF `oal.zwo.eaf` | Реалізовано, HIL pending | Direct EAF SDK; absolute/relative move, halt, status, temperature, limits, backlash/reverse |
| Native Gemini `oal.gemini` | Protocol path реалізовано, HIL pending | Direct persistent MyFocuserPro2-compatible serial |
| Gemini HALT | Не заявляється | Stop command для target firmware ще не HIL-verified |
| Native Sky-Watcher `oal.skywatcher` | SynScan v3.3 path реалізовано, HIL pending | J2000 status/GOTO/abort/sync/tracking/alignment/pier-side/pulse guide |
| Sky-Watcher park | Не підтримується цим profile | У SynScan serial v3.3 немає normative park command |
| Sky-Watcher direct motor-controller | Experimental codec only | Потрібна calibrated OAL alignment/model math |
| Persistent native serial transport | Реалізовано | Dedicated I/O thread для `QSerialPort` |
| INDI | Optional compatibility backend | Доступний в observatory builds; `rpi4-native-release` доводить no-INDI path |
| Easy INDI enablement | Реалізовано | `enable_indi_compat.sh`, `rpi4-observatory-release`, `OAS_ENABLE_INDI=ON` |
| ASCOM Alpaca | Compatibility backend | Для interoperability/migration |
| LX200 | Compatibility backend | Generic mount fallback |
| Per-device disconnect | Реалізовано | Main camera, guide camera, mount і focuser незалежні |
| Main + guide camera roles | Реалізовано | Окремі bindings та locks `camera` / `camera.guide` |
| Optical-train profiles | Реалізовано | Main/guide aperture, effective focal length, sensor/sampling, derived f-ratio/image scale |
| Stellarium bridge | Реалізовано, HIL pending | External Telescope Control TCP для mount position/GOTO, default port 10000 |
| Operation manager | Vertical slice | Queue/state/progress/cancel/resource locks |
| Async autofocus | Реалізовано | Locks `camera + focuser` |
| Async mount slew | Реалізовано | Lock `mount`; OAL polls status після GOTO acceptance |
| Async main exposure | Реалізовано | `camera.exposure` |
| Async guide exposure | Реалізовано | `camera.guide.exposure` |
| ASTAP adapter | Реалізовано, real-sky validation pending | CLI discovery/hints/result parsing; solve endpoint поки synchronous |
| Hardware probe | Оновлено | Native telescope/observatory gates + optional `--require-zwo` |
| RPi deployment | Структурно реалізовано | Native і native+INDI presets; real Pi/HIL pending |
| Native protocol smoke test | PASS локально | Pure C++ wire-format tests із warnings-as-errors |
| ZWO driver API-shape compile | PASS локально | Обидва ZWO drivers компілюються проти SDK-compatible headers із `-Werror`; real SDK/HIL pending |
| QHY planetary live/SER | Pending | Наступний data-plane increment |
| Polar alignment math | Реалізовано | Automatic capture/rotate/solve/live-adjust wizard pending |
| Guiding | Basic | Full calibration/dither/recovery pending |
| Scheduler | State model only | Durable execution/checkpoints/weather recovery pending |
| Out-of-process driver host | P0/P1 pending | Bundled drivers поки trusted in-process |
| Idempotency | P0 pending | Retry-safe create semantics incomplete |
| RFC 9457 Problem Details | P0 pending | Existing error envelope remains |
| Sequenced/replayable WS | P0 pending | Snapshot є; sequence/replay pending |
| FITS/RAW/SER data plane | P0/P1 pending | Native frame boundary є; durable science storage pending |
| Security/TLS/auth/safety | P0 pending | Trusted LAN/VPN only |

Native OAL означає, що драйвер говорить ABI v2 безпосередньо з OAL host і може використовувати лише manufacturer SDK або documented low-level protocol нижче нього. Він не проходить через INDI, Alpaca чи LX200.

## Нове у v0.2.10

- Native ZWO ASI camera та native ZWO EAF focuser.
- Dual main/guide camera ownership з independent resource locks.
- Main і guide optical-train profiles у node-side `TelescopeProfile`.
- Stellarium Telescope Control TCP bridge для mount position/GOTO.
- Англійська документація канонічна; українські mirrors підтримуються паралельно.

## v0.2.10: cross-platform build/deployment

- Windows x64 core/native/observatory presets — реалізовано, hardware build/HIL ще треба прогнати на цільовій машині.
- Linux x86_64 native/observatory/headless presets — реалізовано.
- Canon EDSDK transport — source/API-shape compile PASS, реальний EOS HIL pending.
- Windows `windeployqt` і Linux install/tar packaging scripts — реалізовано, target packaging validation pending.
- Локальні SDK paths винесені в ignored `CMakeUserPresets.json`.
