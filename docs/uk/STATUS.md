# Стан OpenAstroSuite / OpenAstroLink — v0.2.10.5

Позначки: ✅ реалізовано; 🟡 реалізовано/частково реалізовано, але потрібен HIL або production qualification; 🧪 experimental; ⏳ ще не реалізовано.

## Core/control plane

| Область | Стан | Примітка |
|---|---|---|
| GUI / node separation | ✅ | Node володіє hardware/workflows; GUI локальний або remote |
| Node після закриття GUI | ✅ | Перевірено в simulator runtime |
| State snapshot/reconnect | ✅ | GUI відновлює authoritative state |
| Per-device disconnect | ✅ | Main/guide camera, mount, focuser незалежні |
| HTTP API | ✅ | `/api/v1` |
| WebSocket events | 🟡 | Є, але sequence/replay contract ще відсутній |
| Stellarium bridge | 🟡 | Position/GOTO є; HIL pending |

## Operations

| Область | Стан | Примітка |
|---|---|---|
| Operation state machine | ✅ | queued/running/succeeded/failed/cancelled |
| Progress/phase/result/problem | ✅ | Є в operation records |
| Cancellation | ✅ | Для operation-backed paths і hardware, що підтримує abort |
| Resource locks | ✅ | Main/guide camera, mount, focuser |
| Async slew | ✅ | operation-backed |
| Async exposure | ✅ | main + guide |
| Async autofocus | ✅ | camera + focuser |
| Idempotency | ⏳ | `Idempotency-Key` ще немає |
| Durable operations | ⏳ | Restart node ще не відновлює operations |

## Native drivers

| Driver | Стан | Примітка |
|---|---|---|
| QHY | 🟡 | Native QHYCCD SDK; MSVC header isolation; HIL pending |
| Canon EOS | 🟡 | EDSDK Windows / libgphoto2 Linux; HIL pending |
| ZWO ASI | 🟡 | Native ASI SDK, multi-camera; HIL pending |
| ZWO EAF | 🟡 | Native EAF SDK; HIL pending |
| Gemini EAF | 🟡 | Direct serial; HIL/firmware qualification pending |
| Sky-Watcher/SynScan | 🟡 | Direct SynScan; HIL pending |
| Sky-Watcher motor-controller | 🧪 | Лише protocol/codec foundation |
| INDI | ✅ | Optional compatibility client |
| Alpaca | ✅ | Compatibility |
| LX200 | ✅ | Minimal compatibility |

## Imaging/workflows

| Область | Стан |
|---|---|
| Main + guide camera roles | ✅ |
| Main + guide optical train | ✅ |
| Single-frame capture | ✅ |
| Preview | ✅ |
| Native frame publication | ✅ |
| Durable FITS/RAW store | ⏳ |
| Planetary SER pipeline | ⏳ |
| ASTAP | 🟡 adapter є, real-sky HIL pending |
| Closed-loop GOTO | 🟡 groundwork, HIL pending |
| Autofocus | 🟡 algorithm є, real optics tuning pending |
| Polar math | ✅ |
| Automatic polar wizard | 🟡 orchestration/HIL pending |
| Guiding | 🟡 basic API, production loop pending |
| Durable scheduler | 🟡 scaffolding, recovery/flip/dither/refocus pending |

## P0 hardening

Capabilities/discovery — 🟡; async operations — 🟡; resource locks — ✅; idempotency — ⏳; TLS/auth/scopes/audit — ⏳; safety/weather/roof/power — ⏳; durable science data plane — 🟡 foundation only; reliable replayable WebSocket — ⏳; RFC 9457 HTTP Problem Details — 🟡 incomplete; public conformance suite — 🟡 incomplete.

## Cross-platform build

| Target | Стан |
|---|---|
| Windows x64 MSVC + Ninja | 🟡 повний vendor SDK build ще кваліфікується |
| Linux x86_64 | 🟡 presets/docs готові, HIL pending |
| Linux ARM64/RPi | 🟡 HIL pending |
| WSL compile/test | 🟡 hardware лише через окремий passthrough |

### Нове у v0.2.10.5

- CMake preset schema v2 → CMake 3.20+.
- `cmake_minimum_required(VERSION 3.20)`.
- Windows official path: Ninja + MSVC.
- `build_windows.ps1` може сам завантажити `vcvars64.bat`.
- Збережена QHY/MSVC header isolation.
- Окремі Windows presets без Canon та з EDSDK.
- Оновлені Linux SDK staging/architecture instructions.

v0.2.10.5 — це build/HIL qualification release, а не unattended observatory release.
