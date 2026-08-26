# Маніфест проєкту — OpenAstroSuite / OpenAstroLink v0.2.10.13

Канонічна документація — англійська. `PROJECT_MANIFEST_UA.md` є українським дзеркалом.

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostics: `oal-hardware-probe`
- Core: `oas_core`
- Protocol/native driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.10.13-urban-adaptive-plate-solve`
- C++20
- Minimum CMake: 3.20
- Preset schema: v2
- Windows toolchain: MSVC 2022 x64 + Ninja
- Qt 6.4+, OpenCV 4
- Native driver ABI: C ABI v2

## Native OAL drivers

`oal.qhy`, `oal.canon`, `oal.zwo.asi`, `oal.zwo.eaf`, `oal.gemini`, `oal.skywatcher`, reference `oal.simulated`.

Усі фізичні native drivers ще HIL-pending для конкретних device/firmware/host.

## Compatibility

INDI, ASCOM Alpaca, LX200, OpenCV/UVC і remote OAL adapters збережені. Native OAL пріоритетний; INDI легко вмикається для іншого обладнання.

## Уже реалізовані foundations

- Local/remote GUI, hardware належить node.
- Per-device disconnect і state snapshot/reconnect.
- Operation manager + resource locks.
- Async slew, main/guide exposure, autofocus.
- Main+guide camera roles та optical profiles.
- Native ABI-v2 registry/capabilities/events/cancellation/frame publication.
- Stellarium position/GOTO bridge.
- Cross-platform Windows/Linux/RPi presets/scripts.

## Основні відкриті задачі

Idempotency; повний RFC 9457 HTTP error model; replayable WebSocket events; TLS/auth/scopes/audit/safety; durable FITS/RAW/SER; production guiding; durable scheduler; driver sandbox/crash recovery; public conformance suite.

## Документація

Головна: `README.md`, `PROJECT_MANIFEST.md`, `docs/*.md`.  
Українські дзеркала: `README_UA.md`, `PROJECT_MANIFEST_UA.md`, `docs/uk/*.md`.  
Специфікація: `docs/OAL_SPECIFICATION.md`.  
Handoff: `docs/NEW_CHAT_HANDOFF.md` і `docs/uk/NEW_CHAT_HANDOFF.md`.


## Windows HIL update v0.2.10.11

- Gemini EAF: native discovery, connection, direct motion і autofocus-driven motion підтверджені на реальному Windows/USB-serial обладнанні.
- Node shutdown: `Ctrl+C` тепер проходить через main-thread graceful shutdown до руйнування Qt event dispatcher; другий `Ctrl+C` залишається force-terminate escape hatch.

- Статус HIL: Gemini EAF пройшов базову Windows-перевірку discovery/connection/motion; інші physical native drivers ще потребують повної HIL-кваліфікації.


## v0.2.10.13 — адаптивний plate solving для міського неба

- `solver.adaptive` виконується на node та захоплює серію коротких експозицій замість одного довгого кадру.
- Кадри оцінюються до запуску ASTAP, вирівнюються за зорями, складаються та очищаються від великомасштабного градієнта засвітки.
- Binning тепер зберігається в `CameraFrame`, а ASTAP отримує правильне поле зору для 2x2/іншого binning.
- За наявності реального монтування поточні RA/Dec автоматично використовуються як hint; radius пошуку адаптивно розширюється.
- Додано `POST /api/v1/solve/adaptive` та кнопку **Adaptive urban capture + solve** у GUI.
