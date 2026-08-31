# Mount geometry and mechanical coordinates

OpenAstroLink separates **sky coordinates** from **mechanical mount axes**. J2000 is the canonical equatorial interchange frame; a mount geometry model converts the requested sky position to the axes required by the selected mount type.

## Supported geometry profiles

The v0.2.10.25 foundation defines:

- German equatorial (GEM)
- fork equatorial
- alt-azimuth
- alt-azimuth with derotator
- equatorial platform
- custom two-axis

The geometry profile is observatory configuration, not a property hard-coded into EQDrive or another transport driver. Raw-axis native drivers expose axis position/motion; OAL Core owns astronomical geometry.

## German equatorial model

For generic/legacy equatorial backends OAL can still use the original direct hour-angle/declination mapping. Direct Sky-Watcher Motor Controller backends (`oal.eqdrive` serial and `synscan-wifi` UDP/11880) use **coordinate model v6**. OAL exposes one mechanical axis system only: the physical startup Home/Park pose is `Axis1=0°, Axis2=0°`. Low-level controller step counts are diagnostics, not a second angular coordinate system.

The v6 mapping follows the telescope-direction-vector method used by the mature INDI SkyWatcher implementation. OAL converts the JNow target to local horizontal coordinates, constructs the telescope direction vector, rotates that vector into the polar-aligned mount frame, and converts the result to mount spherical coordinates. Public `Axis1` is the mount-frame azimuth and public `Axis2` is `90° - mount-frame altitude` (signed on the flipped GEM branch). Home therefore remains exactly `0°,0°` without guessing an hour-angle phase.

A sky target has two mechanically equivalent GEM representations, `(A,+P)` and `(A+180°,-P)`. OAL compares both with the current controller position and selects the shortest valid physical route. Telemetry performs the exact inverse telescope-frame transform on every encoder sample so RA/DEC can be streamed continuously while slewing.

Sky-Watcher Motor Controller position packets are signed step-count offsets from the protocol reference. At direct-MC connect, OAL captures the current counts of both axes in the user-prepared polar Home pose. Those counts become the session Home/Park reference and map to `Axis1=0°, Axis2=0°`. No fixed quarter-turn or 90° DEC offset is assumed.

Profiles created before v0.2.10.42 migrate once for direct Motor Controller backends to model v6. Legacy custom Home/Park calibration is cleared because it may have been recorded under an incompatible older coordinate model; standard Home/Park becomes `0°,0°`, axis signs reset to `+1/+1`, and auto-Home is enabled. Automatic meridian-flip planning remains disabled until HIL qualification.

High-level SynScan paths are intentionally separate: the native SynScan hand-controller driver (`oal.skywatcher`) and SynScan App backend (`synscan-app`, UDP/11881) exchange RA/DEC with SynScan's own alignment model and never pass through the direct Motor Controller axis transform. Only direct Wi-Fi Motor Controller mode (`synscan-wifi`, UDP/11880) shares v6 with native EQDrive serial.

## Mechanical Home and Park

Home/Park is deliberately not stored as celestial RA/DEC for native raw-axis mounts. The intended direct-MC startup workflow is to put the GEM in the repeatable polar Home pose (counterweight axis down, DEC/telescope axis along the polar axis) before connect/power-on. The controller counts observed at that connect become the session mechanical Home/Park reference, and `autoHomeSync` immediately restores the sky model without a manual near-pole Sync. A later plate-solve Sync can refine pointing but is not required to begin normal GOTO operation.

Use **Calibrate current physical pose as persistent Home / Park** at the desired physical pose. Native EQDrive stores the current mechanical axes as both OAL Home and Park. Classic ASCOM has no OAL mechanical-axis interface, so the same action calls the ASCOM driver's standard `SetPark` capability. To obtain identical native and ASCOM park behavior, calibrate each backend once at the same physical pose without moving the mount between backend switches.

## Safety status

From v0.2.10.38, the configurable native GOTO envelope is a **true angular sky separation**. This fixes the pole singularity: a target only a few degrees away on the celestial sphere can require a very large RA-axis rotation near Dec ±90°. The raw native transport therefore has a separate fixed 180° per-axis mechanical hard cap.

The preferred API/profile name is `maxGotoSkyDeltaDeg`; `maxGotoAxisDeltaDeg` remains a backward-compatible alias. Automatic meridian-flip planning remains disabled by default, so large supervised slews should still be HIL-qualified before unattended use.

Alt-azimuth coordinate conversion is present, but production two-axis sidereal tracking and field-derotator control are future work. Fork-equatorial and equatorial-platform profiles share the equatorial hour-angle foundation without GEM pier-flip geometry.
