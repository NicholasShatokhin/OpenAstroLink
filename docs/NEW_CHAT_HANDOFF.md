> **v0.2.10.59 update:** Night Vision / Strict Night Mode and display-only black→red mono/raw preview are implemented; OAL 1.0 mobile GUI is planned as a separate Qt Quick/QML client. Fresh Windows/UI HIL for `.59` is pending; `.58-buildfix9` is the last fresh Windows build-qualified base. Read `NIGHT_VISION.md` and `MOBILE_QML_GUI.md`.

# OpenAstroLink / OpenAstroSuite — new-chat handoff

**Authoritative snapshot:** v0.2.10.58-buildfix9  
**Snapshot date:** 2026-09-08  
**Repository rule:** the `repository/` tree in the FULL HANDOFF package is authoritative. Do not reconstruct code from older chat snippets when the repository already contains a newer implementation.

## Mandatory read order

1. `CURRENT_STATUS.md`
2. `START_HERE.md`
3. `docs/CURRENT_CHECKLIST.md`
4. `docs/RELEASE_0.2.10.58.md`
5. `docs/NEW_CHAT_HANDOFF.md`
6. `docs/MOUNT_GEOMETRY.md`
7. `docs/HIGH_RATE_STREAMING.md`
8. `docs/WEBRTC_STREAMING.md`
9. `docs/VALIDATION.md`
10. `docs/ROADMAP_P0_P1_IMPLEMENTATION.md`
11. `PROJECT_MANIFEST.md`

## Do not restart solved history

The direct-MC mount geometry problem is solved to the current evidence level. Coordinate model **v9** is HIL-qualified on the real Sky-Watcher/EQDrive hardware with `Axis1Sign=+1`, `Axis2Sign=-1`. Native serial EQDrive and direct SynScan/EQDrive Wi-Fi use the same Core geometry/GOTO planning. Do not alter geometry, axis meaning, Home/Park conventions, polarity or serial/Wi-Fi parity without new contradictory HIL evidence.

The hidden driver-level 15° GOTO qualification gate was removed in v0.2.10.50. That does not remove the separate operator-controlled sky-safety policy or raw-axis mechanical guard.

## Platform/build state

- Windows x64/MSVC 2022 + Ninja: previous full observatory build confirmed.
- Native Linux x86_64: previous full observatory build confirmed, including Jammy per-user Qt bootstrap.
- Linux/WSL → Raspberry Pi ARM64: previous full node/probe/native-driver cross-build confirmed to 100%.
- ARM64 vendor matrix confirmed: QHYCCD 26.06.04, Canon EDSDK ARM64, ZWO ASI/EAF ARM64, Gemini, Sky-Watcher, EQDrive.
- Pi 5 shares generic `aarch64`; physical Pi 5 qualification pending.
- macOS Intel/Apple Silicon presets/bootstrap implemented; physical Mac build/sign/runtime pending.
- Native drivers are default; INDI stays optional and OFF by default.

Important qualification boundary: the exact v0.2.10.58-buildfix9 Windows x64/MSVC tree is now **fresh-build qualified** with libdatachannel/WebRTC enabled, WebRTC runtime DLL staging, corrected ZWO EAF import linkage, and AMD64 Canon EDSDK import/runtime pairing. OALV `/video` remains fallback and SER stays upstream. **Do not claim camera/WebRTC HIL qualification yet**: buildfix9 still needs a node-registry retest for `oal.canon`/`oal.zwo.eaf`, followed by QHY high-rate, WebRTC fallback and Dual-Live HIL.

## HIL facts from 2026-09-06

Confirmed:

- Free-point Sky Map click → J2000 GOTO physically moves the real mount.
- Sky Map target → Scheduler coordinate transfer works.
- QHY5III462C still FITS and native Live View work.
- Gemini focuser motion/status works; autofocus cancel restores the starting focus position.
- SER opens successfully in AutoStakkert.
- Mount GOTO abort works.

Observed failures that motivated current code:

- Manual direct-Wi-Fi slew could ignore a press or continue moving after release even though node logs showed press/stop commands. The current code adds UDP retry, instant-stop, source validation, running/stopped post-condition checks and fail-safe stop.
- Axis inversion used to be opaque; current UI uses stateful checkboxes.
- A Sky Map target now also populates the Mount-tab J2000 target fields.
- Scene AF was slow and did not robustly converge even near obvious focus. The old global meter could walk 0.05→0.2→0.8→3.2→10 s on a sparse daylight scene. Current scene AF uses bright-tail metering, tiled structure/contrast, a compact near-focus search and repeatability-gated improvement.
- Still auto-exposure oscillated on a sparse bright scene. Measured sequence at gain 200 alternated around ~0.64 s and ~1.43 s; the shorter frame had a useful bright tail while the longer frame clipped it. Current code uses sparse-scene P99.5 control, hysteresis/crossing logic and resets history when gain changes.
- Live preview could race the small in-memory HTTP cache. Current high-rate path replaces per-frame HTTP/PNG/base64 fetch with binary `/video` OALV streaming.

## Camera data plane — current implementation

- Capture and preview FPS are independent; `captureFpsLimit=0` means camera maximum.
- Preview defaults to 60 FPS and is latest-only/droppable.
- QHY and ZWO native Live View paths reuse buffers.
- Raw SER append occurs before preview/debayer/JPEG/UI work.
- JSON events/state remain on `/events`; pixels use binary OALV v1 on `/video`.
- Main and guide cameras have separate resource locks and can stream simultaneously in Dual Live.
- GUI exposes capture/record/preview FPS plus preview/record drop telemetry.
- Desired invariant: preview may drop; recording must not silently drop.

Immediate HIL: >=60 FPS where hardware allows, 5–10 minute zero-record-drop SER, then simultaneous main+guide Dual Live without starving mount/focuser/events.

## Sky Map / framing state

- Offline left-side Sky Map works without Stellarium.
- Catalogue targets and arbitrary visible-sky points can be selected.
- Free-point GOTO is HIL-confirmed.
- Target transfer into Scheduler is HIL-confirmed.
- Selected map coordinates also populate Mount target fields in current code.
- After plate solve, measured camera footprint can be rendered from solved center/scale/dimensions/PA.
- Planned main/guide footprints and Scheduler mosaic grid can be drawn before solve.
- One selected frame can be sent to Stellarium Remote Control's rectangular FOV marker. Standard Stellarium Telescope Control remains mount position/GOTO only.
- Footprint/Stellarium export still needs end-to-end HIL.

## Beta priorities — preserve this order unless a new blocker forces a change

1. Fresh build of v0.2.10.58 with libdatachannel/WebRTC enabled.
2. High-rate main stream + SER zero-drop HIL.
3. Dual Live main+guide HIL.
4. Direct-Wi-Fi manual slew repeat HIL.
5. Scene autofocus repeat HIL; then night star/HFR autofocus.
6. Still auto-exposure repeat HIL.
7. Scheduler end-to-end HIL.
8. Mosaic HIL.
9. Polar Alignment HIL.
10. Full supervised night qualification.

Do not pull Smart Telescope UX, one-button observing or broad unattended-observatory automation into the nearest Beta; those are OAL 1.0 work.

## Working style for the next chat

- Prefer concrete repository edits/patches over abstract advice when a defect is identified.
- Preserve English as canonical documentation and update the Ukrainian mirror at the same time.
- When a milestone changes, update `CURRENT_STATUS`, `START_HERE`, `STATUS`, `NEW_CHAT_HANDOFF`, `VALIDATION`, `ROADMAP`, release notes and `site/` together.
- Distinguish implemented/static-tested from physically build-qualified and HIL-qualified.
- Never treat GitHub as newer than the supplied repository unless the user explicitly says it is synchronized.

### WebRTC v1
`/webrtc` signaling; `oalv-main` + `oalv-guide` DataChannels; OALW v1 fragments carry OALV/JPEG; `/video` is per-role fallback. RTP codecs are not yet implemented.

### 2026-09-07 buildfix8 vendor-runtime ABI note
The exact v0.2.10.58 Windows build now succeeds with WebRTC enabled, and the node/OpenCV Live View starts. Canon EDSDK and ZWO EAF plugin loads exposed `ERROR_BAD_EXE_FORMAT`; buildfix8 validates actual PE Machine architecture, removes stale runtime duplicates and provides `scripts/repair_windows_vendor_runtime.cmd`. Retest these two plugins before resuming QHY/WebRTC throughput HIL.

- Windows buildfix9: Canon EDSDK import/runtime pairing is now path- and PE-validated; `EDSDK_64/Library/EDSDK.lib` is locked to sibling `EDSDK_64/Dll`. Canon/ZWO EAF runtime re-test remains pending.
