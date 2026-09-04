# Stellarium integration


> **Current release:** v0.2.10.51. The Stellarium bridge remains supported and uses the active OAL mount path.

Version: 0.2.10.38

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

## Repeatable Home workflow (v0.2.10.38)

For a native raw-axis equatorial mount that is physically returned to the same Home pose before power-up (counterweight axis down, telescope/DEC axis toward the celestial pole), calibrate **Set current mechanical axes as Home** once and enable **Assume saved Home on connect**. On later connects OpenAstroLink compares the raw axes with the saved Home and, when within the configured tolerance, restores the encoder-to-sky model automatically. A plate-solve Sync can still refine pointing but is not required merely to make Stellarium GOTO operational.

Changing the sky-separation GOTO safety limit, Park coordinates, or Home preference no longer invalidates Sync. Changing an axis sign, geometry, pier branch, latitude, or longitude still invalidates the transform and may require either automatic Home restoration (when currently at Home) or a new Sync.

## v0.2.10.38: near-pole safety and ASCOM site

The native safety envelope now checks true angular separation on the celestial sphere, not raw RA-axis rotation, so a small move near the pole is not rejected solely because RA is singular there. The raw transport retains a separate 180° per-axis hard cap.

For Classic ASCOM, OAL verifies the driver's own site before GOTO. If EQMOD reports different latitude/longitude, the command is blocked until the site is corrected; EQMOD Setup may require **Allow Site Writes**. OAL Axis1/Axis2 inversion does not apply to ASCOM.
