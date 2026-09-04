# OpenAstroLink scheduler and autonomous acquisition specification

**Canonical language:** English  
**Target:** staged delivery from the first supervised beta through OAL 1.0  
**Implementation status:** v0.2.10.50 retains a mixed DSO/planetary/mosaic node executor with a persistent per-block observing calendar. Each `ObservationBlock` can carry its own `startAtUtc`, optional `parkAfter` / `autoUnparkBefore`, and the node persists the plan, armed state and next-block cursor. DSO blocks execute `slew -> adaptive solve/recenter -> autofocus -> FITS/RAW`; planetary blocks execute `GOTO -> full-frame acquisition/detection -> planetary autofocus -> hardware ROI -> finite SER`; mosaic blocks generate sky tiles from optical-profile FOV and reuse the DSO executor per tile. A process restart between blocks resumes at the first unfinished block; a restart inside a block restarts that block. Mid-frame/SER checkpoints, weather/roof safety, meridian recovery and full unattended OAL 1.0 hardening remain planned.

### v0.2.10.49 persistent calendar, mosaic blocks and inter-block parking

Scheduling is now a property of each observation block rather than one timestamp on the whole plan. A block with no `startAtUtc` runs as soon as the previous block completes (or as soon as the calendar is armed when it is first). A block with a future UTC timestamp enters `waiting-block-start` until its own event time. This allows one persisted calendar to represent eclipses, conjunctions, planetary windows, DSO sessions and other events many months ahead.

The node persists the complete plan, whether it is armed, and the index of the first unfinished block. `parkAfter=true` parks after a completed event before the scheduler waits for the next event; `autoUnparkBefore=true` restores observing state immediately before that block begins. A clean operator Stop disarms the calendar. If the node process crashes/restarts while armed, completed blocks are not repeated; the unfinished block restarts from its block boundary.

`mosaic-fits` is a third executable acquisition mode. The block coordinate is the mosaic center. Tile centers are calculated in a tangent plane from main-sensor FOV (`sensorWidthPx`, `sensorHeightPx`, image scale), rows/columns, overlap and grid rotation. Serpentine traversal reduces long return slews. Every tile inherits the normal DSO solve/recenter/autofocus/FITS policy, with optional recenter/autofocus at each tile boundary.

Polar Alignment remains a guided solve/sample workflow in v0.2.10.49, but its RA-offset mount motion can now be restricted to a persisted horizontal safe region. OAL samples the entire requested path in Az/Alt and refuses the slew if any intermediate point leaves the configured region.

### v0.2.10.48 scheduler lifecycle and editing

The supervised scheduler now supports immediate or future in-memory start, editing/deleting/reordering plan blocks in the GUI, copying the mount's current J2000 pointing into a block, and a `waiting-camera` preflight. If interactive Live View owns the main-camera lock when a plan starts, the node cancels/finalizes Live View first and begins autonomous acquisition only after the camera resource is free. This prevents a queued science exposure from appearing to hang behind a continuous preview.


## 1. Goals

The OpenAstroLink scheduler is an observatory workflow engine, not merely a timer or a list of exposures. It must orchestrate target acquisition, focusing, plate solving, centering, guiding/tracking, science capture, recovery and calibration-aware metadata while keeping the node authoritative over hardware.

The same scheduler must support two first-class acquisition families:

1. **Deep-sky imaging (DSO):** FITS/RAW still-image sequences, normally long-exposure, optionally filtered, guided and dithered.
2. **Lunar/planetary/high-speed imaging:** SER video sequences, normally short-exposure/high-frame-rate, with ROI control and continuous target centering.

A plan may mix both families in one night.

## 2. Current implementation boundary

From v0.2.10.49, the node owns one persisted `ObservationPlan` / `ObservationBlock` state machine for DSO, planetary and mosaic acquisition. All long steps execute asynchronously through the existing `OperationManager` and reuse the same hardware resource locks as interactive commands. The DSO path is:

```text
PREPARE_BLOCK -> SLEW -> SOLVE/RECENTER -> AUTOFOCUS -> CAPTURE -> [periodic corrections] -> next frame/block
```

The solve/recenter loop plate-solves the actual field, measures pointing error against the block target, performs `Sync + correction slew`, and repeats until the configured arcminute tolerance or attempt limit is reached. Recenter and autofocus may run before the first frame and/or every N completed science frames. Science captures force durable FITS/RAW output when the camera backend supports it.

`SessionTarget` remains accepted by the GUI/API for compatibility and is converted into DSO blocks. Planetary SER blocks are now executable. The implemented planetary path performs a full-frame acquisition after GOTO, detects the bright planetary/lunar target, optionally runs planet-mode autofocus, recentres a fixed-size hardware ROI on the detected centroid and records one or more finite SER runs. During a SER, ROI origin may move while width/height stay fixed; every origin change is written to a same-basename `.roi.jsonl`. Optional mount corrections use an image-response calibration rather than assuming camera orientation. The calendar is **block-boundary durable**: plan/armed state/next-block cursor survive node restart. Mid-block frame/SER progress, weather holds, meridian-flip recovery and full unattended safety remain OAL 1.0 work.

## 3. Plan model

A persisted `ObservationPlan` contains ordered or priority-scheduled `ObservationBlock` objects.

Each block shall contain:

- stable block ID and human-readable name;
- target resolver input and resolved coordinates/ephemeris;
- acquisition mode: `dso-fits`, `planetary-ser` or `mosaic-fits`;
- optical train, camera, mount and focuser bindings;
- optional guide train;
- optional per-block `startAtUtc` (independent event date/time) and future time-window constraints;
- optional `parkAfter` / `autoUnparkBefore` inter-block mount policy;
- minimum altitude / maximum airmass;
- Sun altitude/twilight constraint;
- Moon separation/illumination constraint where relevant;
- meridian/side-of-pier policy;
- weather/safety requirements when those device profiles are implemented;
- retry/recovery policy;
- completion policy (frame count, total integration, SER count, duration or time window).

The scheduler shall keep a durable checkpoint after every state transition that materially changes observatory state or science progress.

## 4. Common block execution

The normal execution state machine is:

```text
WAIT_CONSTRAINTS
 -> RESOLVE_TARGET
 -> PREPARE_DEVICES
 -> SLEW
 -> SETTLE
 -> SOLVE_AND_RECENTER        (when enabled)
 -> AUTOFOCUS                 (when due)
 -> START_TRACKING/GUIDING    (as required)
 -> ACQUIRE
 -> PERIODIC_CORRECTIONS
 -> COMPLETE_BLOCK
```

Recovery states include:

```text
RETRY_CAPTURE
REACQUIRE_TARGET
RECENTER
REFOCUS
REGUIDE
MERIDIAN_FLIP
WEATHER_HOLD
SAFE_PARK
RESUME_FROM_CHECKPOINT
FAILED
```

All long actions must use OAL asynchronous operations and resource locks rather than blocking the scheduler thread.

## 5. DSO FITS acquisition

A DSO sequence shall support:

- exposure time;
- frame count and/or requested total integration time;
- gain/ISO, offset, binning and readout mode;
- filter and filter sequence;
- FITS/RAW science output policy;
- optional guide/dither policy;
- optional subframe/ROI where supported;
- per-frame provenance and checksum.

### 5.1 Recenter policy

Plate-solve/recenter may be requested:

- before the first science frame;
- after every frame;
- every N frames;
- every N minutes;
- when guide/solve error exceeds a threshold;
- after autofocus;
- after filter changes;
- after a meridian flip;
- after a recovery/reconnect event.

The plan must allow aggressive policies such as `recenter after every exposure`.

### 5.2 Autofocus policy

Full autofocus may be requested:

- before the first science frame;
- after every frame;
- every N frames;
- every N minutes;
- after filter changes;
- after a temperature change greater than a configured delta;
- when HFR/FWHM/focus score degrades beyond a threshold;
- after a meridian flip or target reacquisition.

The scheduler must preserve the previous valid focus position and define rollback behavior if autofocus fails.

## 6. Temperature focus compensation during an exposure

OpenAstroLink shall support optional **in-exposure thermal focus compensation** for optical trains whose focuser and imaging configuration permit it.

This is distinct from a full autofocus run. The system uses a calibrated coefficient such as `focuser steps / degree C` (optionally piecewise or filter-specific) and applies small bounded corrections while a long exposure is running.

Required controls:

- per-optical-train enable/disable;
- calibrated temperature-focus model and confidence;
- temperature source (focuser sensor, weather station or another bound sensor);
- minimum temperature delta before correction;
- maximum step size per correction;
- maximum accumulated motion per exposure and per minute;
- minimum interval between moves;
- camera/focuser capability check for motion during exposure;
- defer-to-frame-boundary fallback when the hardware or optical train should not move focus during integration.

Every correction must be recorded with UTC timestamp, temperature, old/new focus position and the affected exposure/frame ID. A plan may explicitly prohibit focus motion during an exposure even when the hardware supports it.

## 7. Planetary/lunar SER acquisition

Planetary blocks shall be first-class scheduler operations rather than emulated DSO exposures.

Required parameters include:

- SER duration and/or frame count;
- number of SER runs;
- exposure, gain, offset and high-speed readout mode;
- target FPS or maximum-rate mode;
- bit depth and Bayer/raw policy;
- ROI width/height and initial sensor origin;
- interval between SER runs;
- optional refocus/recenter between every SER;
- optional refocus/recenter after a configurable number of seconds or runs.

## 8. Planetary ROI and automatic centering

**v0.2.10.48 implementation:** full-frame bright-object detection, hardware ROI start, fixed-size ROI shifts during SER, ROI provenance, and optional two-axis calibrated mount micro-slew correction are implemented. The mount loop is disabled by default because it requires HIL qualification per backend; ROI-only tracking is the default.

The scheduler shall support two complementary centering loops.

### 8.1 ROI tracking

When the camera supports movable hardware ROI, the system may keep the object centered by moving the ROI window over the sensor without moving the mount.

Rules:

- ROI dimensions remain constant during a SER file unless the file is closed and a new one is started;
- sensor-origin changes must be recorded with timestamp/frame range;
- every SER sidecar must contain the initial ROI and a history of ROI moves;
- calibration frames must be reproducible for the same sensor region;
- dark/flat acquisition tools must be able to reuse a recorded ROI or ROI-history segment.

A companion `*.roi.jsonl` (or equivalent structured metadata section) is preferred when many ROI changes occur.

### 8.2 Mount recentering

The target centroid is measured in the live planetary stream. Mount correction is requested when:

- centroid error exceeds a configured threshold;
- the target approaches an ROI edge;
- ROI movement reaches a configured sensor boundary;
- drift indicates wind, backlash or tracking error that software ROI movement alone should not absorb.

Correction may use pulse guide, a bounded micro-slew or another mount-native low-disturbance operation. Hysteresis and a settle window are required to avoid oscillation between ROI moves and mount moves.

Every correction must be logged.

## 9. Fully autonomous planetary workflow

A planetary plan shall be able to execute unattended as follows:

```text
resolve topocentric ephemeris
 -> validate altitude/time/safety constraints
 -> unpark
 -> slew to planet
 -> acquire/identify target in full frame or finder mode
 -> center target
 -> planetary autofocus
 -> establish requested ROI
 -> start SER
 -> continuously monitor centroid
 -> move ROI and/or correct mount as policy requires
 -> optionally refocus/recenter between SER runs
 -> repeat requested runs
 -> park or continue with the next block
```

For initial acquisition, the implementation may use a larger camera ROI/full frame, a guide/finder camera or a short star-field solve before switching to the small planetary ROI. Plate solving is not required on every planetary frame.

## 10. Meridian handling

A `near-meridian` target is a target whose **hour angle is close to zero**, meaning it is close to crossing the observer's local north-south meridian. This is a critical GEM test and scheduler condition because the mount may need to choose or change side-of-pier there.

Scheduler meridian policy shall eventually support:

- do not start a block if it cannot finish before the flip margin;
- finish current exposure then flip;
- abort/restart an exposure when a hard safety limit is reached;
- slew to the opposite GEM pointing state;
- solve/recenter;
- restore guiding;
- restore focus if required;
- resume the exact sequence checkpoint.

Automatic meridian flip is planned for the autonomous/OAL 1.0 path and is not production-qualified today.

## 11. Metadata and reproducibility

Each science artifact shall be traceable to its scheduler context.

Record at minimum:

- plan/block/run/frame IDs;
- target and resolved ephemeris/coordinates;
- UTC start/end;
- mount position/side-of-pier;
- solve/recenter history;
- focus position, temperature and focus corrections;
- guiding state/quality when available;
- exposure/gain/offset/binning/readout;
- filter;
- ROI dimensions and sensor origin/history;
- camera/focuser/mount driver identity and versions;
- safety/weather state where available.

## 12. Durability and restart semantics

The scheduler must persist:

- plan definition and revision;
- current block and phase;
- completed science frames/SER runs;
- active exposure/video run identity;
- last validated focus position;
- last solved target center;
- current ROI state;
- meridian-flip state;
- retry counters;
- suspended/unsafe reason.

After node restart, the scheduler must never blindly repeat an unsafe hardware action. It first re-discovers devices and reconciles physical state, then either resumes, requests operator intervention, or transitions to safe park according to plan policy.

## 13. Planned OAL 1.0 dependencies

Autonomous scheduling depends on the broader OAL 1.0 work that is specified but not yet complete:

- TLS, authentication, roles/scopes and audit log;
- idempotent commands and durable operation recovery;
- replayable WebSocket event stream;
- production guiding, dithering and post-flip recovery;
- weather/safety, dome/roof and power interlocks;
- emergency-stop semantics;
- durable science store/checksums/download recovery;
- driver isolation and public conformance tests.

The first supervised beta may expose scheduler building blocks before all of these autonomous-safety requirements are complete, but it must not present itself as unattended-safe.


## Optional Polar Alignment motion constraint

Polar Alignment does not require a restricted sky region. By default the workflow may use the normal mount-accessible sky, subject to the mount/backend hard limits. Observatories with balconies, roofs, walls, trees or other obstructions can enable `TelescopeProfile.polarMotionLimits`. When enabled, OAL samples each planned RA-slew path in Az/Alt and rejects the motion before it starts if any sampled point leaves the configured allowed region.
