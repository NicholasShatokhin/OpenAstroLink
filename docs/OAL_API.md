# OpenAstroLink HTTP API — v0.2.5 reference implementation

Envelope remains:

```json
{"ok":true,"error":null,"data":{}}
```

## Node / devices

- `GET /api/v1/node/info`
- `GET /api/v1/node/backends`
- `GET /api/v1/profile`
- `POST /api/v1/profile`
- `GET /api/v1/devices`
- `GET /api/v1/state`
- `POST /api/v1/devices/connect`
- `POST /api/v1/devices/{camera|mount|focuser}/disconnect`
- `POST /api/v1/devices/disconnect-all`

Each device can now be disconnected independently. Disconnect/reconnect is rejected with HTTP `409` while that device resource is locked by an active operation.

## P0 operation resources — v0.2.4 vertical slice

Long-running work starts returning an operation resource rather than keeping the initiating HTTP request open. Current operation states are:

```text
queued → running → succeeded | failed | cancelled
```

Operation endpoints:

- `GET /api/v1/operations`
- `GET /api/v1/operations/active`
- `GET /api/v1/operations/{id}`
- `POST /api/v1/operations/{id}/cancel`

Example operation:

```json
{
  "id":"op-...",
  "kind":"autofocus.run",
  "state":"running",
  "progress":0.42,
  "phase":"focus.scan",
  "cancelSupported":true,
  "cancelRequested":false,
  "resourceLocks":["camera","focuser"],
  "result":null,
  "problem":null
}
```

The node queues an operation when any requested resource is already locked. Operations with disjoint resource sets can run independently. `/api/v1/state` includes both active `operations` and current `resourceLocks`.

WebSocket event type `operation` carries operation snapshots/progress so reconnecting GUIs do not have to poll continuously. The full sequence/replay/resume protocol is still a later P0 increment.

### Autofocus

`POST /api/v1/autofocus/{id}/run` returns **202 Accepted** and locks:

```text
camera + focuser
```

The GUI remains responsive. `HALT focuser` or operation cancellation requests cancellation. While autofocus owns the camera, a new exposure operation remains queued until the `camera` lock becomes free. It can be cancelled while queued.

### Mount slew

`POST /api/v1/mounts/{id}/slew` returns **202 Accepted** and locks:

```text
mount
```

`POST /api/v1/mounts/{id}/abort` is a safety path: it requests cancellation of the owning mount operation and sends the backend-specific abort command (`:Q#` for LX200, Alpaca AbortSlew, INDI TELESCOPE_ABORT_MOTION, or the OAL abort endpoint).

## Mount

- `GET /api/v1/mounts/{id}/status`
- `POST /api/v1/mounts/{id}/slew` — async 202 operation
- `POST /api/v1/mounts/{id}/abort`
- `POST /api/v1/mounts/{id}/sync`
- `POST /api/v1/mounts/{id}/tracking`
- `POST /api/v1/mounts/{id}/park`
- `POST /api/v1/mounts/{id}/pulse-guide`

Direct mount commands are rejected while a mount operation owns the `mount` lock, except the explicit abort safety path.

## Focuser

- `GET /api/v1/focusers/{id}/status`
- `POST /api/v1/focusers/{id}/move`
- `POST /api/v1/focusers/{id}/move-relative`
- `POST /api/v1/focusers/{id}/halt`

Move is rejected while autofocus owns `focuser`; HALT remains available as a cancellation/safety command.

## Camera / analysis

- `GET /api/v1/cameras/{id}/status`
- `POST /api/v1/cameras/{id}/capture` — async 202 `camera.exposure` operation
- `GET /api/v1/frames/{id}/preview` — current PNG/Base64 preview compatibility resource
- `POST /api/v1/solve`
- `POST /api/v1/autofocus/{id}/run` — async 202 operation
- `POST /api/v1/motion/estimate`

`capture` now returns an operation immediately. The operation owns `camera`, records exposure metadata and returns a `frameId` on success. The node emits `frameReady`; remote GUIs fetch the preview separately. QHY and simulated cameras expose an abort path; non-abortable backends use best-effort cancellation and discard a frame if cancellation was requested before readout completed. The PNG/Base64 preview is transitional: FITS/RAW data plane remains P0 pending.

## Guiding / polar alignment / sessions

Existing guiding, polar-alignment and session endpoints remain. Polar math and state remain node-local. The scheduler is still a state model, not a durable operation workflow.

## Scope deliberately not claimed by v0.2.4

This increment does **not** yet finish the complete P0 operation specification. Still pending:

- async migration of solve, park and session execution;
- idempotency keys and retry-safe create semantics;
- durable operation persistence across node restart;
- RFC 9457 Problem Details conversion;
- sequenced/replayable WebSocket streams;
- FITS/RAW data plane and production security.


## v0.2.5 hardware note

The HTTP surface is intentionally unchanged for the first RPi hardware increment. The node backend registry can now expose `qhy`, `indi` and `astap` when those build/runtime dependencies are available. See `RPI_FIRST_HARDWARE.md`.
