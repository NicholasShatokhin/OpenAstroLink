# Module maturity matrix — v0.2.9

| Module | Status | Notes |
|---|---|---|
| Core/GUI separation | Implemented | `openastrolink-node` owns hardware/workflows; GUI can be local or remote |
| Native OAL ABI v2 | Foundation implemented | Manifest, lifecycle, discovery, capabilities, health, events, frame publication |
| Native driver registry | Implemented | Manifest validation, default search paths, device enumeration, generic adapters |
| Native reference simulator | Implemented | ABI-v2 camera/mount/focuser |
| Native QHY `oal.qhy` | Implemented, HIL pending | Direct QHYCCD SDK; exact hardware ID, single capture, ROI/bin/gain/offset, abort, frame publication |
| Native Canon EOS `oal.canon` | Implemented, HIL pending | Native ABI-v2 driver; USB/PTP via linked libgphoto2, shutter/ISO, Bulb, cooperative cancel, original-file spool, preview publication |
| Native ZWO ASI `oal.zwo.asi` | Implemented, HIL pending | Direct ASI SDK; multi-camera enumeration, exposure, ROI/binning, gain/offset, cancel, native frame publication |
| Native ZWO EAF `oal.zwo.eaf` | Implemented, HIL pending | Direct EAF SDK; absolute/relative move, halt, status, temperature, limits, backlash/reverse capability reporting |
| Native Gemini `oal.gemini` | Protocol path implemented, HIL pending | Direct persistent 9600-baud MyFocuserPro2-compatible serial; position/move/temp/max-position |
| Gemini HALT | Not advertised | Exact target-firmware stop command is not HIL-verified; capability remains false rather than guessed |
| Native Sky-Watcher `oal.skywatcher` | SynScan v3.3 path implemented, HIL pending | Precise J2000 status/GOTO/abort/sync/tracking/alignment/pier-side/pulse guide |
| Sky-Watcher park | Not supported by this profile | SynScan serial v3.3 has no normative park command |
| Sky-Watcher direct motor-controller | Experimental codec only | Published protocol foundation exists; direct axis→RA/Dec still needs calibrated OAL alignment/model math |
| Persistent native serial transport | Implemented | Dedicated serial I/O thread avoids cross-thread `QSerialPort` use and repeated DTR/open cycles |
| INDI | Optional compatibility backend | Available in observatory builds; `rpi4-native-release` proves the no-INDI path |
| Easy INDI enablement | Implemented | `enable_indi_compat.sh`, `rpi4-observatory-release`, `OAS_ENABLE_INDI=ON` |
| ASCOM Alpaca | Compatibility backend | Maintained for interoperability/migration |
| LX200 | Compatibility backend | Minimal generic mount migration path |
| Per-device disconnect | Implemented | Main camera, guide camera, mount, and focuser are independent |
| Main + guide camera roles | Implemented | Separate `main` / `guide` bindings and resource locks `camera` / `camera.guide` |
| Optical-train profiles | Implemented | Main and guide aperture, effective focal length, sampling/sensor data, derived f-ratio and image scale |
| Stellarium bridge | Implemented, HIL pending | External Telescope Control TCP bridge for mount position and GOTO; default port 10000 |
| Operation manager | Vertical slice | Queue/state/progress/cancel/resource locks |
| Async autofocus | Implemented | Locks `camera + focuser` |
| Async mount slew | Implemented | Locks `mount`; OAL polls status after GOTO acceptance |
| Async main exposure | Implemented | `camera.exposure`; hardware abort where supported |
| Async guide exposure | Implemented | `camera.guide.exposure`; independent guide-camera lock |
| ASTAP adapter | Implemented, real-sky validation pending | CLI discovery/hints/result parsing; solve endpoint is still synchronous |
| Hardware probe | Updated | Existing native telescope/observatory gates plus optional `--require-zwo` |
| RPi deployment | Structurally implemented | Native and native+INDI presets; actual Pi/HIL pending |
| Native protocol smoke test | PASS locally | Pure C++ wire-format tests compile with warnings treated as errors |
| ZWO driver API-shape compile | PASS locally | Both native ZWO sources compile against SDK-compatible headers with `-Werror`; real SDK/HIL pending |
| QHY planetary live/SER | Pending | Next data-plane increment |
| Polar alignment math | Implemented | Automatic capture/rotate/solve/live-adjust wizard pending |
| Guiding | Basic | Full calibration/dither/recovery pending |
| Scheduler | State model only | Durable execution/checkpoints/weather recovery pending |
| Out-of-process driver host | P0/P1 pending | Bundled drivers are currently trusted in-process |
| Idempotency | P0 pending | Retry-safe create semantics incomplete |
| RFC 9457 Problem Details | P0 pending | Existing error envelope remains |
| Sequenced/replayable WS | P0 pending | Snapshot exists; sequence/replay pending |
| FITS/RAW/SER data plane | P0/P1 pending | Native frame boundary exists; durable science storage pending |
| Security/TLS/auth/safety | P0 pending | Trusted LAN/VPN only |

Native OAL means that a driver speaks ABI v2 directly to the OAL host and may use a manufacturer SDK or documented low-level hardware protocol beneath it. It does not pass through INDI, Alpaca, or LX200.

## v0.2.9 additions

- Native ZWO ASI camera driver and native ZWO EAF focuser driver.
- Dual main/guide camera ownership with independent resource locks.
- Main and guide optical-train profiles stored in the node-side `TelescopeProfile`.
- Stellarium Telescope Control TCP bridge for mount position/GOTO.
- English-canonical documentation with Ukrainian mirrors.
