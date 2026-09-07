# OpenAstroLink / OpenAstroSuite — handoff у новий чат

**Authoritative snapshot:** v0.2.10.55


## v0.2.10.55 checkpoint — high-rate streaming

- Прибрано старий 30 FPS Live View cap; capture і preview rates незалежні (`0 = MAX`).
- Raw SER append виконується до droppable preview processing.
- Remote Live View використовує OALV v1 binary `/video`; JSON events — `/events`.
- Main і guide cameras можуть стрімити одночасно з independent resource locks та Dual Live telemetry.
- QHY/ZWO native live buffers повторно використовуються; є 8-bit high-rate і selectable 16-bit Live View.
- Потрібен repeat HIL для >=60 FPS, zero-record-drop SER та Dual Live.
- Fixes v0.2.10.54 для Wi-Fi manual-slew / scene AF / sparse exposure не закриті до repeat HIL.
- Mount geometry v9 frozen.

## Не починати історію проєкту заново

Продовжувати з поточного repository. Не перевиводити direct-MC mount geometry і не повертатися до pre-v9 polarity hypotheses без нових HIL-доказів.

## Frozen current state

- Mount coordinate model **v9** HIL-підтверджений на реальному EQDrive/Sky-Watcher hardware.
- Qualified mapping: `Axis1Sign=+1`, `Axis2Sign=-1`.
- Native EQDrive serial і direct SynScan/EQDrive Wi-Fi використовують одну Core geometry/GOTO planning.
- Mechanical Home/Park лишається підготовленою polar Home pose з чинними v9 conventions.
- v0.2.10.50 видаляє **лише** тимчасовий прихований EQDrive `maxNativeGotoDeg` qualification gate. **Geometry, polarity, Home/Park і transport-direction logic не змінені.**
- Core/profile `maxGotoSkyDeltaDeg` safety лишається operator-controlled; raw-axis `maxAxisDeltaDeg` — явний guard.

## Build foundation тепер кваліфікована

- Windows x64 / MSVC 2022 + Ninja: успішний full observatory build.
- Native Linux x86_64: успішний observatory build, включно з automatic per-user Qt bootstrap на Jammy.
- Linux/WSL → AArch64 Raspberry Pi: 100% build для node, hardware probe і native drivers.
- Підтверджені ARM64 vendor paths: QHYCCD 26.06.04, Canon EDSDK, ZWO ASI, ZWO EAF, плюс source-native Gemini/Sky-Watcher/EQDrive.
- Raspberry Pi 5 використовує той самий generic `aarch64` target. Physical Pi 5 runtime/HIL ще pending.
- macOS Apple Silicon/Intel presets/bootstrap є; physical Mac qualification pending.

## Sky Map

OpenAstroSuite v0.2.10.52 має lightweight offline Sky Map у лівій області: bright stars/selected DSOs, pan/zoom/search, live telescope і solved markers, приблизний main-camera FOV та controller-backed Slew/Sync/Abort/Park/Scheduler actions. v0.2.10.52 також дозволяє вибирати довільну видиму точку неба: click у порожнє місце переводиться через existing horizontal→J2000 path і використовує той самий GOTO/Scheduler contract. Catalogue-object → Scheduler transfer підтверджений у running GUI. Окремої mount geometry мапа не вводить.

## HIL findings 2026-09-06

- Free-point Sky Map GOTO фізично працює; transfer target у Scheduler підтверджений.
- Sky Map selection тепер також копіює той самий J2000 target у Mount-tab coordinates.
- Direct Wi-Fi manual commands доходили до node, але physical press/release були intermittent; v0.2.10.54 harden-ить UDP start/instant-stop retries та post-condition checks.
- Axis inversion відображається stateful checkboxes у Mount tab.
- Scene autofocus control/cancel/restore працює, але optical convergence v0.2.10.53 була надто повільною/шумною. v0.2.10.54 використовує fast bright-tail metering, tiled contrast і repeatability gate.
- Still auto-exposure HIL показав sparse-scene limit cycle ~0.64 с ↔ ~1.43 с. v0.2.10.54 використовує P99.5 bright-tail control, smooth highlight handling, crossing bisection та reset при gain change.
- QHY Live/Still і базовий Gemini motion HIL-positive.
- Mount geometry v9 лишається frozen.

## Driver policy

Native OAL — reference/default path. Нормальна конфігурація має `OAS_ENABLE_INDI=OFF`. INDI вмикається лише явно через `*-indi-release` або `OAS_ENABLE_INDI=ON` для compatibility/unsupported hardware.

## Dependency policy

Platform wrappers спочатку шукають існуючі dependencies і bootstrap-ять redistributable components, де це можливо. QHY/ZWO/Qt/OpenCV автоматизовані там, де source/download deterministic. Canon EDSDK — тільки local discovery/manual download.

## Що вже є для наступної фази

- Node-owned hardware/workflows; local/remote GUI.
- HTTP + WebSocket state/control.
- Async operations/resource locks.
- Native QHY, Canon, ZWO ASI/EAF, Gemini, Sky-Watcher, EQDrive.
- Live View, FITS/RAW, SER, histogram exposure assistant.
- Реалізація scene/star autofocus.
- Plate solving/adaptive urban solve.
- Persistent per-block scheduler з DSO, planetary та mosaic execution.
- Guided Polar Alignment з optional safe-region motion.
- Stellarium live position/GOTO bridge.

## Найближча Beta — порядок

1. **HIL autofocus**: convergence, repeatability, backlash, cancel/failure rollback, final verification frame.
2. **HIL auto-exposure**: convergence, lock, reacquisition після lighting change.
3. **Scheduler HIL**: mixed blocks, cancellation, restart-at-block-boundary behavior.
4. **Mosaic HIL**: FOV tile geometry, solve/recenter, serpentine traversal.
5. **Polar Alignment HIL**: real-sky sampling і safe-region motion.

Smart Telescope UX у цю Beta не тягнути — це OAL 1.0.

## Далі читати

1. `START_HERE_UA.md`
2. `docs/uk/STATUS.md`
3. `docs/uk/MOUNT_GEOMETRY.md`
4. `docs/uk/BUILD_PLATFORMS.md`
5. `docs/uk/VALIDATION.md`
6. `docs/uk/ROADMAP_P0_P1_IMPLEMENTATION.md`
7. `PROJECT_MANIFEST_UA.md`

### Sky Map framing v0.2.10.53
Offline Sky Map тепер має measured `lastSolvedFrame` geometry, predicted main/guide profile footprints, Scheduler-synchronized mosaic planner grid і client-side export однієї вибраної рамки у rectangular FOV marker Stellarium Remote Control. Standard Stellarium Telescope Control лишається mount position/GOTO only. Mount v9 geometry не змінена.
