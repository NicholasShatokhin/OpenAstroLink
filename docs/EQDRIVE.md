# Native EQDrive support — v0.2.10.50

`oal.eqdrive` is the native ABI-v2 driver for EQDrive controllers. It supports two low-level transports while keeping astronomical geometry in OAL Core:

- direct Sky-Watcher/EQMOD Motor Controller over serial;
- the public EQDrive ASTEP mount protocol as fallback/alternate transport.

The direct-MC coordinate model is **v9 and HIL-qualified** on the real mount. Its geometry, signs and Home/Park convention are frozen unless new HIL evidence contradicts them.

## Canonical serial protocol: ASTEP

The ASTEP path uses the public EQDrive astronomical-equipment protocol. The mount commands used by OAL include movement-free discovery/telemetry (`St`, `Pos`, `Cg`) and motion commands such as `Speed`, `Slew`, `Goto` and `Drv`. Axis positions/displacements are mechanical degrees and speed is degrees/hour.

Canonical reference: <https://www.eqdrive.com.ua/en/support/eqdrive-protocol>.

Discovery is deliberately movement-free. A device is exposed as a mount only after a mount-specific response validates. CP210x/SILABS ports are preferred automatically and a port can be pinned, for example:

```text
openastrolink-node.exe --eqdrive-port COM5
```

Focused discovery tries the configured baud list and line-state combinations. If the OS reports that the port is busy/access-denied, OAL does not fight EQMOD/another owner for it.

## Direct Motor Controller coordinate model v9

For the direct Motor Controller path, controller counts observed in the prepared physical polar Home pose become the session mechanical reference `Axis1=0°, Axis2=0°`. Raw controller counts remain diagnostics; OAL Core owns sky↔mechanical GEM geometry.

The HIL-qualified mapping is:

```text
Axis1Sign = +1
Axis2Sign = -1
coordinate model = eqmod-gem-ha-dec-v9
```

Native EQDrive serial and direct SynScan/EQDrive Wi-Fi use the same Core v9 geometry and shared Motor Controller GOTO planning. Transport I/O differs; astronomical kinematics do not.

See [MOUNT_GEOMETRY.md](MOUNT_GEOMETRY.md) for the frozen GEM equations and Home/Park convention.

## GOTO safety after HIL qualification

v0.2.10.50 removes the **temporary hidden driver-level `maxNativeGotoDeg` qualification envelope**. It was introduced only while axis/pier direction was being qualified and must not remain after v9 HIL success.

There are now two explicit safety layers instead of a hidden duplicate limit:

1. **Core/profile sky safety** — `maxGotoSkyDeltaDeg` (legacy alias `maxGotoAxisDeltaDeg`) is an operator-controlled true angular sky-separation policy. It can be kept conservative for supervised tests or raised to 180° when the operator wants normal full-range GOTO.
2. **Raw-axis request guard** — `mount.gotoAxes` keeps an explicit per-request `maxAxisDeltaDeg` mechanical ceiling, defaulting to the shortest-axis 180° envelope. This protects raw mechanical commands without constraining normal RA/DEC GOTO inside the transport driver.

No v9 geometry/sign/Home/Park logic changed when the temporary driver cap was removed.

## Park and meridian behavior

Mechanical Home/Park persistence is owned by OAL Core/profile rather than by an independent hidden EQDrive celestial model. Automatic meridian-flip orchestration remains outside the currently qualified unattended scope; supervised GOTO itself is no longer artificially limited by the driver qualification gate.

## Direct Wi-Fi is a transport peer, not ASTEP

`synscan-wifi` talks directly to a SynScan-compatible Wi-Fi/EQDrive Motor Controller over UDP 11880. It is not ASTEP and it is not the SynScan App/Pro API (`synscan-app`, UDP 11881). Direct serial and direct Wi-Fi nevertheless share coordinate model v9 and the same low-level GOTO-plan semantics.

See [SYNSCAN_NETWORK.md](SYNSCAN_NETWORK.md).
