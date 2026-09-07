# Оновлення стану — v0.2.10.55

## v0.2.10.55 — high-rate streaming / Dual Live pass

- ✅ Windows x64, native Linux x86_64 та Raspberry Pi ARM64 builds були qualified до цього source pass; сам v0.2.10.55 ще потребує одного physical build після пакування.
- ✅ Sky Map arbitrary-point GOTO та Sky Map -> Scheduler HIL-підтверджені.
- ✅ QHY still FITS, QHY native Live View, Gemini focuser motion/cancel restore та SER -> AutoStakkert HIL-підтверджені.
- 🟡 Fixes v0.2.10.54 для Wi-Fi manual-slew reliability, scene autofocus convergence та sparse-scene auto-exposure потребують repeat HIL.
- 🆕 Live acquisition більше не має 30 FPS cap; capture і preview limits незалежні, preview droppable, raw SER пишеться до preview work.
- 🆕 Remote Live View використовує binary OALV v1 `/video`, JSON events залишаються на `/events`.
- 🆕 Main і guide cameras можуть одночасно мати незалежні Live View operations з Dual Live telemetry.
- ⏳ >=60 FPS, 5-10 хв SER без record drops та simultaneous main+guide HIL — найближчі streaming gates.
- 🔒 Direct-MC mount geometry v9 frozen і незмінна.

Див. `RELEASE_0.2.10.55.md` та `HIGH_RATE_STREAMING.md`.
