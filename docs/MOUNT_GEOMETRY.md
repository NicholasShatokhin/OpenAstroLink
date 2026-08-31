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

For a GEM, OAL converts J2000 to of-date coordinates, obtains local sidereal time from UTC and observatory longitude, derives hour angle, selects a pier branch, and then maps the result to mechanical axes. One Sync on a known sky position establishes the installation-specific encoder offset and axis signs. Automatic meridian-flip planning remains disabled by default until HIL qualification.

The axis-sign fields describe installation orientation. They must not be replaced by ad-hoc RA/DEC sign inversions in a hardware driver. The current EQDrive HIL installation is consistent with the default `+1/+1` profile after Sync, but other installations can reverse either axis.

## Mechanical Home and Park

Home/Park is deliberately not stored as celestial RA/DEC for native raw-axis mounts. A repeatable GEM Home pose (counterweight axis down, DEC/telescope axis along the polar axis) establishes both a persistent mechanical reference and, when `autoHomeSync` is enabled, a startup sky model without a manual near-pole Sync.

Use **Calibrate current physical pose as persistent Home / Park** at the desired physical pose. Native EQDrive stores the current raw axes as both OAL Home and Park. Classic ASCOM has no OAL raw axes, so the same action calls the ASCOM driver's standard `SetPark` capability. To obtain identical native and ASCOM park behavior, calibrate each backend once at the same physical pose without moving the mount between backend switches.

## Safety status

From v0.2.10.38, the configurable native GOTO envelope is a **true angular sky separation**. This fixes the pole singularity: a target only a few degrees away on the celestial sphere can require a very large RA-axis rotation near Dec ±90°. The raw native transport therefore has a separate fixed 180° per-axis mechanical hard cap.

The preferred API/profile name is `maxGotoSkyDeltaDeg`; `maxGotoAxisDeltaDeg` remains a backward-compatible alias. Automatic meridian-flip planning remains disabled by default, so large supervised slews should still be HIL-qualified before unattended use.

Alt-azimuth coordinate conversion is present, but production two-axis sidereal tracking and field-derotator control are future work. Fork-equatorial and equatorial-platform profiles share the equatorial hour-angle foundation without GEM pier-flip geometry.
