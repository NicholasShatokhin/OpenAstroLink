# Native EQDrive support

This document describes the experimental native OpenAstroLink driver for EQDrive controllers.

## Scope

`oal.eqdrive` is a new ABI-v2 mount driver and is separate from the existing `oal.skywatcher` / EqMount path. The existing Sky-Watcher driver is retained unchanged for SynScan-compatible hardware.

The driver talks directly to an EQDrive controller over a serial/VCP connection using the published ASTEP command set. It does **not** require EQMOD or ASCOM for the native path.

Reference: EQDrive public ASTEP protocol, <https://www.eqdrive.com.ua/en/support/eqdrive-protocol>.

## Discovery

Automatic discovery is intentionally conservative. It prefers CP210x/EQDrive-like serial devices. A port can be pinned explicitly:

```text
openastrolink-node.exe --eqdrive-port COM5
```

or selected in **Devices → Native serial discovery → EQDrive (ASTEP)**.

Focused discovery tests a bounded list of baud rates and modem-line combinations and only uses read-only ASTEP commands (`St`, firmware queries, `Cg`). Discovery never slews the mount.

The resulting backend has the form:

```text
native:oal.eqdrive/eqdrive:<stable-id>
```

where a USB serial number is preferred over a COM-port name when available.

## Implemented ASTEP operations

The driver uses the public mount section of ASTEP:

- `St` — axis state, positions and speeds;
- `Pos` — axis positions;
- `Slew` — relative axis motion;
- `Goto` — used for best-effort motion cancellation by retargeting the current position;
- `Speed` — tracking and temporary guide-rate offsets;
- `Vin`, `Vbs`, `Cg` — telemetry/configuration reads;
- `FW`, `FWs`, `FWx` — optional identity reads.

ASTEP axis positions/offsets are degrees and axis speeds are degrees/hour.

## Celestial coordinate model in v0.2.10.16

The native driver deliberately does not guess a mechanical home position, pier side, or meridian-flip convention for an unqualified installation. Instead it exposes a conservative **sync-anchor-v1** model.

1. Connect the mount.
2. Point the mount at a known sky position using a trusted method.
3. Issue one OpenAstroLink **Sync** with the known RA/Dec.
4. After that anchor exists, OAL maps ASTEP axis deltas to sky coordinates and accepts RA/Dec GOTO commands on the same pier-side branch.

Before the first Sync, `mount.status` returns `coordinateValid=false`. The GUI shows **Sky coordinates: not synced yet**, adaptive plate solving will not use the invalid mount position as a solve hint, and the Stellarium bridge will not publish a bogus `0,0` position.

### Current limitations

The following are intentionally not claimed as qualified in this release:

- automatic pier-side determination;
- automatic meridian flip;
- native park/unpark geometry;
- persistence of a celestial alignment model across arbitrary mechanical repositioning;
- verified sign/orientation parity on every EQDrive/mount installation.

Those features require real-mount HIL after basic serial control is confirmed.

## Telemetry

The native ABI additionally exposes `eqdrive.telemetry` with axis positions/speeds, driver state, detected firmware text, serial port/baud rate and best-effort `Vin`, `Vbs`, `Cg` reads.

## Relationship to Classic ASCOM

Classic ASCOM/EQMOD remains a first-class compatibility path on Windows. It is useful both for immediate observing and as a behavioral reference while native EQDrive is being qualified. See [ASCOM_CLASSIC.md](ASCOM_CLASSIC.md).
