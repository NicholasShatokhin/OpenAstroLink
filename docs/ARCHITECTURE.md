# Архітектура

## v0.2.2: ядро більше не прив'язане до GUI

```text
                   ┌───────────────────────────┐
                   │ OpenAstroSuite Qt GUI     │
                   │ presentation only         │
                   └─────────────┬─────────────┘
                                 │ ObservatoryController
                 ┌───────────────┴────────────────┐
                 │                                │
        local-node / remote-node             embedded/dev
                 │                                │
     RemoteObservatoryController          ApplicationController
          HTTP + WebSocket                       │
                 │                                │
                 ▼                                │
┌───────────────────────────────────────────┐     │
│ openastrolink-node                        │     │
│ ApplicationController                    │◄────┘ conceptual same core
│                                           │
│ ICamera / IMount / IFocuser               │
│ solve / autofocus / guide / polar/session │
│ OAL HTTP + WebSocket                      │
└──────┬─────────────┬──────────────┬────────┘
       │             │              │
      QHY          Gemini          Mount
                    EAF
```

### Production model on Raspberry Pi

`openastrolink-node` is the hardware owner and runs under `systemd`. A GUI attached to the Pi connects to `127.0.0.1`; a GUI on another computer connects to the Pi's LAN/VPN address. Both clients control the same core and therefore see the same active devices and state.

Closing either GUI must not stop the node. High-level astronomy actions are sent to the node and execute there.

### Embedded mode

The old in-process topology is retained as **Embedded core (developer mode)**. It is useful for simulation and development, but it is not the recommended RPi observatory topology because a GUI process should not own production hardware.

## Code layers

### `oas_core`

Static library containing:

- `ApplicationController`;
- device interfaces and active backends;
- autofocus/guiding/polar/scheduler algorithms;
- OAL HTTP/WebSocket server;
- settings and device binding persistence.

It contains no Widgets/UI code and is linked by `openastrolink-node`.

### `openastrolink-node`

`QCoreApplication` entry point. It restores persisted device bindings, starts HTTP/WebSocket, and remains alive independently of any GUI.

### `ObservatoryController`

GUI-facing abstract contract. `MainWindow` no longer depends directly on `ApplicationController`.

Implementations:

- `ApplicationController` — local embedded execution;
- `RemoteObservatoryController` — thin HTTP/WebSocket proxy to a node.

### `gui`

Presentation layer only in node mode. It may render preview frames and results, but it does not run autofocus, solve, polar alignment or hardware operations locally when connected to a node.

## Device ownership and persistence

Successful camera/mount/focuser connections are persisted by the node. On reboot the node can auto-connect the same backend/endpoints. This is required so an RPi can recover without a local desktop session.

## Полярне вирівнювання

The existing RA-axis mathematics remains in `ApplicationController`/`PolarAlignmentEstimator`. In remote mode these operations are invoked through OAL endpoints, so the samples and estimator state live on the Raspberry Pi, not in the remote GUI.

## Security boundary

v0.2.2 makes the process/network boundary explicit but does not yet implement the P0 TLS/auth/scope/safety policy. Until that increment, node ports are for a trusted LAN/VPN only.


## v0.2.3 operation-model update

The process boundary introduced in v0.2.2 is now used by a node-local `OperationManager`. Autofocus and mount slew can outlive the initiating HTTP request, publish progress over the event stream, and reserve their hardware resources. The GUI can be closed/reconnected without owning execution. Camera/mount/focuser also have independent disconnect commands. Exposure/solve/session migration, idempotency and durable resume remain pending.
