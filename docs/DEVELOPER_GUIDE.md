# OpenAstroLink core developer guide

> For contributors working on `openastrolink-node`, `oas_core`, OpenAstroSuite, build infrastructure, algorithms and the OAL protocol implementation.

## Read first

1. `../CURRENT_STATUS.md`
2. `CURRENT_CHECKLIST.md`
3. `ARCHITECTURE.md`
4. `OAL_SPECIFICATION.md`
5. `OAL_API.md`
6. `VALIDATION.md`
7. `ROADMAP_P0_P1_IMPLEMENTATION.md`

For mount work, read `MOUNT_GEOMETRY.md` before touching any coordinate transformation.

## Repository layers

- `src/core/` — observatory/application control and long-running workflow ownership.
- `src/oal/` — OAL HTTP/WebSocket/native-driver plumbing.
- `src/backends/` — concrete compatibility/direct backends used by Core.
- `drivers/` — native OAL driver modules.
- `include/oal/driver_api.h` — native driver ABI surface.
- `schemas/` — machine-readable driver manifest schema.
- `docs/openapi.yaml` — HTTP API description.
- `site/` — source for `openastro.link`.
- `tests/` and `tools/*check.py` — structural/regression qualification.

## Invariants that must not be casually changed

- The node owns hardware sessions; GUI code must not become the authoritative device owner.
- Native OAL drivers are the default architecture; INDI is optional compatibility and OFF by default.
- Mount direct-MC geometry v9 is HIL-qualified and frozen: `Axis1Sign=+1`, `Axis2Sign=-1` for the proven profile.
- Sky/operator safety and raw-axis mechanical guards are separate from coordinate geometry.
- Main and guide cameras use independent resource locks.
- SER/science recording stays upstream of droppable preview/network work.
- English documentation is canonical; Ukrainian mirrors are maintained alongside it.

## Adding or changing a feature

1. Define the ownership and resources involved.
2. Decide whether it is a synchronous query or a long-running OAL operation.
3. Publish capabilities rather than hard-coding vendor assumptions in clients.
4. Keep device-specific code in a driver/backend and workflow policy in Core.
5. Add cancellation/failure semantics before presenting an operation as complete.
6. Add regression coverage for the contract being changed.
7. Update canonical EN docs and the UA mirror.
8. Record whether the result is implementation-only, build-qualified, simulated, or HIL-qualified.

## Native driver development

Start with `NATIVE_DRIVER_SDK.md`, `include/oal/driver_api.h` and `schemas/driver-manifest-v2.schema.json`. A driver should expose stable identity, device enumeration, capabilities, health and typed invoke/cancel behaviour. Camera frame publication must use the native frame callback path rather than large Base64 JSON responses.

## API and protocol work

- Keep `docs/openapi.yaml`, `OAL_API.md` and implementation aligned.
- Avoid adding GUI-only state when the same concept belongs in node state/capabilities.
- Preserve the operation state model: `queued -> running -> succeeded|failed|cancelled`.
- Current HTTP error envelopes and event replay semantics are transitional; do not claim 1.0 stability where the specification marks work pending.

## Camera/streaming work

The high-rate invariant is:

```text
camera acquisition -> authoritative recording -> preview processing -> OALV/WebRTC
```

Preview can drop. Recording must not be throttled by JPEG encoding, GUI rendering or network congestion. WebRTC in v0.2.10.58 carries OALV/JPEG over DataChannel; it is not yet an RTP H.264/H.265/AV1 media implementation.

## Build and vendor SDK work

Windows x64 currently uses Qt 6.10/MSVC2022, vendor SDK staging and `libdatachannel` through the project bootstrap. Runtime DLL staging validates PE architecture. Do not trust directory names such as `x64`; validate the binary when build plumbing handles vendor runtimes.

## Qualification discipline

A green build is not HIL. A HIL result from an old revision is not automatically evidence for a changed code path. Update `VALIDATION.md`, `CURRENT_CHECKLIST.md` and the release note when qualification state changes.

## Pull-request/review checklist

- ownership/resources correct;
- no hidden safety/geometry change;
- cancellation/failure handled;
- capabilities/API documented;
- regression tests added/updated;
- EN + UA docs synchronized;
- qualification claim matches evidence;
- no generated/build artifacts accidentally committed.

## Restricted-sky design invariants (v0.2.10.60)

Keep these layers separate in code and review:

1. **Mechanical/raw-axis guard** — hard physical safety; never weakened by visibility UX.
2. **Polar-alignment motion limits** — optional limits for the alignment workflow.
3. **Observable Sky Region** — persisted visibility/planning policy; may reject automated GOTO and influence Scheduler selection, but must not block normal manual joystick motion.

Assisted Polar samples are fitted in the local horizontal frame at each sample timestamp. Do not replace this with a constant J2000 offset model: a fixed physical polar-axis error is fixed in the local frame, while equatorial coordinates rotate with sidereal time. The current fit estimates an effective pointing-frame rotation and reports residual/span/confidence; cone, flexure, refraction and other pointing-model errors can still bias the estimate.

Scheduler out-of-order visibility deferral is intentionally conservative across a crash. Until the 1.0 durable per-block journal exists, prefer repeating a later block after restart over advancing the persistent cursor past an unfinished deferred block.
