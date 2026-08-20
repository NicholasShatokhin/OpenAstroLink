# Матриця зрілості — v0.2.6 Native OAL Driver Foundation

| Модуль | Статус | Коментар |
|---|---|---|
| Core/GUI separation | Реалізовано | `openastrolink-node` owns hardware/workflows; GUI can be local or remote |
| Native OAL ABI v2 | **Реалізовано foundation** | Manifest, lifecycle, discovery, capabilities, health, invoke context, cancellation, push events, frame publication |
| Native driver registry | **Реалізовано** | Manifest validation, default search paths, device enumeration and generic adapters |
| Native driver concurrency | **Реалізовано baseline** | `serial`/`per-device-serial` enforced by host; abort/HALT/cancel bypass normal lock for interruption |
| Native reference simulator | **Реалізовано** | ABI-v2 camera/mount/focuser; native frame path and events |
| Native QHY driver `oal.qhy` | **Реалізовано, HIL pending** | Direct QHYCCD SDK hardware path, exact hardware ID, single capture, ROI/bin/gain/offset, abort, frame publication |
| QHY inside core | Removed | Vendor-specific QHY implementation no longer lives in `oas_core` |
| Native QHY planetary live/SER | Pending | Streaming capability is explicitly `supported:false` in v0.2.6 |
| Native Gemini driver | Pending protocol validation | Current Gemini path remains compatibility INDI/Alpaca until low-level protocol is documented/verified |
| Native mount driver | Pending hardware/protocol selection | INDI/LX200/Alpaca remain compatibility paths for now |
| Driver out-of-process host | High-priority pending | v0.2.6 refuses manifests requesting out-of-process isolation instead of silently loading in-process |
| INDI | Compatibility backend | Maintained for existing hardware; standard mount/focuser properties supported |
| ASCOM Alpaca | Compatibility backend | Maintained for interoperability/migration |
| LX200 | Compatibility backend | Minimal generic mount migration path |
| Per-device disconnect | Реалізовано | Camera/mount/focuser independent |
| Operation manager | Vertical slice | queue/state/progress/cancel/resource locks |
| Async autofocus | Реалізовано | Locks `camera+focuser` |
| Async mount slew | Реалізовано | Locks `mount`, abort path |
| Async exposure | Реалізовано | `camera.exposure`; native QHY abort supported |
| ASTAP adapter | Реалізовано, real-sky validation pending | CLI discovery/hints/result parsing |
| Hardware probe | **Updated native-first** | Reports native OAL drivers/devices, QHY native availability, ASTAP and optional INDI compatibility devices |
| RPi deployment | Реалізовано structurally | Installs node and native drivers/manifests to `/usr/local/lib/openastrolink/drivers`; actual Pi build/HIL pending |
| Polar alignment math | Реалізовано | Automated capture/rotate/solve/live-adjust wizard pending |
| Guiding | Basic | Full calibration/dither/recovery pending |
| Scheduler | State model only | Durable execution/checkpoints/weather recovery pending |
| Solve operation resource | Pending | ASTAP call still synchronous |
| Idempotency | P0 pending | Retry-safe create semantics not complete |
| RFC 9457 Problem Details | P0 pending | Existing error envelope remains |
| Sequenced/replayable WS | P0 pending | Reconnect snapshot exists; sequence/replay protocol pending |
| FITS/RAW/SER data plane | P0/P1 pending | ABI-v2 frame publication removes plugin Base64; durable science data plane still pending |
| Security/TLS/auth/safety | P0 pending | Trusted LAN/VPN only for current remote use |

## Meaning of native vs compatibility

`Native OAL` means the driver speaks ABI-v2 directly to the OAL host and may use only the manufacturer's low-level SDK/protocol underneath. It does **not** pass through INDI, Alpaca or LX200.

Compatibility drivers remain valuable, but new OAL capabilities are designed natively first and only mapped down to legacy systems where possible.
