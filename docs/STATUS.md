# Матриця зрілості — v0.2.7 Native Telescope Hardware Pack

| Модуль | Статус | Коментар |
|---|---|---|
| Core/GUI separation | Реалізовано | `openastrolink-node` owns hardware/workflows; GUI local or remote |
| Native OAL ABI v2 | Реалізовано foundation | Manifest, lifecycle, discovery, capabilities, health, events, frame publication |
| Native driver registry | Реалізовано | Manifest validation, default search paths, device enumeration, generic adapters |
| Native reference simulator | Реалізовано | ABI-v2 camera/mount/focuser |
| Native QHY `oal.qhy` | Реалізовано, HIL pending | Direct QHYCCD SDK; exact hardware ID, single capture, ROI/bin/gain/offset, abort, frame publication |
| Native Gemini `oal.gemini` | **Реалізовано protocol path, HIL pending** | Direct persistent 9600-baud MyFocuserPro2-compatible serial; position/move/temp/max-position |
| Gemini HALT | **Not advertised** | Exact target-firmware stop command not yet HIL-verified; capability is false rather than guessed |
| Native Sky-Watcher `oal.skywatcher` | **Реалізовано SynScan v3.3 path, HIL pending** | Precise J2000 status/GOTO/abort/sync/tracking/alignment/pier-side/pulse guide |
| Sky-Watcher park | Not supported by this profile | SynScan serial v3.3 has no normative park command |
| Sky-Watcher direct motor-controller | Experimental codec only | Published protocol foundation present; direct axis→RA/DEC requires calibrated OAL alignment/model math |
| Persistent native serial transport | Реалізовано | Dedicated serial I/O thread avoids cross-thread QSerialPort use and repeated DTR/open cycles |
| INDI | **Optional compatibility backend** | Default available in observatory builds; `rpi4-native-release` proves no-INDI path |
| Easy INDI enablement | Реалізовано | `--with-indi`, `enable_indi_compat.sh`, `rpi4-observatory-release` |
| ASCOM Alpaca | Compatibility backend | Maintained for interoperability/migration |
| LX200 | Compatibility backend | Minimal generic mount migration path |
| Per-device disconnect | Реалізовано | Camera/mount/focuser independent |
| Operation manager | Vertical slice | queue/state/progress/cancel/resource locks |
| Async autofocus | Реалізовано | Locks `camera+focuser` |
| Async mount slew | Реалізовано | Locks `mount`; native Sky driver returns after GOTO acceptance and OAL polls status |
| Async exposure | Реалізовано | `camera.exposure`; native QHY abort supported |
| ASTAP adapter | Реалізовано, real-sky validation pending | CLI discovery/hints/result parsing; solve operation still synchronous |
| Hardware probe | Updated | `--require-native-telescope` checks QHY + Gemini + Sky-Watcher |
| RPi deployment | Реалізовано structurally | Native and native+INDI presets; actual Pi/HIL pending |
| Native protocol smoke test | **PASS locally** | Pure C++ wire-format tests compile with `-Wall -Wextra -Werror` |
| QHY planetary live/SER | Pending | Next data-plane increment |
| Polar alignment math | Реалізовано | Automatic capture/rotate/solve/live-adjust wizard pending |
| Guiding | Basic | Full calibration/dither/recovery pending |
| Scheduler | State model only | Durable execution/checkpoints/weather recovery pending |
| Out-of-process driver host | P0/P1 pending | Bundled drivers are currently trusted in-process |
| Idempotency | P0 pending | Retry-safe create semantics incomplete |
| RFC 9457 Problem Details | P0 pending | Existing error envelope remains |
| Sequenced/replayable WS | P0 pending | Snapshot exists; sequence/replay pending |
| FITS/RAW/SER data plane | P0/P1 pending | Native frame boundary exists; durable science storage pending |
| Security/TLS/auth/safety | P0 pending | Trusted LAN/VPN only |

Native OAL means the driver speaks ABI v2 directly to the OAL host and may use only a manufacturer low-level SDK/protocol beneath it. It does not pass through INDI, Alpaca or LX200.
