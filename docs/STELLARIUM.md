# Stellarium integration

> Current synchronized snapshot: **v0.2.10.58-buildfix9 (2026-09-09)**. See `STATUS.md`, `CURRENT_CHECKLIST.md` and `RELEASE_0.2.10.58.md` for current qualification boundaries.


> **Current development checkpoint:** v0.2.10.58-buildfix9. The standard Telescope Control bridge remains the mount-position/GOTO path; v0.2.10.53 additionally supports optional camera-footprint export through Stellarium Remote Control.

OpenAstroLink intentionally has two separate Stellarium integrations.

## 1. Telescope Control TCP bridge

This is the long-standing mount bridge. It:

- reports active OAL mount RA/Dec to Stellarium;
- decodes Stellarium GOTO and forwards it unchanged through the active OAL mount/controller path;
- can run on the Raspberry Pi node while the GUI is remote;
- defaults to TCP port `10000`.

```bash
openastrolink-node --stellarium-port 10000
```

REST:

```text
GET  /api/v1/integrations/stellarium
POST /api/v1/integrations/stellarium
```

The standard telescope protocol does **not** transport camera FOV rectangles, focusers, autofocus, polar alignment, sessions or other OAL observatory state.

## 2. Remote Control footprint export — v0.2.10.53

The OpenAstroSuite Sky Map can also talk directly to Stellarium's optional **Remote Control** HTTP plugin (default `http://127.0.0.1:8090`). This path is GUI-local and therefore also works when OpenAstroSuite is a thin client connected to a remote OAL node while Stellarium runs on the operator PC.

The export can send one of:

- last solved main-camera frame;
- predicted main-camera frame;
- predicted guide-camera frame;
- mosaic envelope.

Implementation contract:

1. discover the registered `SpecialMarkersMgr` rectangular-FOV property IDs through `GET /api/stelproperty/list`;
2. set width, height, rotation and visibility through `POST /api/stelproperty/set`;
3. center the Stellarium view on the OAL J2000 frame center through `POST /api/main/view`;
4. adjust viewport FOV through `POST /api/main/fov`.

The stock `SpecialMarkersMgr` exposes one rectangular marker. Consequently, **Send frame** replaces the previously exported OAL footprint. Multiple simultaneous independent OAL rectangles would require a dedicated Stellarium plugin and remain later scope.

The Remote Control plugin must be enabled in Stellarium. If it is disabled/unreachable, frame export fails cleanly and does not affect the TCP telescope bridge.

## Safety and coordinate policy

Neither integration introduces mount geometry. GOTO from Telescope Control still passes through the same active OAL mount and v9 Core geometry/safety policy. Footprint export is display-only and never commands mount motion.

The TCP position packet is normalized to J2000. Sky-frame export also uses the OAL frame's J2000 center. Raw-axis EQDrive still needs a valid OAL sky model (Home restoration or Sync) before meaningful telescope coordinates exist.

Do not expose either TCP/HTTP integration directly to the public Internet; use a trusted LAN/VPN and the OAL security plan for remote operation.
