# OpenAstroLink HTTP API — v0.2.10.47 reference implementation


> **Current release:** v0.2.10.51. Native OAL drivers are default, INDI is opt-in, and the API version in `openapi.yaml` is 0.2.10.50.

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

Guiding and polar-alignment endpoints remain node-local. From v0.2.10.47, `POST /api/v1/sessions` accepts the primary `ObservationPlan`/`ObservationBlock` model and executes mixed DSO FITS/RAW and planetary SER blocks. Legacy `targets` payloads remain accepted and are converted into `dso-fits` blocks.

Implemented DSO block execution is asynchronous and uses the ordinary OAL operation/resource-lock model:

```text
mount.slew -> solver.adaptive -> optional Sync/recenter loop -> autofocus.run -> camera.exposure
```

Recenter/autofocus may be configured before the first science frame and every N completed frames. `GET /api/v1/sessions/current` exposes the execution cursor (`blockIndex`, `currentStep`, `currentOperationId`, per-block/global frame counters and failure reason), while `GET /api/v1/sessions/current/plan` returns the loaded plan. `planetary-ser` blocks execute GOTO, full-frame target acquisition, optional planet autofocus, hardware ROI and finite SER. ROI shifts are logged to `<basename>.roi.jsonl`; optional calibrated mount corrections are disabled by default pending HIL. Scheduler state is not durable across node restart yet.

## Pending P0 API work

- durable session checkpoints/restart resume;
- idempotency keys;
- durable operation persistence;
- RFC 9457 Problem Details;
- WebSocket sequence/replay/resume;
- production FITS/RAW/SER data plane;
- TLS/auth/scopes/audit/safety policy.

## v0.2.10 camera roles and Stellarium integration

`POST /api/v1/devices/connect` accepts optional `role: "main" | "guide"` for cameras. Omitting role preserves backward-compatible main-camera behavior. Guide exposures use `POST /api/v1/cameras/guide/capture` and resource `camera.guide`. `GET/POST /api/v1/integrations/stellarium` reads or configures the mount-only Stellarium TCP bridge.
