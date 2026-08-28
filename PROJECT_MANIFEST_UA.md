# Маніфест проєкту — OpenAstroSuite / OpenAstroLink v0.2.10.19

Канонічна документація — англійська. `PROJECT_MANIFEST_UA.md` є українським дзеркалом.

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostics: `oal-hardware-probe`
- Core: `oas_core`
- Protocol/native driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.10.19-hil-start-catalog-mount-fix`
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


## v0.2.10.17 — адаптивний plate solving для міського неба

- `solver.adaptive` виконується на node та захоплює серію коротких експозицій замість одного довгого кадру.
- Кадри оцінюються до запуску ASTAP, вирівнюються за зорями, складаються та очищаються від великомасштабного градієнта засвітки.
- Binning тепер зберігається в `CameraFrame`, а ASTAP отримує правильне поле зору для 2x2/іншого binning.
- За наявності реального монтування поточні RA/Dec автоматично використовуються як hint; radius пошуку адаптивно розширюється.
- Додано `POST /api/v1/solve/adaptive` та кнопку **Adaptive urban capture + solve** у GUI.

## v0.2.10.17 HIL status/recovery
- підтверджено graceful Ctrl+C shutdown;
- Stellarium bridge приймає GOTO та передає RA/Dec у node;
- QHY auto-connect retry тепер виконує повторний native discovery;
- Gemini move не блокує status до завершення руху; autofocus чекає фізичний idle;
- додано live GUI polling position/moving;
- Sky-Watcher native discovery має детальний SynScan serial handshake log і same-session retry.
- Важливо: native RA/DEC Sky-Watcher device наразі підтримує SynScan hand-controller protocol. Direct USB/EQDIR motor-controller path ще не експонується як повний mount backend.


## v0.2.10.17 — сумісність mount

- Native ABI-v2 driver `oal.eqdrive` збережено окремо від `oal.skywatcher`; тепер спочатку пробується EQMOD-сумісний Sky-Watcher Motor Controller serial transport, а публічний EQDrive ASTEP використовується як fallback.
- Native EQDrive використовує `sync-anchor-v2`: перед RA/Dec GOTO потрібен один Sync на відому позицію; `coordinateValid=false` не дозволяє використовувати несинхронізовані координати як ASTAP hint або публікувати їх у Stellarium.
- Windows backend `ascom-classic` виконує COM automation в окремому native `oas-ascom-host.exe`; локальний GUI має ASCOM Chooser та setup dialog.
- `synscan-wifi` напряму працює з mount/EQDrive Wi-Fi adapter через Motor Controller protocol UDP 11880; `synscan-app` реалізує SynScan App Protocol до запущеного SynScan Pro через UDP 11881.
- Native serial discovery включає EQDrive та міграцію persisted port binding; новий вибір Gemini COM оновлює збережений native backend після успішного rediscovery.
- Документація: `docs/uk/EQDRIVE.md`, `docs/uk/ASCOM_CLASSIC.md`, `docs/uk/SYNSCAN_NETWORK.md`.


## v0.2.10.17 HIL-надійність камери/монтування/мережі

- QHY single-frame readout дотримується vendor lifecycle Exp/Get/Cancel, має watchdog скасування, readback параметрів SDK та логування статистики кадру.
- Native reconnect discovery фільтрується за відсутнім драйвером, тому активна QHY-сесія й COM, зайнятий EQMOD/ASCOM, не зачіпаються через інший відсутній пристрій.
- SynScan endpoint-и зберігаються окремо: direct `synscan-wifi` auto-discover-ить adapter на UDP 11880, а `synscan-app` auto-discover-ить хост SynScan Pro на UDP 11881; TCP 11882 лишається app-host compatibility service.
- Classic ASCOM GOTO логує координати та pier-side перед рухом; інверсія RA/DEC не додається.
- Focused EQDrive diagnostics охоплює всі DTR/RTS комбінації та кілька read-only ASTEP запитів.
- Застарілий COM binding Gemini може автоматично мігрувати після однозначного rediscovery, а ручний вибір COM refresh-ить лише Gemini.

## v0.2.10.19 live native catalogue / direct Wi-Fi

- Backend catalogue передається у node state, тому remote GUI оновлює списки після native hot-plug/discovery.
- `synscan-wifi` — direct Motor Controller UDP/11880 до mount/EQDrive Wi-Fi adapter; `synscan-app` лишається UDP/11881 до SynScan Pro.
- `oal.eqdrive` тепер dual-protocol: спочатку EQMOD-сумісний Sky-Watcher Motor Controller через serial, потім офіційний EQDrive ASTEP fallback; наявний `oal.skywatcher` збережено. Direct `synscan-wifi` використовує той самий Motor Controller command set через UDP 11880.
- Native EQDrive GOTO потребує Sync і має HIL safety limit; park/pier/pulse-guide ще не кваліфіковані.
