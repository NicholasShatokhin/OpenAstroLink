# Current status — OpenAstroLink / OpenAstroSuite v0.2.10.58-buildfix9

**Snapshot date:** 2026-09-08  
**Core/source version:** `0.2.10.58`  
**Windows qualification checkpoint:** `buildfix9`  
**Master checklist:** `docs/CURRENT_CHECKLIST.md`

## Current headline

The exact current Windows x64/MSVC observatory tree now configures and builds successfully with WebRTC enabled through `libdatachannel 0.24.5`. CMake stages the WebRTC runtime DLL set, repairs ZWO EAF linkage to the DLL import library, validates vendor PE architecture, and pairs the AMD64 Canon `EDSDK_64/Library/EDSDK.lib` with the matching `EDSDK_64/Dll` runtime. The final build reaches `Linking CXX executable OpenAstroSuite.exe` without compiler/link errors.

This closes the old **clean Windows build** Beta blocker. The next runtime gate is to restart the node from buildfix9 and require both `oal.canon` and `oal.zwo.eaf` to load into the native registry without Win32 error 193. After that, qualification moves to WebRTC/QHY high-rate/Dual-Live HIL.

## Physically/build confirmed

- ✅ Fresh Windows x64/MSVC configure on 2026-09-08 with `OAL WebRTC preview transport: enabled (LibDataChannel::LibDataChannel)`.
- ✅ WebRTC runtime staging: five non-CRT DLLs from the per-user vcpkg runtime directory are staged into the build.
- ✅ QHY and ZWO ASI same-architecture runtime staging.
- ✅ ZWO EAF CMake recovery from `EAF_focuser-static.lib` to the correct `EAF_focuser.lib` DLL import library; same-architecture runtime staging.
- ✅ Canon EDSDK pair locking: AMD64 `EDSDK_64/Library/EDSDK.lib` is paired with `EDSDK_64/Dll`, replacing the stale x86 `EDSDK/Dll` runtime selection; two AMD64 Canon DLLs are staged.
- ✅ Fresh buildfix9 Windows build reaches the final `OpenAstroSuite.exe` link successfully.
- ✅ Earlier `.58` runtime smoke: node starts after WebRTC DLL staging and OpenCV/UVC Live View works at `Capture FPS=MAX`, `Preview=60`.
- ✅ Direct-MC mount coordinate model **v9** remains HIL-qualified on the real mount with `Axis1Sign=+1`, `Axis2Sign=-1`.
- ✅ Free-point Sky Map GOTO physically moves the real mount.
- ✅ Sky Map target → Scheduler transfer works.
- ✅ QHY5III462C still FITS and native Live View are historically HIL-positive.
- ✅ Gemini focuser basic motion/status and AF cancel/start-position restore are HIL-positive.
- ✅ SER opens in AutoStakkert.
- ✅ Mount GOTO abort is HIL-positive.

## Immediate pending runtime/HIL

1. Restart buildfix9 node and require `oal.canon` + `oal.zwo.eaf` in the native registry with no `ERROR_BAD_EXE_FORMAT` / `%1 is not a valid Win32 application`.
2. Confirm Live View telemetry reports actual `webrtc-datachannel`, not only `/video` fallback.
3. QHY short-exposure HIL at `Capture FPS=MAX`, `Preview=60`; measure capture/record/preview FPS and drop counters.
4. 5–10 minute SER with `recordDropped=0`, then reconcile AutoStakkert frame count with telemetry/sidecar.
5. Preview 15/30/60/MAX while recording; preview policy must not reduce acquisition/recording throughput.
6. Main + Guide Dual Live on two physical cameras with independent exposure/gain/rates and per-role WebRTC fallback.
7. 30–50 rapid direct-Wi-Fi manual slew press/release/direction changes; no ignored press and no continued motion after release.
8. Repeat Scene AF and sparse-scene still auto-exposure HIL; then night star/HFR AF.
9. Scheduler → ASTAP/recenter → planetary → mosaic → Polar Alignment → full supervised night.

## High-rate/WebRTC architecture

- `captureFpsLimit=0` means camera maximum; preview has an independent limit, default 60 FPS.
- Raw SER is appended before preview/debayer/JPEG/network work.
- `/events` carries JSON control/state.
- `/video` carries binary OALV v1 fallback preview.
- `/webrtc` carries signaling; `oalv-main` and `oalv-guide` are independent unordered, partially reliable DataChannels.
- OALW v1 fragments carry the existing OALV/JPEG packet. RTP H.264/H.265/AV1 media tracks are not implemented yet.
- Slow networking is allowed to drop preview; it must not silently drop SER recording.

Static branch evidence retained: **75/75 general scripts PASS**, **43/43 WebRTC PASS**, **43/43 high-rate/Dual-Live PASS**.

## Implemented but awaiting current HIL

- 🟡 Direct SynScan/EQDrive Wi-Fi manual-slew retry + instant-stop + post-condition verification + fail-safe stop.
- 🟡 Stateful mount-axis inversion checkboxes.
- 🟡 Sky Map target synchronization into Mount tab.
- 🟡 Scene AF bright-tail metering + tiled contrast + compact search + repeatability gate.
- 🟡 Sparse-scene still auto-exposure P99.5 controller + hysteresis/crossing + gain-change reset.
- 🟡 Camera footprints, mosaic overlay and Stellarium FOV export.
- 🟡 Actual WebRTC transport/fallback, high-rate QHY, zero-drop SER and simultaneous Main+Guide Dual Live.

## Frozen mount facts

- Coordinate model: **v9**.
- Qualified signs: `Axis1Sign=+1`, `Axis2Sign=-1`.
- Native serial EQDrive and direct UDP/11880 SynScan/EQDrive Wi-Fi share the same Core geometry/GOTO planning.
- **Do not change** v9 geometry, axis meaning, Home/Park convention, serial/Wi-Fi parity or polarity without new contradictory HIL evidence.
- The hidden `maxNativeGotoDeg`/15° qualification gate is removed. Operator sky-safety and explicit raw-axis mechanical guards remain separate mechanisms.

## Beta boundary

The old Windows compilation blocker is closed. The supervised `0.3 Beta` gate is now predominantly **runtime/HIL qualification**, not missing core implementation. See `docs/CURRENT_CHECKLIST.md` for the complete master checklist and `docs/ROADMAP_P0_P1_IMPLEMENTATION.md` for the execution order.

Smart Telescope UX remains OAL 1.0 scope.
