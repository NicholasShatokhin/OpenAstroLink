# OpenAstroLink / OpenAstroSuite v0.2.10.57

**Date:** 2026-09-07

## Purpose

This is a **pre-HIL high-rate streaming hardening revision** discovered while qualifying the v0.2.10.56 handoff snapshot. It changes camera-stream/data-plane correctness and performance only. Mount coordinate model v9, Home/Park conventions, axis meaning and direct serial/Wi-Fi geometry are intentionally unchanged.

## High-rate camera streaming hardening

- Removed the stale QHY capability field `maxFps: 30`; QHY native Live now advertises streaming support without inventing a fixed device-independent ceiling.
- OALV v1 `width`/`height` now describe the encoded preview JPEG payload after optional preview downscaling. `sourceWidth`/`sourceHeight` preserve the pre-transport preview dimensions.
- Remote OALV reception decodes each JPEG once. The already-decoded Qt image is converted to the cached OpenCV BGR frame instead of running a second JPEG decode.
- The same native physical camera is rejected if an operator attempts to assign it simultaneously to Main and Guide. Dual Live remains a two-physical-camera feature.
- QHY high-rate steady-state acquisition reuses the buffer sized at `camera.liveStart`; `GetQHYCCDMemLength` is no longer queried on every normal live frame, with a fallback query only if start-time sizing was unavailable.
- The high-rate static regression now checks these invariants in addition to the existing acquisition-first SER, droppable preview and split `/events` + `/video` data plane.

## Qualification boundary

All repository Python/static regression checks pass after this source hardening: **73/73 scripts PASS**, including **43/43** dedicated high-rate/Dual-Live checks. A local Linux configure attempt in the handoff sandbox stops before compilation because Qt >= 6.4 is not installed in that environment; this is a host-dependency limitation, not a successful or failed source build qualification. Windows compilation cannot be physically performed in that Linux sandbox.

Therefore v0.2.10.57 remains **not current-revision build/HIL qualified**. The next gate is a fresh Windows build (and preferably native Linux with dependencies present), followed by QHY high-rate/SER/Dual-Live HIL.

## Next gates

Fresh build → QHY camera-max + Preview 60 measurement → 5–10 minute zero-drop SER + AutoStakkert frame-count check → preview 15/30/60/MAX independence → Main+Guide Dual Live → Wi-Fi manual-slew repeat HIL → autofocus/auto-exposure repeat HIL → Scheduler → Mosaic → Polar Alignment → supervised night.
