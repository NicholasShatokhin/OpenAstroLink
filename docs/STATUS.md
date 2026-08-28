# OpenAstroSuite / OpenAstroLink status — v0.2.10.19

Status legend: ✅ implemented; 🟡 implemented/partially implemented but HIL or production qualification pending; 🧪 experimental; ⏳ not yet implemented.

## Core and control plane

| Area | Status | Notes |
|---|---|---|
| GUI / node separation | ✅ | Node owns hardware and workflows; GUI can be local or remote |
| Node survives GUI exit | ✅ | Confirmed in simulator runtime tests |
| Device reconnect/state snapshot | ✅ | GUI rehydrates aggregate state from node |
| Per-device disconnect | ✅ | Main camera, guide camera, mount, focuser independent |
| HTTP API | ✅ | `/api/v1` reference API present |
| WebSocket state/events | 🟡 | Channel exists; reliable sequence/replay contract still missing |
| Stellarium bridge | ✅ basic HIL | Stellarium GOTO confirmed through node and Classic ASCOM/EQMOD; native-mount parity still pending |

## Operations

| Area | Status | Notes |
|---|---|---|
| Operation resource/state machine | ✅ | queued/running/succeeded/failed/cancelled |
| Progress/phase/result/problem | ✅ | Stored in operation records |
| Cancellation | ✅ | Implemented for operation-backed paths where driver supports it |
| Resource locks | ✅ | Main camera, guide camera, mount, focuser locking |
| Async mount slew | ✅ | Operation-backed |
| Async main/guide exposure | ✅ | Operation-backed |
| Async autofocus | ✅ / HIL partial | Camera + focuser reservation; real Gemini motion during autofocus confirmed; optical convergence/repeatability still to qualify |
| Idempotency | ⏳ | HTTP `Idempotency-Key` not implemented |
| Durable operations | ⏳ | Operations are not yet restored after node restart |

## Native driver platform

| Area | Status | Notes |
|---|---|---|
| Native OAL ABI v2 | ✅ | Manifest/lifecycle/discovery/capabilities/events/cancellation/frame publication foundation |
| Driver registry | ✅ | Manifest/device enumeration and generic adapters |
| Reference simulator | ✅ | Camera/mount/focuser |
| Out-of-process sandbox | ⏳ | Manifest policy groundwork exists; driver host not production implemented |
| Third-party conformance suite | ⏳ | Project checks exist, public conformance suite does not |

## Hardware drivers

| Driver | Status | Notes |
|---|---|---|
| QHY | 🟡 HIL active | QHY5III462C discovery/connect/capture confirmed; post-frame readout termination, watchdog/control read-back and server-first live-catalogue propagation are under repeated-capture HIL qualification |
| Canon EOS | 🟡 | EDSDK Windows / libgphoto2 Linux; HIL pending |
| ZWO ASI | 🟡 | Native ASI SDK, multi-camera discovery; HIL pending |
| ZWO EAF | 🟡 | Native EAF SDK; HIL pending |
| Gemini EAF | ✅ basic HIL | Windows native discovery/connection, direct motion and autofocus-driven motion confirmed; long-run/reconnect/limits qualification remains |
| Sky-Watcher/SynScan | 🟡 | Serial SynScan path retained; `synscan-wifi` is direct Motor Controller UDP/11880 and `synscan-app` is SynScan Pro/App UDP/11881; direct Wi-Fi movement semantics corrected, HIL pending |
| Sky-Watcher direct motor-controller | 🧪 | Shared Motor Controller codec now follows EQMOD/INDI status/direction semantics and is used by direct Wi-Fi plus the EQDrive native fallback |
| EQDrive native | 🧪 | Separate `oal.eqdrive` dual-protocol driver: EQMOD-compatible Motor Controller first, ASTEP fallback; direct Standard5N HIL still pending while Classic ASCOM/EQMOD is confirmed |
| INDI compatibility | ✅ | Optional compatibility client; independent of native drivers |
| Classic ASCOM | ✅ basic HIL | Windows out-of-process COM bridge; EQMOD HEQ5/6 connect, park/unpark and Stellarium-driven GOTO confirmed |
| ASCOM Alpaca | ✅ | Compatibility backend |
| LX200 | ✅ | Minimal compatibility path |

## Imaging and optics

| Area | Status | Notes |
|---|---|---|
| Main + guide camera roles | ✅ | Independent roles and locks |
| Optical train profiles | ✅ | Main/guide aperture, focal length, pixel/sensor data, derived sampling |
| Single-frame capture | ✅ | Main/guide paths |
| In-memory preview | ✅ | PNG preview endpoint |
| Native frame publication | ✅ | Driver ABI supports non-JSON frame handoff |
| Canon original-file spool | ✅ | Driver path supports original file handling |
| Durable FITS/RAW store | ⏳ | Final science data plane not complete |
| Planetary SER pipeline | ⏳ | Production ring-buffer/SER/drop accounting not complete |

## Astronomy workflows

| Area | Status | Notes |
|---|---|---|
| ASTAP adapter | 🟡 | Adapter exists; real-sky qualification pending |
| Adaptive urban solve | 🟡 | Short-exposure quality gate + background removal + star registration/stack + mount hint + retry operation implemented; real city-sky HIL pending |
| Closed-loop GOTO/recenter | 🟡 | Groundwork exists; end-to-end HIL pending |
| Autofocus | 🟡 | Algorithm/operation exists; real optics/backlash tuning pending |
| Polar-axis math | ✅ | Sample/estimation API exists |
| Automatic polar wizard | 🟡 | Full live adjustment orchestration not production-qualified |
| Guiding | 🟡 | Basic state/API and pulse-guide primitives; production loop pending |
| Session/scheduler | 🟡 | Scaffolding exists; durable recovery/flip/dither/refocus pending |
| Target resolver/ephemerides | 🟡 | Not yet a complete normative production service |

## P0 protocol hardening

| Item | Status |
|---|---|
| Capabilities/identity/discovery | 🟡 foundation implemented; schemas still evolving |
| Async operations | 🟡 major paths implemented; not every long workflow converted |
| Resource locks | ✅ |
| Idempotency | ⏳ |
| TLS/auth/roles/scopes/audit | ⏳ |
| Safety interlocks/weather/roof/power | ⏳ |
| Separate durable science data plane | 🟡 ABI/preview foundation only |
| Reliable replayable WebSocket stream | ⏳ |
| RFC 9457 HTTP Problem Details | 🟡 operation problems exist; HTTP model incomplete |
| Conformance suite | 🟡 regression checks/simulator exist; public suite incomplete |

## Cross-platform build status

| Target | Status | Notes |
|---|---|---|
| Windows x64, MSVC + Ninja | 🟡 | Configure path qualified; full vendor SDK build still being iterated on physical host |
| Linux x86_64 | 🟡 | Presets and docs prepared; vendor SDK architecture/runtime must be qualified |
| Linux ARM64 / Raspberry Pi | 🟡 | Same Linux architecture; hardware HIL pending |
| WSL compile/test | 🟡 | Supported for build testing; direct hardware requires explicit passthrough |

### v0.2.10.5 build fixes

- CMake preset schema reduced to **v2**, compatible with CMake 3.20+.
- `cmake_minimum_required` reduced to 3.20 because the project does not require 3.24-only CMake features.
- Windows official preset path is now **Ninja + MSVC**, avoiding Visual Studio instance discovery and avoiding MinGW/MSVC ABI mixing.
- Windows build helper can load `vcvars64.bat` automatically.
- QHY Windows header isolation retained from v0.2.10.2.
- `CMakeUserPresets.example.json` now has a no-Canon Windows preset and a separate EDSDK-enabled preset.
- Linux SDK staging and architecture checks are documented.

## Release posture

v0.2.10.19 is a **supervised HIL qualification release**, not yet an unattended observatory release. Windows HIL has confirmed graceful shutdown, QHY connection/capture, Gemini motion/autofocus motion, Classic ASCOM through EQMOD, and Stellarium-driven GOTO. Current qualification focus is QHY repeated-capture stability/control read-back, mount GOTO direction/pier-side diagnostics, SynScan App network transport, and native EQDrive discovery.
