## Current deployment policy — v0.2.10.51

Native OAL drivers are the primary execution path on Windows, Linux, Raspberry Pi and macOS. INDI is not part of the default runtime and is enabled only as a compatibility adapter when needed. Windows x64, Linux x86_64 and Raspberry Pi/Linux ARM64 node builds are now confirmed; macOS remains build-configured but physically unqualified.

# OpenAstroSuite / OpenAstroLink architecture — v0.2.10.5

## Native OAL is the reference hardware architecture

```text
                         OpenAstroSuite GUI / scripts
                           local or remote client
                                  │
                           HTTP + WebSocket
                                  │
                         openastrolink-node
                                  │
                 ApplicationController / AstroCore
                         operations + locks
                                  │
                  native device registry / ABI v2
                   ┌──────────────┼───────────────┐
                   │              │               │
             oal.simulated     oal.qhy      oal.canon      other native
                   │              │          Gemini / mount
                   │           QHY SDK             │
                   ▼              ▼                ▼
                virtual       QHY camera         hardware

        Compatibility / migration adapters (parallel path)
                   ┌──────────────┼───────────────┐
                   │              │               │
                  INDI        ASCOM Alpaca      LX200
                   │              │               │
                legacy / already-supported astronomy hardware
```

The architectural rule is: **INDI/ASCOM/LX200 are migration layers; native OAL is the reference architecture.** Compatibility systems do not define the limits of native OAL capabilities, operations, events or data transfer.

A native driver may use a vendor SDK underneath when a documented physical protocol is unavailable. For example, `oal.qhy` uses QHYCCD SDK for hardware access, while `oal.canon` uses Canon EDSDK on Windows or linked libgphoto2 as the USB/PTP transport library on Linux. Neither path passes through INDI/ASCOM; all device semantics above the hardware-access layer are native OAL.

## Process topology

The GUI remains optional. `openastrolink-node` owns hardware and astronomy workflows. A monitor/keyboard attached to the Raspberry Pi runs the same GUI against `127.0.0.1`; a remote computer connects to the Pi over LAN/VPN.

```text
                 Raspberry Pi 4
┌────────────────────────────────────────────────────────┐
│ systemd                                                │
│   openastrolink-node                                   │
│      │                                                 │
│      ├─ native OAL drivers                             │
│      ├─ compatibility adapters                         │
│      ├─ autofocus / solve / polar / guide / sessions   │
│      └─ HTTP :8080 / WebSocket :8090                   │
│                    ▲                      ▲             │
│                    │                      │             │
│             local GUI                 remote GUI        │
│             monitor+KB                  LAN/VPN         │
└────────────────────────────────────────────────────────┘
```

Closing either GUI does not stop the node or active operations.

## Code layers

### `oas_core`

Contains `ApplicationController`, device interfaces/adapters, astronomy algorithms, operation manager, OAL HTTP/WebSocket server and configuration. It does not contain Qt Widgets UI code.

Native vendor implementations no longer need to be compiled into `oas_core`. The old direct in-core QHY backend has been removed in v0.2.6 and replaced by the native `oal.qhy` plugin.

### Native driver registry

`OalDriverPluginLoader` scans manifest-based ABI-v2 plugins from configured driver directories, validates their manifests, loads them, enumerates devices and exposes identity/capabilities/health/invoke/cancel operations.

The controller sees native devices through generic `NativeOalCamera`, `NativeOalMount` and `NativeOalFocuser` adapters. It does not contain QHY-specific method calls.

### Native driver ABI v2

Defined in `include/oal/driver_api.h`. It adds:

- manifest + ABI negotiation;
- self-described devices/capabilities;
- operation/deadline context;
- cancellation;
- push events;
- health;
- host frame publication without Base64 pixels.

See `docs/NATIVE_DRIVER_SDK.md`.

### Compatibility adapters

INDI, ASCOM Alpaca and LX200 remain in the core compatibility layer. They are intentionally retained for equipment without a native OAL driver and for migration/interoperability testing.

## Native frame path

For native camera drivers, science pixel bytes no longer need to cross the plugin boundary as JSON/Base64:

```text
camera SDK / USB
      ↓
native OAL driver
      ↓ publishFrame(OalFrameDescriptorV2)
OAL host frame store
      ├─ capture result
      ├─ preview
      ├─ autofocus
      └─ later FITS/SER/data-plane writers
```

The current host makes a synchronous copy. Shared-memory/ring-buffer zero-copy transport is a future data-plane increment.

## Operations and ownership

The node-local `OperationManager` owns long-running operations and resource locks. Current async vertical slices include autofocus, mount slew and camera exposure. Native drivers are callable from operation worker threads and therefore must obey their declared concurrency model.

`camera + focuser` are reserved during autofocus; `mount` is reserved during slew. Safety cancellation/abort paths remain available.

## Driver isolation

ABI v2 manifests can declare an isolation policy. v0.2.6 supports trusted/reference in-process drivers. If a manifest asks for `out-of-process`, the loader refuses it rather than silently weakening isolation. A sandboxed `oal-driver-host` is the next driver-platform milestone and has been moved earlier in the roadmap.

## Polar alignment

The RA-axis estimator remains node-local. Native-vs-compatibility hardware is irrelevant to the mathematical layer: capture, solve and mount motion flow through `ICamera`/`IMount`, so the eventual automated polar-alignment wizard can operate over either path.

## Security boundary

TLS/auth/scopes/safety policy are not yet complete. Until that P0 work lands, expose node ports only on a trusted LAN or through a VPN; do not forward `8080/8090` directly to the public Internet.

## v0.2.10 additions

The device model now includes independent `main` and `guide` camera roles. Main and guide capture use independent resource locks (`camera` and `camera.guide`). The node also hosts a Stellarium Telescope Control TCP bridge for mount position/GOTO, while full observatory services remain OAL-native. ZWO ASI/EAF are first-class native ABI-v2 drivers.
