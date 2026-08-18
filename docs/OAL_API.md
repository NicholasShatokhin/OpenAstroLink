# OpenAstroLink HTTP API v0.2

Envelope:

```json
{"ok":true,"error":null,"data":{}}
```

## Discovery/state

- `GET /api/v1/devices`
- `GET /api/v1/state`

## Mount

- `GET /api/v1/mounts/{id}/status`
- `POST /api/v1/mounts/{id}/slew` — `{raDeg, decDeg}`
- `POST /api/v1/mounts/{id}/sync`
- `POST /api/v1/mounts/{id}/tracking` — `{enabled}`
- `POST /api/v1/mounts/{id}/park` — `{parked}`
- `POST /api/v1/mounts/{id}/pulse-guide` — `{direction, durationMs}`

## Focuser

- `GET /api/v1/focusers/{id}/status`
- `POST /api/v1/focusers/{id}/move` — `{position}`
- `POST /api/v1/focusers/{id}/move-relative` — `{delta}`
- `POST /api/v1/focusers/{id}/halt`

## Camera/analysis

- `POST /api/v1/cameras/{id}/capture`
- `POST /api/v1/solve`
- `POST /api/v1/autofocus/{id}/run`

`capture` може повертати PNG як Base64 для простоти референсної реалізації. Для великих астрокамер виробнича версія повинна використовувати окреме binary/frame endpoint або object storage.

## Motion and observer

- `POST /api/v1/motion/estimate`
- `POST /api/v1/observer/system-location`

## Guiding

- `POST /api/v1/guider/default/start`
- `POST /api/v1/guider/default/stop`
- `GET /api/v1/guider/default/status`

## Polar alignment

- `POST /api/v1/polar-align/sample`
- `POST /api/v1/polar-align/ra-offset` — `{deltaDeg}`
- `GET /api/v1/polar-align/estimate`

## Sessions

- `POST /api/v1/sessions`
- `GET /api/v1/sessions/current`

## WebSocket

Messages:

```json
{
  "type": "state|solveResult|autofocusProgress|autofocusResult|guidingUpdate|sessionUpdate",
  "timestampUtc": "...",
  "payload": {}
}
```
