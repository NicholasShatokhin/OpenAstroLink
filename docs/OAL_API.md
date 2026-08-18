# OpenAstroLink HTTP API — v0.2.2 reference implementation

Envelope remains:

```json
{"ok":true,"error":null,"data":{}}
```

## Node/client bootstrap added in 0.2.2

- `GET /api/v1/node/info` — node version, HTTP/WS ports, execution location.
- `GET /api/v1/node/backends` — camera/mount/focuser/solver backend lists available **on the node**.
- `GET /api/v1/profile`
- `POST /api/v1/profile`
- `POST /api/v1/devices/connect` — `{type, backend, endpoint}`; the device is opened by the node and the binding is persisted.
- `POST /api/v1/devices/disconnect-all`
- `POST /api/v1/solver/select`
- `POST /api/v1/solver/catalog`
- `POST /api/v1/solver/model`

These endpoints are what let the same Qt GUI control a local Raspberry Pi node or a remote node without owning hardware in the GUI process.

## Discovery/state

- `GET /api/v1/devices`
- `GET /api/v1/state`

## Mount

- `GET /api/v1/mounts/{id}/status`
- `POST /api/v1/mounts/{id}/slew`
- `POST /api/v1/mounts/{id}/sync`
- `POST /api/v1/mounts/{id}/tracking`
- `POST /api/v1/mounts/{id}/park`
- `POST /api/v1/mounts/{id}/pulse-guide`

## Focuser

- `GET /api/v1/focusers/{id}/status`
- `POST /api/v1/focusers/{id}/move`
- `POST /api/v1/focusers/{id}/move-relative`
- `POST /api/v1/focusers/{id}/halt`

## Camera / analysis

- `GET /api/v1/cameras/{id}/status`
- `POST /api/v1/cameras/{id}/capture`
- `POST /api/v1/solve`
- `POST /api/v1/autofocus/{id}/run`
- `POST /api/v1/motion/estimate`

The reference remote GUI still asks capture for a PNG preview in Base64. This is deliberately temporary until the P0 binary data plane.

## Guiding

- `POST /api/v1/guider/default/start`
- `POST /api/v1/guider/default/update`
- `POST /api/v1/guider/default/stop`
- `GET /api/v1/guider/default/status`

## Polar alignment

- `POST /api/v1/polar-align/clear`
- `POST /api/v1/polar-align/sample`
- `POST /api/v1/polar-align/ra-offset`
- `GET /api/v1/polar-align/estimate`

The estimator state is node-local; remote clients do not reproduce the polar math locally.

## Sessions

- `POST /api/v1/sessions`
- `GET /api/v1/sessions/current`
- `POST /api/v1/sessions/current/stop`

The current scheduler is still only a state model; durable execution is a later increment.

## WebSocket

The remote GUI subscribes to the node event port returned by `/api/v1/node/info` and handles `state`, `solveResult`, `autofocusProgress`, `autofocusResult`, `guidingUpdate`, `sessionUpdate`, and `motion`.

Sequence/replay/resume semantics remain a P0 task.
