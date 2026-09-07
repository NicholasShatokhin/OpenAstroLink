# High-rate camera streaming and OALV v1

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

The JSON header carries at least role/frame identity, capture timestamp, image dimensions, exposure, gain, measured capture/record/preview rates, preview/record drop counters, requested limits, preview encoding parameters and transport name.

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
