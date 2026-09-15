# Observable Sky Region

Status: **v0.2.10.60 source/static-qualified; fresh Windows build and HIL pending.**

## Purpose

Observable Sky Region describes where the optical system can actually see unobstructed sky from the current installation. It is aimed at balconies, windows, courtyards, roofs and sites with buildings or trees where the astronomical horizon is not the useful observing boundary.

It is deliberately **not** a hard mechanical safety model.

- **Observable Sky Region**: visibility/planning constraint.
- **Raw-axis mechanical guard**: hard collision/mechanical limit.
- **Polar Motion Limits**: separate optional safe region used by the polar-alignment slew workflow.

Manual joystick movement remains available even outside Observable Sky Region so the operator can calibrate or recover the telescope.

## Two-corner quick calibration

v0.2.10.60 implements the quick rectangle workflow:

1. manually move the telescope to one visible corner;
2. press **Capture first visible corner from current pointing**;
3. move to the opposite visible corner;
4. press **Capture opposite corner and save rectangle**.

The two points are converted to local Alt/Az. Altitude is stored as the min/max of both points. Azimuth uses the shorter of the two arcs; if the window crosses north, it is stored with `minAzDeg > maxAzDeg`.

This quick mode is intended for a reasonably compact rectangular opening. Arbitrary polygons/multiple obstruction masks are a later extension.

## Profile fields

`TelescopeProfile.observableSky` contains:

- `enabled`;
- `minAzDeg`, `maxAzDeg`;
- `minAltDeg`, `maxAltDeg`;
- `rejectAutomatedGoto`;
- `schedulerEligibility`;
- `recheckSeconds`.

The region is persisted with the telescope profile and is available to local and remote GUIs.

## Sky Map

When enabled, the rectangle is drawn directly on Sky Map. Wrapped regions crossing north are rendered as one continuous local-horizon window.

## Automated GOTO policy

If `rejectAutomatedGoto` is enabled, synchronous and asynchronous automated GOTO requests whose destination is outside the observable region are rejected before motion starts.

This policy does **not** affect manual joystick commands.

## Scheduler policy

If `schedulerEligibility` is enabled:

- before starting a block, OAL checks whether its current target is inside the region;
- for a mosaic, the current tile centre is checked;
- if a target leaves the region during a FITS exposure or planetary SER, the active acquisition is allowed to finish;
- at the next safe acquisition boundary the current block may be deferred and another due/visible block selected;
- partial progress of a deferred block is preserved for the lifetime of the running node process;
- if no due target is visible, Scheduler enters `waiting-observable-sky` and periodically rechecks the plan;
- a future block is not started before its `startAtUtc` merely because it happens to be visible.

### Crash-resume limitation

The existing Beta scheduler persists a linear next-block cursor, not a full out-of-order completion ledger. v0.2.10.60 therefore preserves reordered partial progress in-process and writes a conservative resume hint so an unfinished deferred block is not silently skipped. After a crash, a later block that had completed out of order may be repeated. Durable per-block execution journaling remains an OAL 1.0 item.

## API

- `POST /api/v1/observable-sky/corner/1`
- `POST /api/v1/observable-sky/corner/2`
- `POST /api/v1/observable-sky/clear`
- normal `GET/POST /api/v1/profile` carries the persisted policy.

## HIL acceptance

1. move to one lower/left visible limit and capture corner 1;
2. move to the opposite visible limit and capture corner 2;
3. verify the Sky Map overlay;
4. verify an automated GOTO outside the region is rejected when policy is enabled;
5. verify manual joystick motion remains possible outside the region;
6. create at least two scheduler targets, one leaving the region and one inside it;
7. verify the running FITS/SER is not aborted at the boundary;
8. verify transition happens only after the current exposure/SER completes;
9. verify the deferred block resumes if it becomes visible again.
