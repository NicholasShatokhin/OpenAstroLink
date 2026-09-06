# OpenAstroLink / OpenAstroSuite v0.2.10.53

## Sky Map framing

v0.2.10.53 completes the three planned camera-footprint stages on top of the v0.2.10.51–52 offline Sky Map navigation MVP.

- Every successful main-camera plate solve stores `lastSolvedFrame`: J2000 center, measured angular width/height, solver PA, solved pixel dimensions and provenance.
- Sky Map draws that measured frame as a rotated celestial rectangle.
- Independent predicted main/guide-camera footprints are derived from the optical profile before a solve and have separate visibility controls and position angles.
- The Scheduler mosaic rows/columns/overlap/rotation can be previewed directly as a Sky Map tile grid; main Sky Map PA and Scheduler mosaic rotation are synchronized.
- `Use in Scheduler` transfers the selected J2000 target and current main framing rotation.
- OpenAstroSuite can export Solved/Main/Guide/Mosaic-envelope footprints to Stellarium's optional Remote Control HTTP plugin. The standard Telescope Control TCP bridge remains mount position/GOTO only.
- Stock Stellarium supplies one rectangular FOV marker, so frame export intentionally replaces the previous exported frame instead of pretending multiple persistent rectangles are available.

## Compatibility

- Windows x64, Linux x86_64 and Raspberry Pi ARM64 build foundations remain unchanged.
- Native OAL drivers remain the default; INDI remains opt-in.
- Direct-MC mount geometry v9 is unchanged and remains HIL-qualified/frozen.

## Validation still required

Physical/UI validation is required for solved-frame orientation sign, main/guide profile FOV sizes, mosaic planner overlap, Scheduler framing transfer and Stellarium Remote Control display against a running Stellarium instance.
