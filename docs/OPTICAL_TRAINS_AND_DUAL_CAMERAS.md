# Optical trains and dual-camera operation


> **Current release:** v0.2.10.51. Optical-train and dual-camera semantics remain part of the current supervised Beta foundation.

Version: 0.2.10

## Why telescope parameters belong in the core

The optical train is not just GUI metadata. Focal length, aperture and camera sampling influence solver field-of-view hints, plate-scale validation, guiding scale, autofocus diagnostics and future scheduler/safety checks. They therefore belong in the node-side `TelescopeProfile` and are shared by local and remote clients.

## Main optical train

The profile stores:

- optical design (`reflector`, `refractor`, `SCT`, etc. as descriptive text);
- clear aperture / primary mirror diameter in mm;
- optional central obstruction in mm;
- effective focal length in mm, after reducer/Barlow;
- main-camera pixel size and sensor dimensions.

Derived values:

```text
f-ratio = focalLengthMm / apertureMm
plate scale = 206.265 × pixelSizeUm / focalLengthMm  [arcsec/pixel]
```

## Guide optical train

A second independent train stores guide-scope name, aperture, focal length, guide-camera pixel size and sensor dimensions. It may represent a separate guide scope or an OAG; for an OAG the effective guide focal length should match the main optical path.

## Main + guide camera

Two cameras can be connected simultaneously. The device registry distinguishes:

```json
{"type":"camera","role":"main"}
{"type":"camera","role":"guide"}
```

The main camera uses resource `camera`; the guide camera uses `camera.guide`. Therefore a guide exposure and main-camera exposure are independent unless a higher-level workflow explicitly locks both.

REST connection example for a guide camera:

```json
POST /api/v1/devices/connect
{"type":"camera","role":"guide","backend":"native:oal.zwo.asi/...","endpoint":""}
```

Guide capture:

```text
POST /api/v1/cameras/guide/capture
```

## Current guiding maturity

Dual-camera ownership and capture are implemented. The current guiding engine is still a basic closed loop and does not yet constitute a production guide-camera pipeline with calibration, centroid/star selection, subframes, dither, backlash compensation and star-loss recovery. That is the next layer built on this dual-camera foundation.
