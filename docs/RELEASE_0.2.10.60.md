# OpenAstroLink v0.2.10.60 — Assisted Polar / Observable Sky source checkpoint

Date: **2026-09-14**

Qualification: **source/static-qualified only**. Fresh Windows build and real-mount/real-sky HIL are still required for the new `.60` functionality. The frozen direct-MC mount geometry v9 is unchanged.

## Added

### Assisted Polar Alignment

- manual-recenter sampling from a selected Sky Map target;
- optional plate-solved centred-field sample;
- local Alt/Az, UTC-aware 3-D SVD/Kabsch fit;
- estimated physical polar-axis direction;
- signed altitude/azimuth correction;
- total error, RMS residual, sample span and confidence diagnostics;
- local and remote controller support;
- HTTP API and event-stream results.

### Observable Sky Region

- persisted two-corner local Alt/Az rectangle;
- north-wrap representation (`minAzDeg > maxAzDeg`);
- Sky Map overlay;
- optional automated-GOTO rejection outside visible sky;
- manual joystick explicitly remains unrestricted;
- Scheduler target eligibility and runtime block deferral;
- exposure/SER-safe switching: active science acquisition is not aborted merely because a target crosses the region boundary;
- conservative crash-resume cursor so a deferred unfinished block is not silently skipped.

## Safety model

Observable Sky is a visibility/planning constraint. It is separate from:

1. frozen mount geometry v9;
2. raw-axis mechanical collision guard;
3. Polar Motion Limits used by the automatic polar-alignment RA-slew workflow.

## Previous functionality retained

v0.2.10.60 keeps the v0.2.10.59 Night Vision/Strict Night Mode and optional black→red monochrome/raw preview, WebRTC/OALV high-rate preview architecture, Dual Live, native drivers, scheduler executors, mosaic and polar-alignment workflows.

## Required HIL

- fresh Windows `.60` configure/build;
- 3–5 Assisted Polar manual-recenter samples and correction/convergence repeat;
- two-corner Observable Sky calibration + Sky Map overlay;
- automated-GOTO rejection outside region;
- manual joystick outside region;
- Scheduler transition after completed FITS/SER and later resumption of deferred work.

## Qualification at checkpoint creation

Static regression: **86/86 scripts PASS**. The dedicated restricted-sky/Assisted-Polar guard passes **70 assertions plus a synthetic SVD rotation-fit recovery test**. Frozen direct-MC geometry v9 and EQDrive source are byte-identical to the previous HIL base. Fresh Windows compilation and real-sky/mount HIL are intentionally still pending.
