# High-rate camera streaming and OALV v1

> Current synchronized snapshot: **v0.2.10.58 (2026-09-07)**. See `STATUS.md` and `RELEASE_0.2.10.58.md` for current qualification boundaries.


## Design goal

OpenAstroLink must be able to record every frame delivered by a planetary camera while the operator still sees a responsive Live View. Preview is expendable; raw acquisition and SER recording are not. Main and guide cameras must also be able to stream simultaneously for optical-axis alignment and guiding setup.

## Pipeline

```text
native camera SDK
      |
      v
acquired raw frame
      |
      +--> SER writer (every acquired frame, raw)
      |
      +--> analysis sampler (rate-limited when appropriate)
      |
      +--> latest-only preview worker
                |
                +--> optional debayer / display stretch / resize
                +--> local Qt image
                +--> remote JPEG -> OALV /video
```

`captureFpsLimit` and `previewFpsLimit` are intentionally independent. Zero means unlimited by OAL. The physical camera, USB controller, CPU, memory bandwidth and storage device remain the final limits.

## OALV v1 packet

```text
0..3   "OALV"
4      version = 1
5      role: 0 main, 1 guide
6..9   uint32 big-endian JSON header length
10..   JSON header
...    JPEG payload
```

The JSON header carries at least role/frame identity, capture timestamp, image dimensions, exposure, gain, measured capture/record/preview rates, preview/record drop counters, requested limits, preview encoding parameters and transport name. In v0.2.10.57, `width`/`height` are explicitly the encoded JPEG payload dimensions after preview-only scaling; `sourceWidth`/`sourceHeight` preserve the pre-transport preview dimensions.

## WebSocket routing

The configured OAL WebSocket port is shared, but clients use different URL paths:

- `ws://host:port/events` for text JSON events;
- `ws://host:port/video` for OALV binary preview packets.

Unknown/legacy WebSocket paths are treated as event clients for compatibility.

## Backpressure policy

The preview path is latest-only. At most one preview conversion/encode per camera role should be in flight. If it is still busy when a new raw frame arrives, only that preview opportunity is dropped. Raw/SER acquisition continues. The WebSocket sender also skips a slow client while its queued outbound bytes exceed the bounded threshold.

SER writing is synchronous in the current Beta implementation so a storage device that cannot sustain the raw data rate can still cap acquisition. Qualification therefore reports capture/record FPS and requires `recordDropped = 0`; a bounded asynchronous recording ring is a possible OAL 1.0 optimization if HIL shows storage stalls.

## Main + guide

Main and guide Live View operations own separate locks. Dual Live is supported when they map to different physical cameras. The UI must not imply that one physical camera can be acquired by two operations simultaneously.

## Science integrity

Preview transformations are display-only. Debayer, JPEG, scaling and 8-bit display conversion must not alter FITS/RAW science files or raw SER payloads.



## WebRTC preview transport (v0.2.10.58)

When `libdatachannel` is available the same post-recording OALV/JPEG preview packet can use a WebRTC data plane. `/webrtc` is a Qt WebSocket signaling endpoint. The node is the offerer and creates `oalv-main` and `oalv-guide` DataChannels; the remote GUI answers and reassembles OALW v1 fragments. Channels are unordered and partially reliable (`maxPacketLifeTime=150 ms`).

OALW v1 uses a fixed 24-byte header and 48 KiB payload fragments. The receiver keeps latest-sequence state independently for Main and Guide, so an incomplete old preview frame may be discarded as soon as a newer sequence arrives. A 2 MiB sender buffered-amount threshold drops preview work instead of allowing network congestion to back-pressure acquisition.

`/video` remains the binary OALV v1 WebSocket fallback. It is closed only when both role DataChannels are ready; if one role loses WebRTC, `/video` returns and only that role consumes its fallback frames. `OAL_WEBRTC_ICE_SERVERS` supplies optional STUN/TURN URLs.

This is WebRTC **transport v1**, not an RTP codec pipeline: payload remains OALV v1/JPEG. H.264/H.265/AV1 media tracks are a later optimization after transport HIL.

## v0.2.10.57 pre-HIL hardening

Before physical high-rate qualification, the v0.2.10.56 snapshot audit found and fixed four data-plane risks:

- QHY no longer publishes a fictitious fixed `maxFps:30`; camera maximum is determined by exposure, ROI/binning, USB path and hardware.
- OALV payload dimensions are no longer ambiguous when `previewMaxWidth` downsizes a preview.
- The remote controller performs one JPEG decode per OALV frame rather than decoding the same packet independently through Qt and OpenCV.
- One native physical camera cannot be opened as both Main and Guide; Dual Live requires different native device identities.

QHY also reuses the live buffer sized at `camera.liveStart` and only asks the SDK for memory length as a fallback when that start-time size was unavailable.

## v0.2.10.57 qualification boundary

Implementation is present, but current-revision HIL is still pending. The first acceptance run is: camera-max acquisition + 60 FPS preview, then 5–10 minute raw SER with zero record drops, then simultaneous main+guide Dual Live. Preview backpressure may drop preview frames; it must not silently throttle or drop recording frames.

## Current qualification — 2026-09-08

The high-rate/WebRTC source path is now fresh Windows build-qualified. Remaining qualification is physical: QHY `Capture FPS=MAX`/Preview-60 throughput, 5–10 minute zero-record-drop SER, Preview 15/30/60/MAX independence, and simultaneous Main+Guide streams. See `CURRENT_CHECKLIST.md` and `VALIDATION.md`.

