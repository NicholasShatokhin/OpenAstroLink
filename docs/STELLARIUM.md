# Stellarium integration

Version: 0.2.10.35

OpenAstroLink includes a direct TCP bridge compatible with Stellarium's external Telescope Control protocol. It is deliberately a mount bridge, not a replacement for the OAL observatory API.

## What works

- OAL reports the current mount RA/Dec to connected Stellarium clients.
- A Stellarium GOTO is decoded and forwarded to the active OAL mount.
- The bridge can run on the Raspberry Pi node while the GUI is local or remote.
- It can be configured through the GUI or OAL REST API and persisted in node settings.

Default port: `10000`.

```bash
openastrolink-node --stellarium-port 10000
```

REST:

```text
GET  /api/v1/integrations/stellarium
POST /api/v1/integrations/stellarium
```

Example request:

```json
{"enabled":true,"port":10000}
```

## Scope boundary

Stellarium's standard telescope-control connection transports telescope position and GOTO commands. It does not provide the complete OpenAstroLink model for cameras, focusers, autofocus, polar alignment, safety, operations or sessions. Those remain available through OAL GUI/REST/WebSocket. A future dedicated Stellarium OAL plug-in could expose a richer observatory panel inside Stellarium.

## Safety

The bridge uses the same active mount and therefore remains subject to OAL resource locks and the mount driver's own limits/interlocks. Do not expose the TCP port directly to the public Internet; use a trusted LAN/VPN and the OAL security plan for remote operation.

## Live position

The bridge publishes the active mount position to Stellarium every 500 ms and sends an immediate position update when a Stellarium client connects. The packet is normalized to J2000. Raw-axis EQDrive needs one OAL Sync before a valid encoder-to-sky position exists.
