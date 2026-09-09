# OpenAstroLink user and astronomer guide

> Current project status: supervised development/Beta qualification. This guide explains the intended operating model and the parts already available; check `CURRENT_CHECKLIST.md` for exact HIL status before relying on a workflow.

## What you run

OpenAstroLink has two important pieces:

- **`openastrolink-node`** — owns cameras, mounts and focusers and executes operations/workflows;
- **OpenAstroSuite** — the reference local/remote graphical client.

The GUI can run on the same computer as the node or connect remotely. Stellarium can connect to the telescope bridge independently.

## Who this is for

- visual/planetary/deep-sky astronomers who want one control layer for mixed hardware;
- users who want a small observatory node on Windows/Linux/Raspberry Pi;
- users who want local and remote control to use the same equipment state;
- experimenters who want direct native hardware support while retaining compatibility options.

## Current hardware direction

Native drivers currently exist for QHY cameras, Canon EOS, ZWO ASI, ZWO EAF, Gemini focusers, Sky-Watcher/EQDrive mounts and the simulated reference driver. Some devices are HIL-qualified and others are implementation/build-qualified only. The website/status checklist distinguishes these states.

## First setup

1. Install/build the node and GUI for your platform.
2. Start `openastrolink-node` and then OpenAstroSuite.
3. Refresh/discover hardware and assign the required camera/mount/focuser.
4. Configure the observing site and optical train.
5. For a mount, verify the correct backend and perform only supervised motion until that backend/profile is qualified on your hardware.
6. Test one short camera exposure before scheduling automation.

## Live View and high-rate capture

The current camera pipeline separates capture/recording from preview. `Capture FPS = MAX` means the camera is not artificially capped by OAL. Preview can be limited independently (for example 15/30/60/MAX).

For planetary recording, SER is written upstream of preview processing. A preview frame may be dropped under load while `recordDropped` should remain zero in a healthy recording run.

## Main and guide cameras

Main and guide roles have independent locks and settings. Dual Live can show both cameras simultaneously when two different physical cameras are assigned. Use this for optical alignment and guiding preparation.

## Mount control

Current mount features include GOTO, tracking, park/home, sync, abort, manual motion, Stellarium integration and the built-in Sky Map. The direct-MC v9 geometry is HIL-qualified for the proven EQDrive/SynScan profile and should not be manually reinterpreted in normal use.

## Autofocus

OpenAstroLink contains both scene-contrast autofocus and astronomical/star focus foundations. Current Scene AF includes exposure metering, tiled contrast evaluation, compact coarse/fine search, repeatability checks and rollback on failure/cancel. Real-scene/night repeatability is still a Beta HIL gate.

## Auto exposure

The current still-image controller uses bright-tail statistics for sparse astronomical scenes and has a LOCK/hysteresis strategy. It is under final HIL qualification; inspect the resulting exposure rather than assuming every automatic result is production-qualified yet.

## Scheduler and observing workflows

The node scheduler can represent DSO FITS/RAW, planetary SER and mosaic blocks. DSO blocks can slew, solve/recenter, autofocus and capture. Planetary blocks have acquisition/ROI/SER foundations. Mosaic and Polar Alignment workflows are implemented but still have remaining HIL gates.

## Data

Keep science output on storage with sufficient free space. SER/FITS/RAW and sidecar/provenance metadata are intended to remain usable outside OpenAstroSuite. For long planetary sessions, monitor measured FPS and dropped-frame counters.

## Remote use

Current pre-1.0 nodes are intended for a trusted LAN or VPN. Do not forward the development HTTP/WebSocket ports directly to the public Internet. Authentication/TLS/RBAC/audit are planned for OAL 1.0.

## Safety

The nearest Beta is **supervised**. Keep physical access or an independent stop path when testing mount motion. Do not treat current scheduler/Polar Alignment code as an unattended roof/weather safety system.

## Troubleshooting order

1. Confirm node starts without missing/wrong-architecture DLL errors.
2. Check the native-driver registry and discovery log.
3. Refresh devices.
4. Test the device interactively before running Scheduler.
5. If Live View is slow, compare capture FPS, preview FPS and drop counters separately.
6. If remote preview fails, determine whether WebRTC is active or `/video` fallback is being used.
7. Preserve logs when reporting a hardware issue; include exact device/firmware/backend and the operation that failed.

## Status references

- `CURRENT_CHECKLIST.md` — most useful current feature/HIL matrix.
- `VALIDATION.md` — acceptance gates.
- `BUILD_PLATFORMS.md` — platform/build details.
- `HIGH_RATE_STREAMING.md` / `WEBRTC_STREAMING.md` — camera transport details.
- `MOUNT_GEOMETRY.md` — direct-MC geometry evidence.
- `SCHEDULER.md` — scheduler model.
