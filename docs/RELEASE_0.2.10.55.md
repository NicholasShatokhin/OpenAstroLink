# OpenAstroLink / OpenAstroSuite v0.2.10.55

## High-rate camera streaming and Dual Live

This release builds on the 2026-09-06 HIL hardening in v0.2.10.54 and removes the old preview architecture that coupled Live View responsiveness to HTTP PNG/base64 frame fetches. Mount coordinate geometry v9 is unchanged.

### High-rate acquisition

- The old 30 FPS Live View clamp is removed.
- `captureFpsLimit = 0` means consume native camera frames at the maximum rate delivered by the SDK; explicit limits up to 1000 FPS are accepted.
- `previewFpsLimit` is independent and defaults to 60 FPS. Preview work is latest-only/droppable and cannot intentionally throttle acquisition.
- Native Live View defaults to 8-bit transfer for high-rate operation; 16-bit remains selectable.
- QHY and ZWO ASI native drivers reuse their live buffers instead of allocating a new large vector on every SDK frame.
- Native frame ownership avoids an extra OpenCV full-frame clone after the ABI-v2 host copy.

### Recording-first pipeline

The acquisition path is now:

`camera SDK -> acquired raw frame -> SER append -> optional tracking analysis -> droppable preview processing -> UI/network preview`

SER therefore receives every frame that OAL successfully acquires before preview conversion, scaling, debayering or JPEG encoding. Preview drops are expected under load; `recordDropped` is exposed separately and must remain zero in qualification runs. Physical USB/disk bandwidth can still cap the achievable acquisition/recording rate.

Planetary SER tracking analysis is decimated to approximately 20 Hz instead of running expensive detection on every high-rate frame.

### OALV v1 binary preview transport

Remote Live View no longer fetches each live frame through `HTTP -> PNG -> base64 -> JSON`. The WebSocket port now has two logical paths:

- `/events`: JSON state, operations and normal OAL events;
- `/video`: binary OALV v1 preview packets.

An OALV v1 packet is:

1. ASCII magic `OALV`;
2. one-byte version (`1`);
3. one-byte camera role (`0=main`, `1=guide`);
4. 32-bit big-endian JSON-header length;
5. compact JSON header (frame id, timestamp, dimensions, exposure, gain, measured FPS/drop counters, transport metadata);
6. JPEG preview payload.

JPEG encoding is off the acquisition and WebSocket event threads. Slow preview clients lose old preview frames rather than slowing science acquisition. Still/AF frames retain the existing exact-ID HTTP preview path.

### Main + guide simultaneous Live View

- The REST Live View route accepts camera roles `main` and `guide`.
- Main and guide use independent resource locks (`camera`, `camera.guide`) and can stream simultaneously when they are different physical devices.
- OpenAstroSuite adds Guide Live controls and a Dual Live alignment view with independent exposure, gain, binning, capture limit, preview limit, transfer depth and preview width.
- The UI reports measured capture FPS, record FPS, preview FPS, preview drops and record drops for both camera roles.

### HIL gates for this release

- >=60 FPS main-camera Live View at a short enough exposure when the camera/USB path supports it.
- Confirm preview FPS changes do not change SER recording FPS.
- Run a 5-10 minute high-rate SER test with `recordDropped = 0`.
- Run simultaneous main + guide Live View and verify independent controls.
- Verify high-rate preview remains responsive while mount/focuser operations and JSON event traffic continue.
- Re-run the v0.2.10.54 Wi-Fi manual-slew, scene-autofocus and sparse auto-exposure HIL gates.

## Safety / frozen geometry

Direct-MC mount geometry v9 remains frozen and unchanged. High-rate camera work does not alter mount geometry, Home/Park, pier-side mapping or GOTO coordinate equations.
