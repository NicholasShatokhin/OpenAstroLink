# WebRTC preview transport — v0.2.10.58

> Current qualification: source/static qualified; fresh native build and physical/network HIL pending.

## Purpose

WebRTC is a low-latency remote preview transport layered downstream of camera acquisition and SER recording. It does not own the camera and must never be allowed to back-pressure scientific acquisition.

## Topology

- `/events`: JSON state/events.
- `/webrtc`: WebSocket signaling for SDP/ICE.
- `oalv-main`: WebRTC DataChannel for Main preview.
- `oalv-guide`: WebRTC DataChannel for Guide preview.
- `/video`: binary OALV v1 WebSocket fallback.

The node is the WebRTC offerer. The GUI is the answerer. Optional STUN/TURN URLs are supplied through `OAL_WEBRTC_ICE_SERVERS` as a comma, semicolon or newline-separated list.

## Payload

v0.2.10.58 deliberately reuses OALV v1/JPEG. The already-encoded OALV packet is split into OALW v1 fragments. OALW has a 24-byte header containing magic/version, role, flags, frame sequence, total OALV size, fragment index and fragment count. Payload fragments are at most 48 KiB.

This is a real WebRTC PeerConnection/DataChannel transport, but not yet an RTP H.264/H.265/AV1 media-track pipeline.

## Drop/backpressure policy

DataChannels are unordered and partially reliable with `maxPacketLifeTime=150 ms`. If a DataChannel has more than 2 MiB buffered, the current preview frame is skipped. The receiver uses latest-sequence semantics independently for Main and Guide, so incomplete older frames may be abandoned when a newer sequence arrives.

These drops are preview drops only. Raw SER append remains upstream of preview processing.

## Fallback

The GUI initially opens `/video` while WebRTC negotiates. `/video` is closed only when both Main and Guide DataChannels are open. If either channel becomes unavailable, `/video` is restored. WebSocket frames for a role whose WebRTC DataChannel is healthy are ignored, which prevents duplicate delivery during transitions.

## Dependency/bootstrap

- Windows: `bootstrap_native_dependencies.ps1` installs `libdatachannel:x64-windows` through per-user vcpkg and the official native preset requires WebRTC.
- Native Linux: `bootstrap_native_dependencies.sh` discovers an installed CMake package or builds pinned libdatachannel v0.24.5 per-user with media/WebSocket helpers disabled.
- Generic/custom builds: `OAS_ENABLE_WEBRTC=ON` enables discovery; `OAS_REQUIRE_WEBRTC=ON` makes absence fatal. Without the library and without the require flag, `/video` remains available.

## HIL acceptance

1. `node/info` reports `webrtc.available=true`.
2. Main and Guide reach `networkTransport=webrtc-datachannel`.
3. One-channel loss restores only that role to `/video` without freezing the other role.
4. Network congestion increases preview drops but does not reduce capture/record rate; `recordDropped=0`.
5. Dual Live remains independent in exposure/gain/rates.
6. 5–10 minute SER recording remains frame-count consistent in AutoStakkert.

## Current qualification — 2026-09-08

Windows x64/MSVC buildfix9 is fresh-build qualified with `LibDataChannel::LibDataChannel`; five vcpkg WebRTC runtime DLLs are staged into the build tree. The node has an earlier `.58` runtime-start/OpenCV-UVC Live View smoke pass. Actual `webrtc-datachannel` negotiation, fallback/reconnect behavior, QHY high-rate throughput, and Main+Guide Dual Live remain HIL gates. See `CURRENT_CHECKLIST.md`.

