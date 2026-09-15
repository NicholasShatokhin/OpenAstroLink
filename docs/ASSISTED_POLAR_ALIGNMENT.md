# Assisted Polar Alignment — manual recenter workflow

Status: **v0.2.10.60 source/static-qualified; Windows build and real-sky HIL pending.**

## Purpose

Assisted Polar Alignment is for restricted-sky observing where Polaris or the normal wide-sky polar workflow is unavailable. The operator may start with only a rough compass/latitude setup, choose objects that are actually visible through a balcony/window/roof opening, let the mount GOTO approximately, manually re-centre each object with the normal mount joystick, and record the resulting discrepancy.

The mode complements the existing automatic multi-solve polar-alignment workflow. It does not change mount geometry v9 and does not replace the raw-axis mechanical safety guard.

## Operator workflow

1. Roughly orient the equatorial mount using compass/north and site latitude.
2. Select a well-known visible object on Sky Map.
3. Press **GOTO selected Sky Map target**.
4. Use the normal manual mount joystick until the object is centred in the camera.
5. Press **Record centred selected target**.
6. Repeat for at least two targets; three or more well-separated targets are strongly preferred.
7. Press **Estimate polar correction**.
8. Adjust the mount's physical altitude/azimuth bolts by the reported signed corrections.
9. Repeat the sample/estimate cycle to verify convergence.

A plate-solve-assisted variant is available: after centring and solving the camera field, **Record centred plate-solved field** records the solved sky centre instead of relying on the selected catalogue coordinate.

## What is recorded

Each sample contains:

- the real target/solved centre in equatorial coordinates;
- the coordinate currently reported by the mount;
- UTC timestamp;
- source (`manual-target` or `plate-solve`).

The timestamp matters because the fit is performed in the local horizontal frame, whose orientation relative to equatorial coordinates changes with sidereal time.

## Fit model

For each sample OAL converts both the mount-reported direction and the real centred direction to local Alt/Az at that sample's UTC. It then finds the best rigid 3-D rotation `R` that maps reported direction vectors to observed direction vectors in a least-squares sense.

The same fitted rotation is applied to the ideal local polar-axis direction. The result is reported as:

- estimated polar-axis azimuth and altitude;
- signed altitude correction in arcminutes;
- signed azimuth correction in arcminutes;
- total angular polar error;
- fit RMS residual;
- angular span of the samples;
- confidence (`good`, `limited`, or `poor`).

The current implementation uses an SVD/Kabsch rotation fit.

## Interpretation and limitations

This is an **effective pointing-frame fit**, not a magical separation of every mount imperfection. Rigid pointing effects that look like a global rotation can be absorbed into the fit. Cone error, flexure, backlash, refraction, poor time/site data, stale plate solves, and encoder/model errors can bias the inferred polar axis.

For that reason:

- prefer fresh plate solves where practical;
- use three or more samples;
- spread samples across the available sky instead of clustering them tightly;
- repeat the estimate after applying the mechanical correction;
- treat a large RMS or small sample span as a warning, not as a precise correction.

The GUI intentionally exposes RMS, span and confidence so the operator can judge whether the restricted visible patch contains enough geometric information.

## Confidence heuristic in v0.2.10.60

- **good**: at least 3 samples, span >= 15°, RMS <= 5 arcmin;
- **limited**: at least 3 samples, span >= 5°, RMS <= 15 arcmin;
- otherwise **poor**.

These thresholds are an initial operational heuristic and require real-sky HIL calibration.

## API

- `POST /api/v1/assisted-polar/clear`
- `POST /api/v1/assisted-polar/sample-target`
- `POST /api/v1/assisted-polar/sample-solved`
- `POST /api/v1/assisted-polar/estimate`

Sample-count/result events are also exposed over the normal OAL event stream.

## HIL acceptance

The first real-sky qualification should:

1. start from a deliberately rough polar setup;
2. record 3–5 visible targets using manual recentering;
3. save the first estimate, RMS and span;
4. apply the physical Alt/Az correction;
5. repeat the sample set;
6. verify that the estimated total polar error decreases;
7. compare against the existing automatic polar alignment or an independent reference when possible.
