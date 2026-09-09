# Sky Map — offline mount navigation and framing

> Current synchronized snapshot: **v0.2.10.58-buildfix9 (2026-09-09)**. See `STATUS.md`, `CURRENT_CHECKLIST.md` and `RELEASE_0.2.10.58.md` for current qualification boundaries.


**Introduced:** v0.2.10.51  
**Free-point targeting:** v0.2.10.52  
**Camera footprints / mosaic framing / Stellarium frame export:** v0.2.10.53

OpenAstroSuite includes a lightweight, fully offline **Sky Map** in the left-side workspace. It is a native OAL telescope-navigation and framing surface when a full planetarium is not required. Stellarium remains supported through both the standard Telescope Control bridge and an optional Remote Control frame-export path.

## Navigation

- horizon/all-sky projection using the observer site and current UTC;
- bright-star catalogue plus selected Messier/DSO targets and simple constellation guides;
- N/E/S/W and altitude guides;
- pan/zoom/search;
- click a catalogue object **or any arbitrary visible sky position** to select it;
- double-click either kind of target to GOTO;
- live telescope marker, solved-position marker and J2000/Alt-Az target readout;
- controller-backed **Slew**, **Sync**, **Abort slew**, **Park**, **Unpark** and **Use in Scheduler**.

All telescope actions still pass through `ObservatoryController` and the active OAL mount backend. Sky Map does not contain a second mount-geometry model. The HIL-qualified direct-MC v9 geometry remains frozen.

## v0.2.10.53 camera footprints

Sky Map now has three independent footprint layers:

1. **Solved frame** — measured from the last successful main-camera plate solve. OAL stores `lastSolvedFrame` with J2000 center, width/height in degrees, solver rotation/position angle, solved image pixel dimensions and a measured/predicted flag. Width and height are derived from the actual solved image dimensions and solver pixel scale, so ROI/binning of the solved image is naturally reflected.
2. **Main planned frame** — predicted before a solve from the main optical profile (`focalLength`, pixel size, sensor width/height) and a user-editable main-camera position angle.
3. **Guide planned frame** — the same concept for the independent guide optical train and guide-camera position angle.

Each layer can be shown/hidden independently and can display its angular size and PA. The map draws a true rotated rectangular footprint on the celestial sphere rather than the old approximate circular/elliptical FOV marker.

The rotation convention is shared with `SolveResult` and `MosaicFitsBlock`: **sensor +X is rotated by `rotationDeg` from local celestial east toward celestial north**.

## Mosaic planner

The `Mosaic planner grid` option reuses the Scheduler mosaic settings:

- columns / rows;
- overlap percentage;
- main-camera rotation.

The selected Sky Map target is the mosaic center. If no target is selected, the current telescope pointing is used as a framing preview anchor. Tile centers are generated in the local tangent plane and rendered with the same rotated camera footprint. The Sky Map main PA and Scheduler mosaic rotation are synchronized, and **Use in Scheduler** transfers both the selected J2000 target and the current main-camera framing angle.

This is planning visualization only; execution remains the existing node-side Scheduler/mosaic engine.

## Stellarium frame export

The standard Stellarium Telescope Control TCP protocol remains responsible only for telescope position/GOTO. It has no camera-footprint packet.

When Stellarium's optional **Remote Control** plugin is enabled, Sky Map can export one selected OAL footprint at a time to Stellarium's built-in rectangular FOV marker. Available sources are:

- solved frame;
- main planned frame;
- guide planned frame;
- mosaic envelope.

The default Remote Control URL is `http://127.0.0.1:8090`. OpenAstroSuite discovers the actual `SpecialMarkersMgr` property IDs from `/api/stelproperty/list`, sets the rectangular marker width/height/rotation/visibility through `/api/stelproperty/set`, centers Stellarium on the frame with `/api/main/view`, and adjusts the viewport FOV with `/api/main/fov`.

Stock Stellarium exposes one built-in rectangular FOV marker, so exporting another OAL frame replaces the previous exported footprint. Rendering multiple simultaneously persistent OAL rectangles inside Stellarium would require a dedicated Stellarium plugin; it is not part of this Beta feature.

## Coordinate contract

Catalogue positions and free-point selections are stored as J2000. Rendering uses the existing `equatorial_frames` conversion path. A free-point click is inverted to Az/Alt and converted once to J2000, so Scheduler/GOTO receive an inertial sky coordinate rather than a permanently fixed horizontal direction.

Camera-frame corners are constructed around a J2000 center in a local east/north tangent plane, rotated by the frame PA, then transformed to horizontal coordinates only for display. The same J2000 center is sent to Stellarium Remote Control.

## Validation

Before Beta release verify:

1. known-object and free-point GOTO still match the Mount tab;
2. solved rectangle center agrees with the solved marker;
3. measured solved width/height agree with `scaleArcsecPerPx × image dimensions`;
4. a deliberately rotated camera produces the same orientation sign on Sky Map and in the solver result;
5. main/guide predicted frame sizes agree with the optical profile;
6. changing main PA updates the Scheduler mosaic rotation and vice versa;
7. 2×2 and 3×2 planner grids use the requested overlap and remain centered on the selected target;
8. **Use in Scheduler** preserves J2000 target coordinates and framing rotation;
9. with Stellarium Remote Control enabled, each of Solved/Main/Guide/Mosaic envelope can be exported and displayed at the expected center, size and PA;
10. disabling/hiding the exported Stellarium frame works without affecting the Telescope Control bridge.

Catalogue-object → Scheduler transfer is already HIL/UI-confirmed. v0.2.10.53 adds the footprint/framing path without changing mount v9 geometry.


## 2026-09-06 HIL checkpoint

Free-point Sky Map GOTO is physically confirmed on the real mount, and Sky Map target transfer into Scheduler is confirmed in the running GUI. Current code also mirrors the selected target into Mount-tab J2000 fields. Camera-footprint and Stellarium-export overlays remain HIL-pending.
