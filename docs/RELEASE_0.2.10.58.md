# OpenAstroLink v0.2.10.58 — WebRTC preview transport checkpoint

v0.2.10.58 adds an optional native WebRTC preview transport while preserving the v0.2.10.57 high-rate acquisition and recording architecture.

## WebRTC

- `libdatachannel` provides the peer connection and DataChannel implementation.
- `/webrtc` is the signaling WebSocket; `/events` remains JSON control/state and `/video` remains the OALV v1 WebSocket fallback.
- `oalv-main` and `oalv-guide` are independent unordered, partially reliable DataChannels.
- OALW v1 fragments carry the existing OALV v1/JPEG preview packet. This release does **not** claim an RTP H.264/H.265/AV1 media track yet.
- Preview uses latest-frame semantics and a 2 MiB DataChannel backpressure guard. Slow networking may drop preview frames; it must not stall acquisition or SER recording.
- `OAL_WEBRTC_ICE_SERVERS` can provide comma/semicolon/newline-separated STUN/TURN URLs.
- Official Windows native presets bootstrap and require `libdatachannel`; Linux native bootstrap can build the pinned v0.24.5 dependency per-user.

## High-rate camera streaming

SER/raw recording still occurs upstream of preview processing. JPEG is encoded once in the droppable preview worker and the same OALV packet can be delivered over WebRTC or the `/video` fallback.

## Qualification boundary

Source/static qualification is separate from hardware qualification. The **fresh Windows v0.2.10.58-buildfix9 configure/build is now confirmed**. QHY high-rate HIL, Dual Live HIL, network fallback HIL, 5–10 minute zero-drop SER validation, and Canon/ZWO-EAF node-registry runtime retest remain pending.

Mount direct-MC geometry v9 is frozen and unchanged (`Axis1Sign=+1`, `Axis2Sign=-1`).

## Windows bootstrap buildfix2 (2026-09-07)

The first-run vcpkg bootstrap emitted status text on PowerShell's success pipeline. Because `Ensure-Vcpkg` returned its root path through the same pipeline, the caller could receive bootstrap status text plus the path and then attempt to execute that concatenated string. The bootstrap now routes native stdout to the host, validates the physical `vcpkg.exe` with `Test-Path`, and invokes the executable through an explicit `Join-Path` result. Existing per-user vcpkg clones do not need to be deleted.

## Windows native dependency bridge buildfix3 (2026-09-07)

A successful native dependency bootstrap could still be followed by a raw
`cmake --preset my-windows-observatory` failure saying that libdatachannel was
not found. The bootstrap correctly wrote `LibDataChannel_DIR` and the vcpkg
installation prefix to `.oal/native-deps-windows-x64.json`, but only
`scripts/build_windows.ps1` consumed that record. A direct CMake preset kept the
user preset's Qt-only `CMAKE_PREFIX_PATH`.

Native Windows CMake configure now imports the dependency record automatically
when it exists and the build is not cross-compiling. Explicit preset/`-D`
values continue to win; empty or `*-NOTFOUND` cache entries are filled from the
record. The recorded vcpkg prefix is merged into `CMAKE_PREFIX_PATH` so
libdatachannel can resolve its OpenSSL/libjuice/usrsctp package dependencies.
Windows backslash paths from the JSON record are normalized to CMake-style
forward slashes before use.

This makes these two configure paths equivalent after a successful bootstrap:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

and, from an x64 MSVC developer environment:

```powershell
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
```

## Windows Qt 6.10 compile buildfix4 (2026-09-07)

The first fresh Windows build with WebRTC enabled reached native compilation and exposed a Qt 6.10 source-compatibility error in the direct SynScan/EQDrive Wi-Fi stop-confirmation diagnostic. `QString::arg(axis, last)` mixed an integer and a `QString` in the variadic overload, which Qt 6.10/MSVC rejects. It is now expressed as the equivalent chained form `QString::arg(axis).arg(last)`.

This is a diagnostic-string-only compile fix. It does **not** change mount geometry v9, axis polarity, Home/Park conventions, manual-slew commands, stop/retry semantics or serial/Wi-Fi parity.

## 2026-09-07 Windows compile progression — buildfix5

- ✅ WebRTC dependency discovery/configure is now HIL-build-confirmed on Windows/MSVC: CMake finds `LibDataChannel::LibDataChannel` and generates the observatory build.
- ✅ buildfix4 removed the Qt 6.10 mixed `QString::arg` compile blocker; the subsequent build progressed to 63/85.
- 🛠️ buildfix5 fixes two blockers exposed next: WebRTC OALW fragmentation now normalizes `QByteArray::size()` (`qsizetype`) before `std::min`, and Windows ZWO EAF linkage no longer accepts `EAF_focuser-static.lib` for the native driver.
- 🛠️ ZWO EAF bootstrap now prefers the DLL import library; CMake can recover an older cached `*-static.lib` selection by locating `EAF_focuser.lib`/`EAFFocuser.lib` and pairing its runtime DLL directory when available.
- 🔒 Mount geometry v9, axis polarity, Home/Park and SynScan/EQDrive motion semantics are unchanged.
- 🟡 A fresh Windows compile after buildfix5 is still required before v0.2.10.58 is build-qualified.

## Windows vendor runtime ABI buildfix8 (2026-09-07)

A successful WebRTC-enabled Windows build and node runtime smoke exposed `ERROR_BAD_EXE_FORMAT` while loading the native Canon EDSDK and ZWO EAF plugins. buildfix8 hardens Windows vendor staging by parsing each DLL's PE COFF Machine field instead of trusting SDK folder names, accepts only the target architecture, removes stale same-name copies from both the executable and driver directories, and refuses configuration when a runtime directory contains only incompatible DLLs. The bootstrap now also selects AMD64 QHY/ZWO/Canon runtime DLLs by PE metadata. `scripts/repair_windows_vendor_runtime.cmd` can repair an already-built Windows tree in place. Mount geometry v9 is unchanged.

### Windows buildfix9 (2026-09-08)
Canon EDSDK Windows staging now locks the selected import library to its sibling runtime directory. An x64 `EDSDK_64/Library/EDSDK.lib` therefore cannot be paired with the parallel x86 `EDSDK/Dll` tree. The runtime PE machine is validated and stale/mismatched cache state is corrected automatically.

## Windows buildfix9 qualification (2026-09-08)

The exact current Windows x64/MSVC tree now configures and builds successfully with WebRTC enabled. The configure log confirms `LibDataChannel::LibDataChannel`, stages five WebRTC runtime DLLs, stages same-architecture QHY/ZWO runtimes, repairs ZWO EAF to the DLL import library, and pairs `EDSDK_64/Library/EDSDK.lib` with the sibling AMD64 `EDSDK_64/Dll` runtime. The build reaches `Linking CXX executable OpenAstroSuite.exe` without compile/link errors.

This closes the clean-Windows-build gate for the `.58` branch. Runtime qualification is still pending for the corrected Canon and ZWO EAF plugin loads; the next node startup must show `oal.canon` and `oal.zwo.eaf` in the native registry without Win32 error 193. WebRTC/QHY high-rate/Dual-Live HIL remains pending. The full current gate list lives in `CURRENT_CHECKLIST.md`.


## Public website/documentation update (2026-09-09)

The repository site source now publishes a clear explanation of what OpenAstroLink is, why it was created, and why the project needed a different architectural combination than existing astronomy stacks. It also publishes a public manifesto and separate documentation entry points for astronomers/users, OAL core contributors, third-party applications, and native driver authors. DNS/HTTPS for `openastro.link` and `www.openastro.link` are live; the remaining website task is deploying this current source over the older live snapshot.
