# Матриця зрілості — v0.2.5

| Модуль | Статус | Коментар |
|---|---|---|
| `oas_core` separation | Реалізовано | Core/algorithms/backends/OAL не залежать від Qt Widgets |
| `openastrolink-node` | Реалізовано structurally | Headless node, HTTP/WS, auto-connect; RPi target compile/HIL ще має пройти на реальному Pi |
| GUI local/remote-node | Реалізовано | Один GUI працює через localhost або віддалений OAL node |
| Persistent device bindings | Реалізовано | Successful bindings відновлюються після restart node |
| Per-device disconnect | Реалізовано | Camera, mount і focuser від'єднуються незалежно |
| Operation manager | Vertical slice реалізовано | queue/state/progress/cancel/resource locks |
| Async autofocus | Реалізовано | Locks `camera+focuser` |
| Async mount slew | Реалізовано | Locks `mount`, має abort path |
| Async exposure | Реалізовано | `camera.exposure`, cancel/abort, preview fetch після завершення |
| ASTAP adapter | **Додано v0.2.5** | CLI autodetect/env override, hint/FOV arguments, INI result parsing; real-sky validation pending |
| Hardware probe | **Додано v0.2.5** | Probe ASTAP, QHY SDK IDs і INDI exact device names/properties |
| QHY SDK | **First hardware path v0.2.5** | Exact ID selection, process-lifetime SDK init, 16-bit where supported, ROI/bin/gain/offset, exposure abort; RPi HIL pending |
| QHY planetary live/SER | Pending | QHY live mode + SER/data plane next hardware increment |
| INDI mount | **First hardware path v0.2.5** | Standard coord/GOTO/sync/tracking/park/abort/pulse-guide mapping; exact real-driver HIL pending |
| INDI focuser | **First hardware path v0.2.5** | Requires absolute focuser for current AF path; absolute/relative/halt/temp mapping; Gemini HIL pending |
| INDI concurrency | **Hardened v0.2.5** | Per-call short-lived client sockets avoid cross-thread `QTcpSocket` use by operations |
| GeminiAstro EAF | Hardware compatibility path | `gemini-eaf` → INDI on RPi or Alpaca; exact Gemini INDI driver installation/HIL pending |
| RPi deployment helpers | Expanded | bootstrap, INDI systemd helper, node install, RPi CMake preset |
| Polar alignment math | Реалізовано | RA-axis estimator exists; complete automated capture/slew/solve wizard still pending |
| Guiding | Basic | Calibration/dither/recovery pending |
| Scheduler | State model only | Not durable execution engine |
| Solve operation resource | Pending | ASTAP is integrated, but current `/solve` call is still synchronous |
| Idempotency | P0 pending | `Idempotency-Key` not yet implemented |
| Durable operations | P0/P1 pending | Operations live in RAM only |
| RFC 9457 error model | P0 pending | Existing envelope remains |
| Security/TLS/auth | P0 pending | Do not expose node directly to public Internet |
| FITS/RAW data plane | P0 pending | Preview compatibility path remains; science persistence pending |
