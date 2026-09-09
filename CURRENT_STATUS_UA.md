> **v0.2.10.59 Night Vision checkpoint (2026-09-09):** Desktop OpenAstroSuite отримав persistent Normal / Night Vision / Strict Night Mode, `Ctrl+Shift+N` та опціональну чорно-червону display palette для справді монохромного/RAW preview без debayer. Camera/science data не перефарбовуються. Для OAL 1.0 зафіксовано окремий mobile Qt Quick/QML frontend. v0.2.10.59 ще потребує fresh Windows build/UI HIL; останнє fresh Windows build-qualified дерево — v0.2.10.58-buildfix9.

# Поточний стан — OpenAstroLink / OpenAstroSuite v0.2.10.59

**Дата snapshot:** 2026-09-09  
**Core/source version:** `0.2.10.59`  
**Windows qualification checkpoint:** `buildfix9`  
**Master checklist:** `docs/uk/CURRENT_CHECKLIST.md`

## Головний поточний результат

Точне поточне Windows x64/MSVC observatory tree тепер успішно конфігурується і збирається з WebRTC через `libdatachannel 0.24.5`. CMake stage-ить WebRTC runtime DLL, виправляє ZWO EAF linkage на DLL import library, перевіряє PE-архітектуру vendor DLL і жорстко спарює AMD64 Canon `EDSDK_64/Library/EDSDK.lib` з відповідним `EDSDK_64/Dll`. Фінальна збірка доходить до `Linking CXX executable OpenAstroSuite.exe` без compiler/link errors.

Отже старий blocker **clean Windows build** закритий. Найближчий runtime gate — запустити node саме з buildfix9 і вимагати, щоб `oal.canon` та `oal.zwo.eaf` завантажилися в native registry без Win32 error 193. Після цього переходимо до WebRTC/QHY high-rate/Dual-Live HIL.

## Фізично/build підтверджено

- ✅ Fresh Windows x64/MSVC configure 2026-09-08 з `OAL WebRTC preview transport: enabled (LibDataChannel::LibDataChannel)`.
- ✅ WebRTC runtime staging: п'ять non-CRT DLL із per-user vcpkg runtime staged у build.
- ✅ QHY і ZWO ASI same-architecture runtime staging.
- ✅ ZWO EAF CMake recovery з `EAF_focuser-static.lib` на правильну DLL import library `EAF_focuser.lib`; same-architecture runtime staging.
- ✅ Canon EDSDK pair locking: AMD64 `EDSDK_64/Library/EDSDK.lib` спарена з `EDSDK_64/Dll`, stale x86 `EDSDK/Dll` замінений; staged дві AMD64 Canon DLL.
- ✅ Fresh Windows buildfix9 build доходить до фінального link `OpenAstroSuite.exe`.
- ✅ Попередній `.58` runtime smoke: node стартує після WebRTC DLL staging, OpenCV/UVC Live View працює з `Capture FPS=MAX`, `Preview=60`.
- ✅ Direct-MC mount coordinate model **v9** HIL-кваліфікована на реальному монтуванні з `Axis1Sign=+1`, `Axis2Sign=-1`.
- ✅ Free-point Sky Map GOTO фізично рухає реальне монтування.
- ✅ Sky Map target → Scheduler transfer працює.
- ✅ QHY5III462C still FITS і native Live View історично HIL-positive.
- ✅ Gemini focuser basic motion/status та AF cancel/start-position restore HIL-positive.
- ✅ SER відкривається в AutoStakkert.
- ✅ Mount GOTO abort HIL-positive.

## Публічний сайт/документація

- ✅ `openastro.link` і `www.openastro.link` зараз доступні та віддають HTTPS.
- 🟡 Live deployment, перевірений 2026-09-09, усе ще віддає старіший snapshot сайту, тому DNS більше не blocker; blocker — **deploy current site source**.
- ✅ Поточний repository site source тепер містить rationale проєкту, public manifesto, документацію для users/astronomers, OAL core developers, third-party integrations та driver-SDK entry points з українськими mirrors.

## Найближчий runtime/HIL

1. Запустити buildfix9 node і вимагати `oal.canon` + `oal.zwo.eaf` у native registry без `ERROR_BAD_EXE_FORMAT` / `%1 is not a valid Win32 application`.
2. Підтвердити, що Live View telemetry показує actual `webrtc-datachannel`, а не лише `/video` fallback.
3. QHY short-exposure HIL: `Capture FPS=MAX`, `Preview=60`; виміряти capture/record/preview FPS і drop counters.
4. SER 5–10 хв з `recordDropped=0`, потім звірити AutoStakkert frame count з telemetry/sidecar.
5. Preview 15/30/60/MAX під час recording; preview policy не має знижувати acquisition/recording throughput.
6. Main + Guide Dual Live на двох physical cameras з independent exposure/gain/rates і per-role WebRTC fallback.
7. 30–50 rapid direct-Wi-Fi manual press/release/direction changes; жодного ignored press або continued motion after release.
8. Repeat Scene AF і sparse-scene still auto-exposure HIL; потім night star/HFR AF.
9. Scheduler → ASTAP/recenter → planetary → mosaic → Polar Alignment → full supervised night.

## High-rate/WebRTC architecture

- `captureFpsLimit=0` означає camera maximum; preview має незалежний limit, default 60 FPS.
- Raw SER append відбувається до preview/debayer/JPEG/network work.
- `/events` — JSON control/state.
- `/video` — binary OALV v1 fallback preview.
- `/webrtc` — signaling; `oalv-main` і `oalv-guide` — незалежні unordered, partially reliable DataChannel.
- OALW v1 fragments несуть наявний OALV/JPEG packet. RTP H.264/H.265/AV1 media tracks ще не реалізовані.
- Slow networking може drop-ати preview; SER recording не має silently drop-атися.

Static branch evidence: **75/75 general scripts PASS**, **43/43 WebRTC PASS**, **43/43 high-rate/Dual-Live PASS**.

## Реалізовано, але потрібен актуальний HIL

- 🟡 Direct SynScan/EQDrive Wi-Fi manual-slew retry + instant-stop + post-condition verification + fail-safe stop.
- 🟡 Stateful mount-axis inversion checkboxes.
- 🟡 Sky Map target synchronization у Mount tab.
- 🟡 Scene AF: bright-tail metering + tiled contrast + compact search + repeatability gate.
- 🟡 Sparse-scene still auto-exposure P99.5 + hysteresis/crossing + gain-change reset.
- 🟡 Camera footprints, mosaic overlay і Stellarium FOV export.
- 🟡 Actual WebRTC transport/fallback, high-rate QHY, zero-drop SER і simultaneous Main+Guide Dual Live.

## Frozen facts монтування

- Coordinate model: **v9**.
- Qualified signs: `Axis1Sign=+1`, `Axis2Sign=-1`.
- Native serial EQDrive і direct UDP/11880 SynScan/EQDrive Wi-Fi використовують спільну Core geometry/GOTO planning.
- **Не змінювати** v9 geometry, axis meaning, Home/Park convention, serial/Wi-Fi parity або polarity без нових суперечливих HIL-доказів.
- Hidden `maxNativeGotoDeg`/15° qualification gate видалений. Operator sky-safety та explicit raw-axis mechanical guards лишаються окремими mechanisms.

## Межа Beta

Старий Windows compilation blocker закритий. Supervised `0.3 Beta` тепер блокує переважно **runtime/HIL qualification**, а не відсутність базового core implementation. Повний список — `docs/uk/CURRENT_CHECKLIST.md`, порядок робіт — `docs/uk/ROADMAP_P0_P1_IMPLEMENTATION.md`.

Smart Telescope UX лишається scope OAL 1.0.
