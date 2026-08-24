# Маніфест проєкту — OpenAstroSuite / OpenAstroLink v0.2.10.10

Канонічна документація — англійська. `PROJECT_MANIFEST_UA.md` є українським дзеркалом.

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostics: `oal-hardware-probe`
- Core: `oas_core`
- Protocol/native driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.10.10-gemini-manifest-timing-fix`
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
