> **v0.2.10.59 update:** реалізовано Night Vision / Strict Night Mode та display-only black→red mono/raw preview; mobile GUI OAL 1.0 запланований як окремий Qt Quick/QML client. Fresh Windows/UI HIL для `.59` pending; `.58-buildfix9` — остання fresh Windows build-qualified база. Читати `NIGHT_VISION.md` і `MOBILE_QML_GUI.md`.

# OpenAstroLink / OpenAstroSuite — handoff у новий чат

**Authoritative snapshot:** v0.2.10.58-buildfix9  
**Дата snapshot:** 2026-09-08  
**Правило repository:** дерево `repository/` у FULL HANDOFF package є authoritative. Не відновлювати код зі старих фрагментів чату, якщо repository уже містить новішу реалізацію.

## Поточна qualification boundary — 2026-09-08

Точний v0.2.10.58-buildfix9 Windows x64/MSVC tree **fresh-build qualified** з увімкненим libdatachannel/WebRTC, WebRTC runtime staging, corrected ZWO EAF import linkage та AMD64 Canon EDSDK import/runtime pairing. Camera/WebRTC HIL ще не qualified: потрібен node-registry retest `oal.canon`/`oal.zwo.eaf`, потім QHY high-rate, WebRTC fallback і Dual-Live HIL. Повний список — `CURRENT_CHECKLIST.md`.


## Обов'язковий порядок читання

1. `CURRENT_STATUS_UA.md`
2. `START_HERE_UA.md`
3. `docs/uk/CURRENT_CHECKLIST.md`
4. `docs/uk/RELEASE_0.2.10.58.md`
5. `docs/uk/NEW_CHAT_HANDOFF.md`
6. `docs/uk/MOUNT_GEOMETRY.md`
7. `docs/uk/HIGH_RATE_STREAMING.md`
8. `docs/uk/WEBRTC_STREAMING.md`
9. `docs/uk/VALIDATION.md`
10. `docs/uk/ROADMAP_P0_P1_IMPLEMENTATION.md`
11. `PROJECT_MANIFEST_UA.md`

## Не починати вже вирішену історію заново

Проблема direct-MC mount geometry вирішена на поточному рівні доказів. Coordinate model **v9** HIL-підтверджена на реальному Sky-Watcher/EQDrive hardware з `Axis1Sign=+1`, `Axis2Sign=-1`. Native serial EQDrive і direct SynScan/EQDrive Wi-Fi використовують одну Core geometry/GOTO planning. Не змінювати geometry, axis meaning, Home/Park conventions, polarity або serial/Wi-Fi parity без нових суперечливих HIL-доказів.

Прихований driver-level 15° GOTO qualification gate видалений у v0.2.10.50. Це не видаляє окрему operator-controlled sky-safety policy або raw-axis mechanical guard.

## Platform/build state

- Windows x64/MSVC 2022 + Ninja: попередній full observatory build підтверджений.
- Native Linux x86_64: попередній full observatory build підтверджений, включно з per-user Qt bootstrap на Jammy.
- Linux/WSL → Raspberry Pi ARM64: попередній full node/probe/native-driver cross-build підтверджений до 100%.
- ARM64 vendor matrix підтверджена: QHYCCD 26.06.04, Canon EDSDK ARM64, ZWO ASI/EAF ARM64, Gemini, Sky-Watcher, EQDrive.
- Pi 5 використовує generic `aarch64`; physical Pi 5 qualification pending.
- macOS Intel/Apple Silicon presets/bootstrap реалізовані; physical Mac build/sign/runtime pending.
- Native drivers — default; INDI лишається optional і OFF by default.

Важлива qualification boundary: точний v0.2.10.58-buildfix9 Windows x64/MSVC tree уже **fresh-build qualified** з libdatachannel/WebRTC, WebRTC runtime staging, corrected ZWO EAF import linkage та AMD64 Canon EDSDK import/runtime pairing. OALV `/video` лишається fallback, SER — upstream. **Camera/WebRTC HIL ще не qualified**: потрібен node-registry retest `oal.canon`/`oal.zwo.eaf`, потім QHY high-rate, WebRTC fallback і Dual-Live HIL.

## HIL facts від 2026-09-06

Підтверджено:

- Free-point Sky Map click → J2000 GOTO фізично рухає реальне монтування.
- Sky Map target → Scheduler coordinate transfer працює.
- QHY5III462C still FITS і native Live View працюють.
- Gemini focuser motion/status працює; autofocus cancel повертає starting focus position.
- SER успішно відкривається в AutoStakkert.
- Mount GOTO abort працює.

Observed failures, які сформували current code:

- Manual direct-Wi-Fi slew міг проігнорувати press або продовжити рух після release, хоча node logs показували press/stop commands. Current code додає UDP retry, instant-stop, source validation, running/stopped post-condition checks і fail-safe stop.
- Axis inversion раніше була непрозорою; current UI використовує stateful checkboxes.
- Sky Map target тепер також заповнює J2000 target fields у Mount tab.
- Scene AF був повільним і не сходився надійно навіть біля очевидного фокуса. Старий global meter міг пройти 0.05→0.2→0.8→3.2→10 s на sparse daylight scene. Current scene AF використовує bright-tail metering, tiled structure/contrast, compact near-focus search і repeatability-gated improvement.
- Still auto-exposure oscillated на sparse bright scene. Виміряна sequence при gain 200 ходила приблизно між ~0.64 s та ~1.43 s; коротший frame мав корисний bright tail, довший уже кліпав його. Current code використовує sparse-scene P99.5 control, hysteresis/crossing logic і reset history при gain change.
- Live preview мав race з малим in-memory HTTP cache. Current high-rate path замінює per-frame HTTP/PNG/base64 fetch на binary `/video` OALV streaming.

## Camera data plane — current implementation

- Capture і preview FPS незалежні; `captureFpsLimit=0` означає camera maximum.
- Preview default 60 FPS і latest-only/droppable.
- QHY і ZWO native Live View reuse buffers.
- Raw SER append відбувається до preview/debayer/JPEG/UI work.
- JSON events/state лишаються на `/events`; pixels ідуть binary OALV v1 через `/video`.
- Main і guide cameras мають окремі resource locks і можуть одночасно stream-ити у Dual Live.
- GUI показує capture/record/preview FPS та preview/record drop telemetry.
- Desired invariant: preview може drop-атися; recording не повинен тихо втрачати frames.

Immediate HIL: >=60 FPS там, де hardware дозволяє, 5–10 min zero-record-drop SER, потім simultaneous main+guide Dual Live без starvation mount/focuser/events.

## Sky Map / framing state

- Offline left-side Sky Map працює без Stellarium.
- Можна обирати catalogue targets і довільні visible-sky points.
- Free-point GOTO HIL-confirmed.
- Target transfer у Scheduler HIL-confirmed.
- Selected map coordinates у current code також заповнюють Mount target fields.
- Після plate solve measured camera footprint можна малювати за solved center/scale/dimensions/PA.
- Planned main/guide footprints і Scheduler mosaic grid можна малювати до solve.
- Одну вибрану frame можна передати у rectangular FOV marker Stellarium Remote Control. Standard Stellarium Telescope Control лишається mount position/GOTO only.
- Footprint/Stellarium export ще потребує end-to-end HIL.

## Beta priorities — зберігати цей порядок, якщо новий blocker не змусить змінити його

1. Runtime registry retest buildfix9: `oal.canon` і `oal.zwo.eaf` без Win32 error 193.
2. High-rate main stream + SER zero-drop HIL.
3. Dual Live main+guide HIL.
4. Direct-Wi-Fi manual slew repeat HIL.
5. Scene autofocus repeat HIL; потім night star/HFR autofocus.
6. Still auto-exposure repeat HIL.
7. Scheduler end-to-end HIL.
8. Mosaic HIL.
9. Polar Alignment HIL.
10. Full supervised night qualification.

Не тягнути Smart Telescope UX, one-button observing або broad unattended-observatory automation у найближчу Beta; це OAL 1.0 work.

## Working style для нового чату

- Коли defect локалізований, віддавати перевагу конкретним repository edits/patches, а не лише абстрактним порадам.
- English лишається canonical documentation; українське mirror оновлювати одночасно.
- При зміні milestone синхронізувати `CURRENT_STATUS`, `START_HERE`, `STATUS`, `NEW_CHAT_HANDOFF`, `VALIDATION`, `ROADMAP`, release notes і `site/`.
- Чітко розрізняти implemented/static-tested, physically build-qualified і HIL-qualified.
- Не вважати GitHub новішим за supplied repository, якщо користувач явно не сказав, що вони synchronized.

### WebRTC v1
`/webrtc` signaling; `oalv-main` + `oalv-guide` DataChannel; OALW v1 fragments несуть OALV/JPEG; `/video` — per-role fallback. RTP codecs ще не реалізовані.

### 2026-09-07 buildfix8 vendor-runtime ABI note
Exact v0.2.10.58 Windows build уже успішний з WebRTC, node/OpenCV Live View стартує. Canon EDSDK і ZWO EAF plugin loads дали `ERROR_BAD_EXE_FORMAT`; buildfix8 перевіряє реальну PE Machine architecture, видаляє stale runtime duplicates і додає `scripts/repair_windows_vendor_runtime.cmd`. Перед QHY/WebRTC throughput HIL треба повторно перевірити ці два plugins.

- Windows buildfix9: Canon EDSDK import/runtime pair тепер перевіряється і за шляхом, і за PE machine; `EDSDK_64/Library/EDSDK.lib` жорстко прив'язана до sibling `EDSDK_64/Dll`. Runtime retest Canon/ZWO EAF ще pending.
