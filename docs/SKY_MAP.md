# Sky Map — offline mount navigation

**Introduced:** v0.2.10.51

OpenAstroSuite includes a lightweight, fully offline **Sky Map** in the left-side workspace. It is intended as a basic telescope-navigation surface when Stellarium is not running; the Stellarium bridge remains supported for users who want a full planetarium.

## Current MVP

- horizon/all-sky projection using the observer site and current UTC;
- bright-star catalogue plus a compact set of Messier/DSO targets;
- constellation guide lines for several common naked-eye patterns;
- N/E/S/W and 30°/60° altitude guides;
- mouse drag to pan and wheel to zoom;
- click to select, double-click to GOTO;
- search by object name (`Vega`, `Polaris`, `M31`, `M42`, ...);
- selected-object RA/DEC (J2000), Alt/Az and magnitude;
- live telescope marker from the active OAL mount state;
- plate-solved position marker;
- approximate main-camera field-of-view overlay from the active optical profile;
- actions for **Slew**, **Sync**, **Abort slew**, **Park**, **Unpark** and **Use in Scheduler**.

All telescope actions go through `ObservatoryController` and therefore the active OAL mount backend. The Sky Map does not contain an alternative mount-geometry implementation and does not bypass operation/safety policy.

## Coordinate contract

Catalogue positions are J2000. Rendering uses the existing `equatorial_frames` conversion path to precess to JNow and convert to horizontal coordinates for the configured observer site and UTC. The HIL-qualified direct-MC mount coordinate model v9 is not touched by the Sky Map.

## Scope boundary

This is deliberately a navigation MVP, not a replacement for a full planetarium. The nearest Beta does **not** require photographic sky surveys, a complete deep-sky catalogue, planet ephemerides, Milky Way textures or recommendation/Smart Telescope logic. Those richer presentation layers belong to later UI work / OAL 1.0.

## Validation

Before Beta release, verify on real hardware:

1. mount marker agrees with reported mount coordinates;
2. search/select of a known star shows plausible Alt/Az for the configured site/time;
3. Sky Map GOTO and Mount-tab GOTO reach the same target;
4. double-click GOTO can be aborted from the Sky Map;
5. Sync changes the mount pointing model only through the active backend;
6. solved marker agrees with a known plate-solve result;
7. `Use in Scheduler` transfers the selected J2000 coordinate without changing it.
