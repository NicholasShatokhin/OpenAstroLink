# OpenAstroLink scheduler and autonomous acquisition specification

**Canonical language:** English  
**Target:** staged delivery from the first supervised beta through OAL 1.0  
**Implementation status:** v0.2.10.46 introduces the first real, non-durable DSO executor on the node. `ObservationPlan`/`ObservationBlock` replace `SessionTarget` as the primary model, while the legacy target API remains a compatibility wrapper. The implemented supervised DSO path is `slew -> adaptive solve/recenter -> autofocus -> FITS/RAW capture`, with optional recenter/autofocus cadence between frames. Planetary SER blocks are represented in the plan ABI but their autonomous executor, durable checkpoints/restart, meridian recovery and unattended safety remain planned.

## 1. Goals

The OpenAstroLink scheduler is an observatory workflow engine, not merely a timer or a list of exposures. It must orchestrate target acquisition, focusing, plate solving, centering, guiding/tracking, science capture, recovery and calibration-aware metadata while keeping the node authoritative over hardware.

The same scheduler must support two first-class acquisition families:

1. **Deep-sky imaging (DSO):** FITS/RAW still-image sequences, normally long-exposure, optionally filtered, guided and dithered.
2. **Lunar/planetary/high-speed imaging:** SER video sequences, normally short-exposure/high-frame-rate, with ROI control and continuous target centering.

A plan may mix both families in one night.

## 2. Current implementation boundary

From v0.2.10.46, the node owns a real `ObservationPlan` / `ObservationBlock` state machine. DSO blocks execute asynchronously through the existing `OperationManager` and therefore reuse the same hardware resource locks as interactive commands. The first implemented path is:

```text
PREPARE_BLOCK -> SLEW -> SOLVE/RECENTER -> AUTOFOCUS -> CAPTURE -> [periodic corrections] -> next frame/block
```

The solve/recenter loop plate-solves the actual field, measures pointing error against the block target, performs `Sync + correction slew`, and repeats until the configured arcminute tolerance or attempt limit is reached. Recenter and autofocus may run before the first frame and/or every N completed science frames. Science captures force durable FITS/RAW output when the camera backend supports it.

`SessionTarget` remains accepted by the GUI/API for compatibility and is converted into DSO blocks. Planetary SER data structures are present now so the plan ABI will not require another redesign, but v0.2.10.46 intentionally rejects autonomous `planetary-ser` execution until ROI/centroid/SER-run orchestration is implemented. The scheduler is still **non-durable**: node restart/resume, weather holds, meridian-flip recovery and unattended safety are OAL 1.0 work.

## 3. Plan model

A persisted `ObservationPlan` contains ordered or priority-scheduled `ObservationBlock` objects.

Each block shall contain:

- stable block ID and human-readable name;
- target resolver input and resolved coordinates/ephemeris;
- acquisition mode: `dso-fits` or `planetary-ser`;
- optical train, camera, mount and focuser bindings;
- optional guide train;
- start/end/time-window constraints;
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
