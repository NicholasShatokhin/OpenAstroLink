# Equatorial coordinate frames

OpenAstroLink uses **J2000** as the canonical equatorial coordinate frame at the API, session, Stellarium and plate-solving boundaries.

## Supported input frames

- `J2000` — mean equator/equinox of J2000.0. This is the default when `coordinateFrame` is omitted.
- `JNOW` — mean equator/equinox of the current UTC date (of-date). OAL precesses it to J2000 before it enters the mount-control layer.

The GUI Mount tab exposes the same selector. Mount status is canonicalized to J2000 by the node and can be displayed as either J2000 or JNow in the GUI.

## Precession model

The core uses the standard IAU-1976 / Meeus J2000-to-date precession rotation. It intentionally models **precession only**. Nutation, annual aberration, atmospheric refraction and full apparent/topocentric corrections are not folded into the generic J2000/JNow conversion. Closed-loop plate solving remains the authoritative way to remove residual pointing error.

## Classic ASCOM

Classic ASCOM drivers expose `EquatorialSystem`. The OAL ASCOM helper reads it. `equJ2000` is passed through; `equTopocentric` is treated as an of-date mount frame and converted at the compatibility boundary. Other legacy ASCOM frames remain pass-through/compatibility territory until a dedicated transform is implemented.

## Stellarium

The Stellarium telescope bridge is treated as a J2000 catalogue-coordinate interface. Incoming GOTO coordinates enter OAL as J2000, and outgoing mount positions are sent after canonicalization to J2000.

## Synchronized Mount-tab target fields (v0.2.10.35)

The Mount tab exposes four editable representations of the same target:

- Equatorial J2000 RA/DEC;
- Equatorial JNow RA/DEC;
- horizontal azimuth/altitude (azimuth from true north through east);
- Galactic longitude/latitude `l/b`.

Editing any pair immediately recalculates the other three. The transformation uses current UTC and the observatory latitude/longitude from the active profile. Horizontal coordinates are therefore time- and site-dependent. A visible epoch/site line records the UTC and observer position used for the latest conversion.

J2000 remains the canonical command frame. A GOTO or Sync from the GUI is converted to J2000 before it reaches the mount backend regardless of which synchronized field the user edited.

A Polaris J2000 preset is provided as an acquisition aid. Filling the preset does **not** silently Sync the mount. Mechanical Park is also not a sky-coordinate Sync. For a raw EQDrive installation the safe session start is: reach the known home/park pose, establish a real sky anchor (center Polaris/another known object or use a plate solve), perform one Sync, then use Stellarium or coordinate GOTO.
