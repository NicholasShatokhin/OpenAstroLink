# OpenAstroLink v0.2.10.51 release checkpoint

## Summary

v0.2.10.51 adds a lightweight offline Sky Map to OpenAstroSuite so the telescope can be navigated without an external Stellarium session. The feature is intentionally a supervised Beta navigation surface, not Smart Telescope UX.

## Sky Map

- New left-side `Imaging / Sky Map` workspace tabs.
- Offline horizon projection with bright stars, selected Messier/DSO targets and several constellation guide lines.
- Search, pan, zoom, object selection and double-click GOTO.
- Telescope, plate-solved center and approximate camera-FOV overlays.
- Slew, Sync, Abort, Park/Unpark and Scheduler-target transfer use the existing `ObservatoryController` contract.

## Frozen mount contract

Direct-MC coordinate model v9 remains HIL-qualified and unchanged (`Axis1Sign=+1`, `Axis2Sign=-1`). The temporary hidden EQDrive 15° qualification gate was already removed in v0.2.10.50. v0.2.10.51 does not change mount geometry, Home/Park conventions, transport polarity or direct-MC planning.

## Build baseline

The previously confirmed Windows x64, native Linux x86_64 and Raspberry Pi/Linux ARM64 build foundation is retained. macOS physical qualification and Pi 5 physical runtime/HIL remain pending.

## Nearest Beta sequence

1. supervised mount sanity after the removed qualification cap;
2. HIL autofocus;
3. HIL auto-exposure;
4. scheduler HIL;
5. mosaic HIL;
6. Polar Alignment HIL;
7. Sky Map real-mount navigation smoke as part of normal UI qualification.
