# Validation plan — v0.2.10.55

## Immediate high-rate / Dual Live HIL gates

1. Main QHY/ASI Live View at short exposure: verify measured capture rate can exceed 60 FPS when hardware permits.
2. Set preview to 15, 30, 60 and MAX while recording SER; capture/record FPS must remain materially unchanged and `recordDropped` must remain 0.
3. Run a 5-10 minute high-rate SER session and open the result in AutoStakkert; frame count must match OAL recording telemetry/sidecar.
4. Run main + guide Live View simultaneously; independently alter exposure/gain/preview rate on each and verify the other stream is unaffected.
5. Generate slow-preview load (large preview width/quality or remote client stall) and verify preview drops grow without record drops.
6. Verify mount/focuser commands and `/events` state updates remain responsive while `/video` is busy.
7. Verify Stop/Cancel finalizes SER cleanly and both camera resource locks are released.

## Repeat HIL retained from v0.2.10.54

- 30-50 rapid direct-Wi-Fi manual press/release direction changes; no ignored press and no continued motion after release.
- Scene autofocus near true focus, then from a deliberately offset but in-range position; require repeatable verified improvement.
- Sparse daylight auto-exposure on the same target that previously oscillated around ~0.64/1.43 s; require a stable lock.
- Night star-field/HFR autofocus remains pending.

## Regression invariants

- Mount geometry v9 files/equations are not changed by camera-stream work.
- Science FITS/RAW and SER remain raw/undebayered.
- Preview is allowed to drop; recording is not.
- INDI remains opt-in.
