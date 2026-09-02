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

Generic/legacy equatorial backends retain their own coordinate model. Direct Sky-Watcher Motor Controller backends (`oal.eqdrive` serial and `synscan-wifi` UDP/11880) use **coordinate model v9**. Both transports expose one mechanical axis system: the controller counts captured in the prepared physical Home pose are `Axis1=0°, Axis2=0°`; raw step counts remain diagnostics only.

v9 retains the v7 conventional EQMOD-style GEM mechanical hour-angle/declination pointing-state geometry. A full four-sign recomputation of the 2026-09-02 HIL shows that the exact east/west mirror is produced by the DEC/polar-distance mapping, so the qualified direct-MC mapping is `Axis1Sign=+1`, `Axis2Sign=-1`. In the northern hemisphere the counterweight-down polar Home is defined as sky `HA=-90° (-6 h), Dec=+90°` on `pier=west`. Away from the pole, for `HA <= 0°` the west branch is `Axis1 = HA + 90°`, `Axis2 = +(90°-Dec)`; for `HA > 0°` the opposite physical pointing state is `Axis1 = HA - 90°`, `Axis2 = -(90°-Dec)`. The southern hemisphere mirrors the corresponding pole/axis convention.

This replaces the v6 polar-telescope-frame experiment. v6 correctly made native serial and UDP transports agree, but it treated `(A,+P)` and `(A+180°,-P)` as freely interchangeable and selected whichever representation produced the shortest motor movement. Real HIL on 2026-08-31 showed that this can choose the wrong **physical GEM pointing state**: serial and Wi-Fi then move identically but both miss, while EQMOD Classic ASCOM on the same hardware points correctly. v7 therefore derived the physical branch from hour angle/side of pier instead of a shortest-path representation choice. Real HIL on 2026-09-02 then showed a second, independent issue: the branch/side-of-pier matched Classic EQMOD ASCOM, but direct-MC physical RA motion was mirrored east/west. v8 tested an Axis1-polarity hypothesis, but the follow-up HIL remained mirrored. v9 fixes the actual controller-to-canonical Axis2 polarity; the EQMOD branch equations and raw serial/UDP protocol remain unchanged.

Sky-Watcher Motor Controller position packets are decoded through the protocol's signed `0x800000` reference by the shared codec. At connect, OAL captures the resulting normalized counts as the session Home/Park reference; no additional quarter-turn/90° transport offset is invented. Native EQDrive serial and direct Wi-Fi use the same `skywatcher_mc::makeGotoPlan()` after Core geometry, so any remaining difference between them is transport I/O rather than astronomical kinematics.

Profiles older than v7 still receive the one-time legacy Home/Park reset. Any direct-MC profile below v9 then migrates to `Axis1Sign=+1`, `Axis2Sign=-1`, keeps the standard `0°,0°` Home/Park convention, and enables auto-Home. A v7 profile keeps its already valid Home/Park values while only the physical Axis1 mapping changes. Automatic meridian-flip orchestration is still not considered HIL-qualified for unattended use.

High-level SynScan paths remain separate: native `oal.skywatcher` hand-controller mode and `synscan-app` exchange celestial RA/DEC with SynScan's own alignment model. Only direct Motor Controller Wi-Fi (`synscan-wifi`, UDP/11880) shares v9 with native EQDrive serial.

## Mechanical Home and Park

Home/Park is deliberately not stored as celestial RA/DEC for native raw-axis mounts. The intended direct-MC startup workflow is to put the GEM in the repeatable polar Home pose (counterweight axis down, DEC/telescope axis along the polar axis) before connect/power-on. The controller counts observed at that connect become the session mechanical Home/Park reference, and `autoHomeSync` immediately restores the sky model without a manual near-pole Sync. A later plate-solve Sync can refine pointing but is not required to begin normal GOTO operation.

Use **Calibrate current physical pose as persistent Home / Park** at the desired physical pose. Native EQDrive stores the current mechanical axes as both OAL Home and Park. Classic ASCOM has no OAL mechanical-axis interface, so the same action calls the ASCOM driver's standard `SetPark` capability. To obtain identical native and ASCOM park behavior, calibrate each backend once at the same physical pose without moving the mount between backend switches.

## Safety status

From v0.2.10.38, the configurable native GOTO envelope is a **true angular sky separation**. This fixes the pole singularity: a target only a few degrees away on the celestial sphere can require a very large RA-axis rotation near Dec ±90°. The raw native transport therefore has a separate fixed 180° per-axis mechanical hard cap.

The preferred API/profile name is `maxGotoSkyDeltaDeg`; `maxGotoAxisDeltaDeg` remains a backward-compatible alias. Automatic meridian-flip planning remains disabled by default, so large supervised slews should still be HIL-qualified before unattended use.

Alt-azimuth coordinate conversion is present, but production two-axis sidereal tracking and field-derotator control are future work. Fork-equatorial and equatorial-platform profiles share the equatorial hour-angle foundation without GEM pier-flip geometry.
