# Plate solving — adaptive urban workflow

This document describes the v0.2.10.16 node-local plate-solving path intended for light-polluted sites, small sensors and mounts where a single 10–15 s exposure produces elongated stars.

## Why the adaptive path exists

The legacy path is still available:

```text
capture one frame -> solve last frame
```

It is useful when a normal solver exposure already contains enough round stars. It is not the preferred first-light path for a difficult urban field.

The adaptive path is:

```text
short exposure
  -> quality assessment
  -> solve if viable
  -> otherwise capture several short exposures
  -> remove large-scale sky gradient
  -> register frames on stars
  -> stack only accepted registrations
  -> solve the synthetic solver frame
  -> expand hinted search radius and retry if required
```

The operation owns `camera + solver`, is cancellable, and runs in `openastrolink-node` even when the GUI is remote.

## Default policy

The GUI button **Adaptive urban capture + solve** uses the normal Capture exposure and gain as the base exposure, plus these defaults:

- solver binning: 2x2;
- attempts: 3;
- middle stack: 3 short frames;
- final stack: 5 short frames;
- maximum single exposure: 3 s;
- local quality target: 20 detected stars;
- background-gradient removal: enabled;
- inter-frame registration: enabled;
- mount RA/Dec hint: enabled when a mount is connected.

The first attempt uses one short frame. The next attempt builds a registered stack without lengthening an individual exposure. The final attempt may increase individual exposure modestly up to the configured maximum while also stacking more frames. The final attempt invokes the selected solver even when the local star-count gate is below target.

## Binning and FOV

`CameraFrame` now carries `binX/binY`. ASTAP FOV derivation multiplies the profile pixel scale by the actual returned binning. This avoids the old failure mode where a 2x2 solver frame was advertised with roughly half its true angular FOV.

## Mount hint

If the request does not provide explicit RA/Dec and `useMountHint=true`, the node snapshots the connected mount coordinates before starting the operation. Hinted search radii expand across retries (5°, 10°, then the configured maximum for the default three-attempt policy). Blind solving remains available through the legacy endpoint but is not the preferred urban strategy.

## Diagnostics

The operation result contains an `attempts` array. Each attempt records captured/registered frame counts, single and effective stacked exposure, detected-star count, background/noise statistics, search radius and solver message. `solverFrameId` identifies the exact preprocessed frame submitted to the external solver; fetch it with the normal frame-preview endpoint.

## REST

Start an adaptive solve:

```http
POST /api/v1/solve/adaptive
Content-Type: application/json
```

Example body:

```json
{
  "baseExposureSec": 1.5,
  "gain": 60,
  "binX": 2,
  "binY": 2,
  "maxAttempts": 3,
  "stackFrames": 3,
  "finalStackFrames": 5,
  "minStarsForSolve": 20,
  "maxSingleExposureSec": 3.0,
  "equalizeBackground": true,
  "registerFrames": true,
  "useMountHint": true,
  "searchRadiusDeg": 20
}
```

The response is a normal `202` operation resource. Poll `/api/v1/operations/{id}` or consume operation WebSocket events.

## First-light tuning

For QHY5III462C in strong city sky, start around 1–2 s, 2x2 binning and enough gain to reveal stars without saturating a large fraction of the image. Prefer raising gain and stacked-frame count before raising the single exposure beyond the point where tracking/polar error visibly elongates stars.

This adaptive path solves the image-acquisition side of the problem. It does not replace proper sidereal tracking, accurate focal-length/pixel-size profile data, a suitable ASTAP database, or real-sky HIL qualification.

### Node ASTAP configuration

The node can be configured without environment variables:

```text
openastrolink-node.exe --astap-executable "C:\\Program Files\\astap\\astap.exe" --astap-database "D:\\ASTAP\\D80" --astap-timeout-ms 60000
```

`--astap-database` should point at the installed ASTAP star database directory. The adaptive pipeline still works with environment-variable configuration; these switches make remote observatory deployments reproducible.

The GUI exposes a dedicated **Adaptive base exposure** (default 1.5 s), so a long normal Capture exposure cannot accidentally turn the adaptive solver back into a 10–15 s trailed-star exposure.
