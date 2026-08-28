# Native EQDrive support

`oal.eqdrive` is the experimental native ABI-v2 driver for EQDrive controllers.
It remains separate from the existing `oal.skywatcher` / EqMount path.

## Canonical serial protocol: ASTEP

The native serial driver uses the public **EQDrive ASTEP (astronomical equipment
protocol)** specification published by EQDrive. The mount section explicitly
defines read-only `St`, `Pos`, `Cg`, voltage telemetry and the motion commands
`Speed`, `Slew`, `Goto`, and `Drv`. Axis positions/displacements are degrees and
speeds are degrees/hour.

Canonical reference: <https://www.eqdrive.com.ua/en/support/eqdrive-protocol>.

Discovery is deliberately movement-free. It probes the mount-specific commands
in this order:

- `St\r` — state, both axis positions/speeds and GOTO/driver bits;
- `Pos\r` — both axis positions;
- `Cg\r` — motor configuration, including Direction/Reversed;
- `FWx/FWs/FW` only as supplemental identity diagnostics.

A device is exposed as a mount only after `St` or `Pos` validates as an ASTEP
mount response. This avoids treating a focuser/configuration-only personality as
a telescope mount.

## Discovery

CP210x/SILABS ports are preferred automatically. A port can also be pinned:

```text
openastrolink-node.exe --eqdrive-port COM5
```

Focused discovery tries the configured baud list and all four DTR/RTS line-state
combinations. If Windows reports `Access is denied`, probing stops immediately:
EQMOD/Classic ASCOM, EQDrive Config and the native driver cannot own the same
COM port simultaneously. A previously verified descriptor remains visible while
the physical port exists, even if another program temporarily owns it.

## Coordinate and motion safety

ASTEP exposes mechanical axis degrees, not celestial RA/DEC. OpenAstroLink uses
`sync-anchor-v2` and does not invent a home/pier model:

1. connect the native EQDrive backend;
2. point at a known sky coordinate;
3. issue Sync once;
4. RA/DEC status and GOTO are derived relative to that axis/sky anchor.

`coordinateValid=false` until Sync. The first HIL release limits each native GOTO
to `maxNativeGotoDeg` (15 deg by default). It converts the requested sky delta to
an **absolute ASTEP `Goto <axis1> <axis2>`** target. EQDrive firmware already owns
its `Direction/Reversed` motor configuration; OpenAstroLink reports those signs
as telemetry and does not blindly apply them a second time.

Park/meridian-flip remain disabled until the real pier model is qualified.
Classic ASCOM/EQMOD remains the qualified full-slew fallback.

## Direct Wi-Fi is a different transport

`synscan-wifi` does **not** use ASTEP. It emulates the path used by SynScan Pro
when talking directly to a SynScan-compatible Wi-Fi adapter: the official
Sky-Watcher Motor Controller protocol over UDP 11880. This is intentionally
separate from `oal.eqdrive` serial ASTEP and from `synscan-app` (SynScan Pro API).
See [SYNSCAN_NETWORK.md](SYNSCAN_NETWORK.md).
