# Status update — v0.2.10.55

## v0.2.10.55 — high-rate streaming / Dual Live pass

- ✅ Windows x64, native Linux x86_64 and Raspberry Pi ARM64 builds were already qualified before this source pass; v0.2.10.55 itself still requires one physical build after packaging.
- ✅ Sky Map arbitrary-point GOTO and Sky Map -> Scheduler are HIL-confirmed.
- ✅ QHY still FITS, QHY native Live View, Gemini focuser motion/cancel restore and SER -> AutoStakkert are HIL-confirmed.
- 🟡 v0.2.10.54 Wi-Fi manual-slew reliability, scene autofocus convergence and sparse-scene auto-exposure fixes require repeat HIL.
- 🆕 Live acquisition is no longer capped at 30 FPS; capture and preview limits are independent, preview is droppable, and raw SER is written before preview work.
- 🆕 Remote Live View uses OALV v1 binary `/video` while JSON events remain on `/events`.
- 🆕 Main and guide cameras can run simultaneous independent Live View operations with Dual Live telemetry.
- ⏳ >=60 FPS, 5-10 minute zero-record-drop SER, and simultaneous main+guide HIL are the immediate camera-streaming gates.
- 🔒 Direct-MC mount geometry v9 remains frozen and unchanged.

See `RELEASE_0.2.10.55.md` and `HIGH_RATE_STREAMING.md`.
