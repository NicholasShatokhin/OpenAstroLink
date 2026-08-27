# OpenAstroLink HTTP API — v0.2.10.16 reference implementation

The transitional envelope remains:

```json
{"ok":true,"error":null,"data":{}}
```

RFC 9457 conversion is still a P0 task.

## Native OAL driver registry

New native-first discovery endpoints:

- `GET /api/v1/drivers` — loaded native driver identities/manifests.
- `GET /api/v1/drivers/devices` — native devices exported by all loaded drivers.
- `GET /api/v1/drivers/{driverId}/devices/{deviceId}/capabilities` — device self-description.

Native device backend keys use:

```text
native:<driverId>/<encoded-deviceId>
```

These endpoints describe ABI-v2 native drivers. INDI/Alpaca/LX200 remain compatibility choices returned by the normal backend listing where built.

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

`/node/info` includes the number of loaded native drivers. `/devices` marks native devices with `nativeOal:true`.

## Operations

Current states:

```text
queued → running → succeeded | failed | cancelled
```

Endpoints:

- `GET /api/v1/operations`
- `GET /api/v1/operations/active`
- `GET /api/v1/operations/{id}`
- `POST /api/v1/operations/{id}/cancel`

Active operations own resource locks. Current async vertical slices:

- `autofocus.run` → `camera + focuser`;
- `mount.slew` → `mount`;
- `camera.exposure` → `camera`;
- `solver.adaptive` → `camera + solver`.

## Mount

- `GET /api/v1/mounts/{id}/status`
- `POST /api/v1/mounts/{id}/slew` — async 202 operation
- `POST /api/v1/mounts/{id}/abort`
- `POST /api/v1/mounts/{id}/sync`
- `POST /api/v1/mounts/{id}/tracking`
- `POST /api/v1/mounts/{id}/park`
- `POST /api/v1/mounts/{id}/pulse-guide`

Native mount drivers receive canonical OAL method calls; compatibility adapters translate these into INDI/Alpaca/LX200 semantics where available.

## Focuser

- `GET /api/v1/focusers/{id}/status`
- `POST /api/v1/focusers/{id}/move`
- `POST /api/v1/focusers/{id}/move-relative`
- `POST /api/v1/focusers/{id}/halt`

## Camera / frames / analysis

- `GET /api/v1/cameras/{id}/status`
- `POST /api/v1/cameras/{id}/capture` — async 202 `camera.exposure`
- `GET /api/v1/frames/{id}/preview`
- `POST /api/v1/solve` — synchronous solve of the last frame
- `POST /api/v1/solve/adaptive` — async urban-resilient short-exposure capture/register/stack/solve operation
- `POST /api/v1/autofocus/{id}/run`
- `POST /api/v1/motion/estimate`

For ABI-v2 native cameras, raw pixel bytes cross the driver boundary via `publishFrame(OalFrameDescriptorV2)` and a host frame token, not Base64 JSON. The HTTP PNG/Base64 preview remains only a compatibility/preview path; the durable FITS/RAW/SER data plane is still pending.

## Guiding / polar / sessions

Existing guiding, polar-alignment and session endpoints remain node-local. Polar math is already implemented; automated orchestration is still pending. Scheduler remains a non-durable state model.

## Pending P0 API work

- async park/session and closed-loop recenter orchestration;
- idempotency keys;
- durable operation persistence;
- RFC 9457 Problem Details;
- WebSocket sequence/replay/resume;
- production FITS/RAW/SER data plane;
- TLS/auth/scopes/audit/safety policy.

## v0.2.10 camera roles and Stellarium integration

`POST /api/v1/devices/connect` accepts optional `role: "main" | "guide"` for cameras. Omitting role preserves backward-compatible main-camera behavior. Guide exposures use `POST /api/v1/cameras/guide/capture` and resource `camera.guide`. `GET/POST /api/v1/integrations/stellarium` reads or configures the mount-only Stellarium TCP bridge.
