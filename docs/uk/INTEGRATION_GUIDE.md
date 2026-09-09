# Integration guide OpenAstroLink для сторонніх програм

> Для developers planetariums, mobile/desktop clients, automation services, observatory dashboards, acquisition tools та іншого ПЗ, яке хоче використовувати OpenAstroLink node без залежності від OpenAstroSuite GUI.

## Integration model

Вважайте `openastrolink-node` authoritative equipment/workflow service. Програма має підключитися до node, discover backends/devices/capabilities, запускати commands/operations і отримувати state/events.

OpenAstroSuite для цього не повинен бути запущений.

## Починати з discovery

Корисні поточні endpoints:

- `GET /api/v1/node/info`
- `GET /api/v1/node/backends`
- `GET /api/v1/drivers`
- `GET /api/v1/drivers/devices`
- `GET /api/v1/devices`
- `GET /api/v1/state`
- per-device capability endpoints `/api/v1/drivers/.../capabilities`

Точний HTTP surface див. `openapi.yaml` та `OAL_API.md`.

## Device bindings і roles

Використовуйте identity/capabilities, які публікує node, а не виводьте features з display name. Cameras можуть мати roles `main` і `guide`; їх resource locks незалежні.

## Operations

Long-running actions використовують model:

```text
queued -> running -> succeeded | failed | cancelled
```

Слідкуйте за operation ID та використовуйте cancel замість blocking HTTP request на весь hardware action.

## Events/reconnect

JSON state/operation events ідуть через event WebSocket (`/events` у current node). Client повинен reconnect-итися та повторно читати aggregate state. Stable sequence/replay semantics ще OAL 1.0 work, тому durable event replay зараз не гарантований.

## Preview/video

Поточні remote preview transports:

- binary OALV v1 через `/video` WebSocket;
- WebRTC signaling `/webrtc` з `oalv-main` і `oalv-guide` DataChannels, які несуть OALW-fragmented OALV/JPEG;
- JSON control/state відділений від pixels.

WebRTC зараз є preview transport, а не authoritative science-recording path. Втрачений preview frame не означає втрату science frame.

## Science data/metadata

FITS/RAW/SER створюються node-side acquisition workflows. Не будуйте client, який використовує Base64 JSON як science data plane. Durable download/provenance APIs ще розвиваються — перед залежністю від pending interface звіряйте release note та `CURRENT_CHECKLIST.md`.

## Mount coordinates/safety

Використовуйте sky-coordinate APIs і залишайте hardware geometry node/backend. Не дублюйте direct-MC axis transforms у third-party client. HIL-qualified v9 geometry — node invariant. Operator sky-safety та mechanical guards — окремі policies.

## Compatibility/versioning

Проєкт pre-1.0. Integration повинна:

- query node/version/capabilities;
- tolerate unknown capability fields;
- не залежати від undocumented JSON fields;
- явно handle failed/cancelled operations;
- вважати current error-envelope та event-replay details transitional;
- pin/test against known OAL development release для production client сьогодні.

## Security boundary

Current development nodes призначені для trusted LAN/VPN. **Не** виставляйте unauthenticated HTTP/WebSocket ports напряму в Internet. TLS/auth/RBAC/audit — OAL 1.0 work.

## Мінімальний перший client

1. прочитати `/api/v1/node/info` і `/api/v1/state`;
2. enumerate devices/capabilities;
3. вибрати/connect device role;
4. виконати non-destructive query;
5. запустити operation і дочекатися terminal state;
6. subscribe to events та перевірити reconnect;
7. після цього додавати Live View чи Scheduler.

## Canonical references

- `OAL_SPECIFICATION.md`
- `OAL_API.md`
- `openapi.yaml`
- `WEBRTC_STREAMING.md`
- `HIGH_RATE_STREAMING.md`
- `COORDINATE_FRAMES.md`
- `CURRENT_CHECKLIST.md`
