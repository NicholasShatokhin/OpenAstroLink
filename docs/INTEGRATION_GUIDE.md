# OpenAstroLink integration guide for third-party applications

> Audience: developers of planetariums, mobile/desktop clients, automation services, observatory dashboards, acquisition tools and other software that wants to use an OpenAstroLink node without depending on the OpenAstroSuite GUI.

## Integration model

Treat `openastrolink-node` as the authoritative equipment/workflow service. Your application should connect to the node, discover available backends/devices/capabilities, issue commands or long-running operations, and observe state/events.

Do not assume that OpenAstroSuite must be running.

## Start with discovery

Useful current endpoints include:

- `GET /api/v1/node/info`
- `GET /api/v1/node/backends`
- `GET /api/v1/drivers`
- `GET /api/v1/drivers/devices`
- `GET /api/v1/devices`
- `GET /api/v1/state`
- per-device capability endpoints under `/api/v1/drivers/.../capabilities`

The exact HTTP surface is documented in `openapi.yaml` and `OAL_API.md`.

## Device bindings and roles

Clients should use node-published identity and capability data rather than infer device features from a display name. Cameras can have `main` and `guide` roles; their resource locks are independent.

## Operations

Long-running actions use the OAL operation model:

```text
queued -> running -> succeeded | failed | cancelled
```

Use operation IDs to observe progress/result and request cancellation instead of keeping a blocking request open for the full hardware action.

Current operation endpoints include listing/active/detail/cancel under `/api/v1/operations`.

## Events and reconnect

JSON state/operation events are delivered over the event WebSocket (`/events` in the current node). A client should be prepared to reconnect and re-fetch aggregate state. Stable sequence/replay semantics are still OAL 1.0 work, so current clients must not assume durable event replay after disconnection.

## Preview/video

Current remote preview transports are:

- binary OALV v1 over `/video` WebSocket;
- WebRTC signaling on `/webrtc`, with `oalv-main` and `oalv-guide` DataChannels carrying OALW-fragmented OALV/JPEG packets;
- JSON control/state remains separate from pixels.

WebRTC is currently a preview transport, not the authoritative science-recording path. Do not infer that an omitted preview frame means a science frame was lost.

## Science data and metadata

FITS/RAW/SER are produced by node-side acquisition workflows. Avoid building a client that treats Base64 JSON as the science data plane. Current durable download/provenance APIs are still evolving; consult the release note and `CURRENT_CHECKLIST.md` before depending on a pending interface.

## Mount coordinates and safety

Use documented sky-coordinate APIs and let the node/backend perform hardware geometry. Do not reproduce direct-MC axis transforms in a third-party client. The HIL-qualified v9 geometry is a node implementation invariant. Operator sky-safety and mechanical guards are separate policies.

## Compatibility and versioning

The project is pre-1.0. Integrations should:

- query node/version/capabilities;
- tolerate unknown capability fields;
- avoid relying on undocumented JSON fields;
- handle failed/cancelled operations explicitly;
- treat current error-envelope and event-replay details as transitional;
- pin/test against a known OAL development release when shipping a client today.

## Security boundary

Current development nodes are intended for trusted LAN/VPN use. Do **not** expose the unauthenticated development HTTP/WebSocket ports directly to the public Internet. TLS/auth/RBAC/audit are OAL 1.0 work.

## Recommended first client

A minimal third-party client should:

1. fetch `/api/v1/node/info` and `/api/v1/state`;
2. enumerate devices/capabilities;
3. connect/select one device role;
4. issue one non-destructive query;
5. start one operation and follow it to terminal state;
6. subscribe to events and recover after a reconnect;
7. only then add live preview or scheduler control.

## Canonical references

- `OAL_SPECIFICATION.md`
- `OAL_API.md`
- `openapi.yaml`
- `WEBRTC_STREAMING.md`
- `HIGH_RATE_STREAMING.md`
- `COORDINATE_FRAMES.md`
- `CURRENT_CHECKLIST.md`
