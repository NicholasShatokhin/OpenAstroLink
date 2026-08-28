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
