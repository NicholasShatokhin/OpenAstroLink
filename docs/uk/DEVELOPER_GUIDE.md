# Guide для core-розробників OpenAstroLink

> Для contributors, які працюють над `openastrolink-node`, `oas_core`, OpenAstroSuite, build infrastructure, algorithms та реалізацією OAL protocol.

## Прочитати спочатку

1. `../CURRENT_STATUS_UA.md`
2. `CURRENT_CHECKLIST.md`
3. `ARCHITECTURE.md`
4. `OAL_SPECIFICATION.md`
5. `OAL_API.md`
6. `VALIDATION.md`
7. `ROADMAP_P0_P1_IMPLEMENTATION.md`

Перед mount changes обов'язково прочитати `MOUNT_GEOMETRY.md`.

## Шари repository

- `src/core/` — observatory/application control і ownership довгих workflows.
- `src/oal/` — OAL HTTP/WebSocket/native-driver plumbing.
- `src/backends/` — concrete compatibility/direct backends.
- `drivers/` — native OAL driver modules.
- `include/oal/driver_api.h` — native driver ABI.
- `schemas/` — machine-readable driver manifest schema.
- `docs/openapi.yaml` — HTTP API description.
- `site/` — source `openastro.link`.
- `tests/` і `tools/*check.py` — structural/regression qualification.

## Invariants, які не можна змінювати випадково

- Hardware sessions належать node; GUI не повинен ставати authoritative device owner.
- Native OAL drivers — default; INDI optional compatibility і OFF by default.
- Direct-MC mount geometry v9 HIL-qualified та frozen: `Axis1Sign=+1`, `Axis2Sign=-1` для підтвердженого profile.
- Operator sky safety та raw-axis mechanical guards відокремлені від coordinate geometry.
- Main/guide cameras мають незалежні resource locks.
- SER/science recording лишається upstream від droppable preview/network work.
- English docs canonical; UA mirrors синхронізуються.

## Додавання або зміна feature

1. Визначити ownership і resources.
2. Вирішити, чи це synchronous query, чи long-running OAL operation.
3. Публікувати capabilities замість vendor assumptions у clients.
4. Device-specific code тримати в driver/backend, workflow policy — у Core.
5. Визначити cancel/failure semantics до оголошення operation завершеною.
6. Додати regression coverage.
7. Оновити canonical EN docs і UA mirror.
8. Чітко позначити: implementation-only, build-qualified, simulated чи HIL-qualified.

## Native driver development

Почати з `NATIVE_DRIVER_SDK.md`, `include/oal/driver_api.h` і `schemas/driver-manifest-v2.schema.json`. Driver має надавати stable identity, device enumeration, capabilities, health та typed invoke/cancel. Camera frames повинні проходити native frame callback path, а не великі Base64 JSON responses.

## API/protocol work

- Тримати `docs/openapi.yaml`, `OAL_API.md` та implementation синхронними.
- Не додавати GUI-only state, якщо concept належить node state/capabilities.
- Зберігати operation state model: `queued -> running -> succeeded|failed|cancelled`.
- Поточний HTTP error envelope і event replay semantics transitional; не називати їх frozen 1.0.

## Camera/streaming

High-rate invariant:

```text
camera acquisition -> authoritative recording -> preview processing -> OALV/WebRTC
```

Preview може drop-атися. Recording не повинен throttling-итися JPEG encoding, GUI rendering чи network congestion. WebRTC v0.2.10.58 переносить OALV/JPEG через DataChannel; RTP H.264/H.265/AV1 ще немає.

## Build/vendor SDK

Windows x64 зараз використовує Qt 6.10/MSVC2022, vendor SDK staging і `libdatachannel` через project bootstrap. Runtime DLL staging перевіряє PE architecture. Не можна довіряти лише назві каталогу `x64`; build plumbing має перевіряти binary.

## Qualification discipline

Green build не дорівнює HIL. HIL старої revision не є автоматично доказом для зміненого code path. При зміні qualification state оновлювати `VALIDATION.md`, `CURRENT_CHECKLIST.md` і release note.

## Review checklist

- ownership/resources correct;
- немає hidden safety/geometry change;
- cancellation/failure handled;
- capabilities/API documented;
- regression tests updated;
- EN + UA docs synchronized;
- qualification claim відповідає evidence;
- build/generated artifacts не потрапили випадково в commit.
