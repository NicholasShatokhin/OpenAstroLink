# План валідації — v0.2.10.58-buildfix9

**Snapshot:** 2026-09-09  
**Master checklist:** `CURRENT_CHECKLIST.md`

## Gate 0 — current Windows build qualification — ✅ ЗАКРИТО

Точне поточне Windows x64/MSVC tree має fresh successful configure і build з увімкненим WebRTC.

Підтверджено host log 2026-09-08:

- знайдено та ввімкнено `LibDataChannel::LibDataChannel`;
- staged п'ять WebRTC runtime DLL з per-user vcpkg runtime;
- QHY і ZWO ASI same-architecture vendor runtime staging;
- ZWO EAF static archive відхилено/замінено на `EAF_focuser.lib`, staged same-architecture runtime;
- Canon EDSDK `EDSDK_64/Library/EDSDK.lib` спарена з `EDSDK_64/Dll`, staged дві AMD64 runtime DLL;
- configure/generate успішні;
- build дійшов до `Linking CXX executable OpenAstroSuite.exe` без compile/link failure.

`WrapVulkanHeaders`/Vulkan SDK лишається optional informational discovery і не потрібен поточному GUI/WebRTC feature set.

## Gate 1 — native vendor plugin runtime — 🟡 НАЙБЛИЖЧИЙ

Наступний startup node має перевірити buildfix9 runtime pair corrections:

1. Запустити `openastrolink-node.exe` з buildfix9 tree.
2. Вимагати `oal.canon` і `oal.zwo.eaf` у native driver registry.
3. Не повинно бути `ERROR_BAD_EXE_FORMAT`, `%1 is not a valid Win32 application` або missing vendor DLL warning.
4. Якщо обидва load — Canon/ZWO EAF Windows ABI/runtime regression закритий.

Попередній `.58` runtime smoke вже підтвердив старт node після WebRTC runtime staging та OpenCV/UVC Live View. Canon/ZWO EAF retest потрібен саме з виправленою buildfix9 vendor-runtime pair.

## Gate 2 — WebRTC transport HIL

1. `/api/v1/node/info` має показувати WebRTC available.
2. Remote Live View telemetry має показати `webrtc-datachannel`, а не лише `websocket` fallback.
3. Зламати/відновити peer/channel однієї ролі й перевірити, що тільки ця роль переходить на `/video` fallback.
4. LAN без ICE server; routed/NAT path зі STUN/TURN, коли доречно.
5. При network loss/congestion preview drops можуть рости, але capture/record FPS мають лишатися стабільними і `recordDropped=0`.
6. Повторити з simultaneous Main + Guide.

## Gate 3 — high-rate QHY / SER

1. QHY5III462C, short exposure, `Capture FPS=MAX`, `Preview=60`.
2. Записати `captureFps`, `recordFps`, `previewFps`, `previewDropped`, `recordDropped`.
3. Capture rate має перевищувати 60 FPS, якщо exposure/ROI/USB hardware це дозволяє.
4. SER 5–10 хв; `recordDropped=0`.
5. Відкрити SER в AutoStakkert і звірити frame count з OAL telemetry/sidecar.
6. Preview 15/30/60/MAX під час recording; capture/record rate не має матеріально падати через GUI preview.
7. Створити slow-preview load і перевірити, що спочатку ростуть preview drops, а не recording drops.
8. Stop/cancel має cleanly finalize SER і звільнити camera locks.

## Gate 4 — Dual Live

1. Підключити дві різні physical cameras як Main і Guide.
2. Запустити обидва streams одночасно.
3. Незалежно змінювати exposure, gain, capture limit, preview limit.
4. Перевірити окремі resource locks і telemetry.
5. WebRTC partial failure/fallback має бути per role, без duplicate healthy stream.
6. Mount/focuser commands і `/events` мають залишатися responsive.

## Gate 5 — retained mount/focus/exposure HIL

- 30–50 rapid direct-Wi-Fi manual press/release/direction changes: без ignored press і без motion після release.
- Axis inversion checkboxes: GUI state має відповідати physical direction behavior.
- Sky Map target має populate Mount-tab J2000 fields.
- Scene AF near true focus, потім deliberately offset but in-range; вимагати repeatable verified improvement і rollback safety.
- Sparse daylight still auto-exposure на target, що раніше oscillated near ~0.64/1.43 s; вимагати stable LOCK.
- Night star-field/HFR autofocus.

## Gate 6 — workflow HIL

1. Scheduler DSO block end to end.
2. ASTAP solve + sync/recenter closed loop; acceptance ~1–2 arcmin, якщо тест не задає tighter tolerance.
3. Planetary block: acquisition → detect → center → AF → ROI → SER; ROI tracking/provenance.
4. Mosaic 2×2 з footprint overlay і per-tile solve/recenter/capture.
5. Polar Alignment real-sky workflow, convergence і final solve.
6. Discovery/hotplug/replug regression.
7. Full supervised night session.

## Regression invariants

- Mount geometry v9 не змінювати без нових HIL-доказів.
- Qualified direct-MC signs: `Axis1Sign=+1`, `Axis2Sign=-1`.
- Science FITS/RAW і SER лишаються raw/undebayered там, де це визначено capture path.
- Preview може drop-атися; recording — ні.
- WebRTC congestion не має блокувати camera acquisition/SER.
- Один native physical camera не можна призначити одночасно Main і Guide.
- Native drivers — default; INDI — optional/OFF by default.

## Static evidence `.58`

- General repository Python/static matrix: **75/75 PASS**.
- WebRTC transport: **43/43 PASS**.
- High-rate/Dual-Live: **43/43 PASS**.
- Vendor runtime PE-machine та Canon pairing targeted guards: PASS.

Static checks не замінюють HIL gates вище.

## v0.2.10.59 — acceptance Night Vision

- Fresh Windows MSVC build з Qt 6.10.
- Normal → Night Vision → Strict Night Mode → Normal через `Ctrl+Shift+N`.
- Restart зі збереженим Strict: core chooser має відкриватися вже в нічній палітрі.
- Справжній кольоровий UVC preview має лишатися кольоровим навіть при увімкненій black→red опції.
- Mono/raw, Debayer OFF + black→red має показувати чорно-червону шкалу.
- Debayer ON має вимикати black→red transform.
- Strict має переводити Sky Map, histogram, reticle, astrometry/star overlays та status accents у red-only.
- FITS/RAW/SER та OALV/WebRTC payload не повинні залежати від GUI palette.
