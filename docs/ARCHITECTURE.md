# OpenAstroSuite / OpenAstroLink architecture — v0.2.6

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
             oal.simulated     oal.qhy      future native
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

A native driver may use a vendor SDK underneath when a documented physical protocol is unavailable. For example, `oal.qhy` uses QHYCCD SDK for hardware access, but all device semantics above that layer are native OAL.

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
