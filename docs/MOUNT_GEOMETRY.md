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

Park is deliberately not stored as celestial RA/DEC. It is a fixed mechanical axis position. The default profile is:

```
Axis 1 = 90 deg
Axis 2 = 0 deg
```

The Profile tab can change Home/Park axis coordinates. The Mount tab provides **Set current mechanical axes as Park** and **Restore default mechanical Park (90°, 0°)**. Native EQDrive and direct SynScan/EQDrive Wi-Fi use this mechanical park. Classic ASCOM continues to use the ASCOM driver's own Park/Unpark implementation.

## Safety status

Native EQDrive and direct Wi-Fi celestial GOTO no longer impose the temporary 15-degree HIL envelope. They use the shortest mechanical-axis path (up to 180 degrees). Automatic meridian flips remain disabled, so long slews must be supervised until pier-side and physical-limit behavior is fully HIL-qualified.

Alt-azimuth coordinate conversion is present, but production two-axis sidereal tracking and field-derotator control are future work. Fork-equatorial and equatorial-platform profiles share the equatorial hour-angle foundation without GEM pier-flip geometry.
