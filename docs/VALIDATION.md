# Validation plan — v0.2.10.58-buildfix9

**Snapshot:** 2026-09-08  
**Master checklist:** `CURRENT_CHECKLIST.md`

## Gate 0 — current Windows build qualification — ✅ CLOSED

The exact current Windows x64/MSVC tree has a fresh successful configure and build with WebRTC enabled.

Confirmed by the 2026-09-08 host log:

- `LibDataChannel::LibDataChannel` found and enabled.
- Five WebRTC runtime DLLs staged from the per-user vcpkg runtime.
- QHY and ZWO ASI same-architecture vendor runtime staging.
- ZWO EAF static archive rejected/replaced by `EAF_focuser.lib`; same-architecture runtime staged.
- Canon EDSDK `EDSDK_64/Library/EDSDK.lib` paired with `EDSDK_64/Dll` and two AMD64 runtime DLLs staged.
- Configure/generate completed.
- Build reached `Linking CXX executable OpenAstroSuite.exe` with no compile/link failure.

Optional `WrapVulkanHeaders`/Vulkan SDK discovery remains informational and is not required by the current GUI/WebRTC feature set.

## Gate 1 — native vendor plugin runtime — 🟡 IMMEDIATE

The next node startup must verify the buildfix9 runtime pair corrections:

1. Start `openastrolink-node.exe` from the buildfix9 tree.
2. Require both `oal.canon` and `oal.zwo.eaf` in the native driver registry.
3. No `ERROR_BAD_EXE_FORMAT`, `%1 is not a valid Win32 application`, or missing vendor DLL warning.
4. If both load, mark Canon/ZWO EAF Windows ABI/runtime regression closed.

The previous `.58` runtime smoke already confirmed that the node can start after WebRTC runtime staging and that OpenCV/UVC Live View works. The Canon/ZWO EAF retest specifically needs the corrected buildfix9 vendor-runtime pair.

## Gate 2 — WebRTC transport HIL

1. Confirm `/api/v1/node/info` reports WebRTC available.
2. Start remote Live View and require telemetry to report `webrtc-datachannel` rather than only `websocket` fallback.
3. Break/restore one role's peer/channel and verify only that role consumes `/video` fallback.
4. Repeat on LAN without an ICE server; repeat routed/NAT path with configured STUN/TURN when appropriate.
5. Under network loss/congestion, preview drops may increase but capture/record FPS must remain stable and `recordDropped=0`.
6. Repeat with simultaneous Main + Guide streams.

## Gate 3 — high-rate QHY / SER

1. QHY5III462C, short exposure, `Capture FPS=MAX`, `Preview=60`.
2. Record `captureFps`, `recordFps`, `previewFps`, `previewDropped`, and `recordDropped`.
3. Verify measured capture rate can exceed 60 FPS when exposure/ROI/USB hardware permits.
4. Record SER for 5–10 minutes; require `recordDropped=0`.
5. Open the SER in AutoStakkert and reconcile frame count with OAL telemetry/sidecar.
6. Repeat Preview 15/30/60/MAX while recording; capture/record rate must not materially fall because of GUI preview.
7. Generate slow-preview load and verify preview drops grow before recording drops.
8. Stop/cancel must finalize SER cleanly and release camera locks.

## Gate 4 — Dual Live

1. Connect two distinct physical cameras as Main and Guide.
2. Start both streams simultaneously.
3. Independently change exposure, gain, capture limit, and preview limit.
4. Verify each camera retains its own resource lock and telemetry.
5. Verify WebRTC partial failure/fallback is per role and does not duplicate the healthy role.
6. Verify mount/focuser commands and `/events` remain responsive under both streams.

## Gate 5 — retained mount/focus/exposure HIL

- 30–50 rapid direct-Wi-Fi manual press/release/direction changes: no ignored press and no motion after release.
- Axis inversion checkboxes: state shown in GUI must match physical direction behavior.
- Sky Map target must populate Mount-tab J2000 fields.
- Scene AF near true focus, then from deliberately offset but in-range focus; require repeatable verified improvement and rollback safety.
- Sparse daylight still auto-exposure on the target that previously oscillated near ~0.64/1.43 s; require stable LOCK.
- Night star-field/HFR autofocus.

## Gate 6 — workflow HIL

1. Scheduler DSO block end to end.
2. ASTAP solve + sync/recenter closed loop; target acceptance ~1–2 arcmin unless the selected test defines a tighter tolerance.
3. Planetary block: acquisition → detect → center → AF → ROI → SER; verify ROI tracking/provenance.
4. Mosaic 2×2 with footprint overlay and per-tile solve/recenter/capture.
5. Polar Alignment real-sky workflow, convergence and final solve.
6. Discovery/hotplug/replug regression.
7. Full supervised night session.

## Regression invariants

- Mount geometry v9 remains byte-for-byte/equation-equivalent unless new HIL evidence explicitly reopens it.
- Qualified direct-MC signs remain `Axis1Sign=+1`, `Axis2Sign=-1`.
- Science FITS/RAW and SER remain raw/undebayered where defined by the existing capture path.
- Preview is allowed to drop; recording is not.
- WebRTC congestion must not block camera acquisition/SER.
- One native physical camera cannot be assigned to Main and Guide simultaneously.
- Native drivers remain the default; INDI remains optional/OFF by default.

## Static evidence retained for the `.58` branch

- General repository Python/static matrix: **75/75 PASS**.
- WebRTC transport checks: **43/43 PASS**.
- High-rate/Dual-Live checks: **43/43 PASS**.
- Vendor runtime PE-machine and Canon pairing targeted guards: PASS.

Static checks do not substitute for the HIL gates above.
