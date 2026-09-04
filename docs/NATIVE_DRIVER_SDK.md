## v0.2.10.51 build/distribution status

Native vendor SDK discovery/bootstrap is part of the supported build workflow. QHY 26.06.04 ARM64 has been validated through a full Raspberry Pi cross-build; QHY/ZWO staging verifies target architecture instead of trusting archive names. Canon EDSDK remains manual-download/local-discovery only. Native drivers remain default; INDI is optional compatibility.

# OpenAstroLink Native Driver SDK — ABI v2 (v0.2.10.5)

## 1. Architectural rule

Native OpenAstroLink drivers are the reference hardware path. INDI, ASCOM Alpaca and LX200 are compatibility/migration adapters and MUST NOT constrain the native OAL capability model.

```text
hardware
   │
   ▼
native OAL driver
   │  ABI v2
   ▼
OAL driver registry / host callbacks
   │
   ▼
OpenAstroLink core
   │
   ├─ HTTP / WebSocket
   ├─ operations / locks
   ├─ autofocus / solve / polar alignment
   └─ local or remote GUI
```

A vendor SDK may be used below a native OAL driver when the physical USB/serial protocol is not public. That is still a native OAL path: OAL, not INDI/ASCOM, defines identity, capabilities, operations, events, health and frame transfer semantics.

## 2. ABI v2 entry point

A native ABI-v2 library exports:

```c
extern "C" const OalDriverV2 *
oalCreateDriverV2(const OalDriverHostV2 *host);
```

The public C ABI is declared in `include/oal/driver_api.h`. C++ compiler ABI compatibility is intentionally not part of the contract.

The driver returns an `OalDriverV2` function table with:

- manifest/identity;
- `start` / `stop` lifecycle;
- device enumeration;
- typed capability document retrieval;
- health retrieval;
- method invocation with request/operation/deadline context;
- cancellation;
- explicit returned-string release.

## 3. Manifest

Every ABI-v2 driver MUST ship a sidecar `*.manifest.json`. The host validates it before loading the library. Schema: `schemas/driver-manifest-v2.schema.json`.

Minimum example:

```json
{
  "$schema": "../schemas/driver-manifest-v2.schema.json",
  "schema": "org.openastrolink.driver-manifest/v2",
  "abiVersion": 2,
  "driverId": "oal.example",
  "name": "Example native OAL driver",
  "version": "1.0.0",
  "library": "oal_driver_example",
  "deviceClasses": ["camera"],
  "threadModel": "per-device-serial",
  "isolation": "in-process",
  "permissions": []
}
```

`driverId` is stable identity. Device IDs are opaque within that driver. The node addresses a native device internally as:

```text
native:<driverId>/<encoded-deviceId>
```

Third-party manifests may declare future `out-of-process` isolation. **v0.2.10 deliberately refuses to load such a driver in-process** because the sandboxed driver host is not implemented yet; the node will not silently weaken the requested isolation policy.

## 4. Discovery and capabilities

`enumerateDevicesJson()` returns device identity, class, name, vendor and transport metadata. `capabilitiesJson()` returns a self-description document. Schema baseline: `schemas/native-capabilities-v1.schema.json`.

Clients MUST rely on capabilities rather than brand/model assumptions. Examples:

- camera exposure range and abortability;
- ROI/binning alignment and limits;
- gain/offset ranges;
- streaming support;
- focuser absolute/relative movement, temperature and limits;
- mount slew/abort/tracking/park/pulse-guide/pier-side support.

A missing capability is not equivalent to `true`.

## 5. Method invocation

The v0.2.10 native adapters map the following canonical method names:

### Common

```text
device.connect
device.disconnect
```

### Camera

```text
camera.capture
camera.abortExposure
```

### Mount

```text
mount.status
mount.slew
mount.abort
mount.sync
mount.setTracking
mount.park
mount.pulseGuide
```

### Focuser

```text
focuser.status
focuser.moveAbsolute
focuser.moveRelative
focuser.halt
```

The request and response payloads are UTF-8 JSON at the ABI boundary for control metadata. Large frame pixels MUST NOT be placed in those JSON strings.

## 6. Operation context and cancellation

`OalDriverCallV2` provides:

- request ID;
- OAL operation ID;
- monotonic deadline.

The driver can implement `cancel(deviceId, operationId)`. Host-level operation/resource locking remains authoritative. Full propagation of every operation ID into every legacy path is still being completed, but ABI v2 reserves the correct boundary now.

Drivers declare a concurrency model such as `serial`, `per-device-serial` or `concurrent`. The v0.2.10 loader enforces normal `serial`/`per-device-serial` invocation locking. A driver MUST NOT assume that calls arrive from the Qt GUI thread; OAL operations execute in worker threads.

Explicit safety/cancellation methods (`camera.abortExposure`, `mount.abort`, `focuser.halt`) and the ABI `cancel()` callback bypass the normal per-device serialization lock so they can interrupt an active long call. Drivers MUST make those abort paths thread-safe against the operation they interrupt.

## 7. Push events

The driver can call the host `emitEvent` callback. Typical native events are:

```text
device.connected
device.disconnected
camera.frameReady
mount.state
focuser.state
driver.warning
driver.error
```

OAL owns delivery to WebSocket clients. ABI v2 therefore avoids forcing each driver to implement network transport or polling loops.

## 8. Frame publication — no Base64

ABI v2 provides `publishFrame()` with `OalFrameDescriptorV2`:

- dimensions and stride;
- pixel format / bits / channels;
- capture timestamp;
- exposure/gain metadata;
- pointer + byte length;
- optional metadata JSON.

The current in-process host synchronously copies the supplied bytes and returns a `frameToken`; ownership of the original driver buffer remains with the driver after the callback returns.

This is a transitional native data-plane primitive. Future versions may add zero-copy/shared-memory/ring-buffer handles without changing device semantics.

## 9. Health

`healthJson()` separates hardware/driver health from a single successful command. Drivers should expose at least a state such as:

```text
ok
disconnected
degraded
error
```

Future out-of-process hosting will add `driver-crashed` and restart telemetry.

## 10. Reference drivers in v0.2.6

### `oal.simulated`

`drivers/reference_simulated/` is the normative reference implementation for ABI mechanics. Camera, mount and focuser are all exported through ABI v2, including event publication and out-of-band camera frames.

### `oal.qhy`

`drivers/qhy/` is the first native hardware driver. It uses the QHYCCD SDK only as the low-level hardware access layer. QHY does not require INDI or Alpaca when this plugin is built and available.

Implemented in v0.2.6:

- device scan by exact QHY hardware ID;
- connect/disconnect;
- single-frame acquisition;
- exposure/gain/offset;
- ROI/binning;
- 16-bit transfer request where available;
- exposure abort/cancellation;
- native frame publication;
- capability/health documents.

Not yet claimed:

- planetary continuous streaming/SER;
- cooling controls and all model-specific controls;
- hardware HIL qualification on the user's QHY camera.

## 11. Build and install

Built native plugins are emitted into:

```text
<build>/drivers/
```

On Linux/RPi the install helper copies plugin libraries and manifests to:

```text
/usr/local/lib/openastrolink/drivers
```

The loader also searches `OAL_DRIVER_PATH`, `appDir/drivers`, and standard OpenAstroLink library locations.

## 12. Compatibility adapters

INDI, ASCOM Alpaca and LX200 remain supported because they provide broad hardware coverage. They map legacy behavior into the richer OAL model. The reverse is explicitly not a design requirement: native OAL features may exist that cannot be represented by a legacy backend.


### `oal.canon`

`drivers/canon/` is a native ABI-v2 Canon EOS driver. On Linux/RPi it links `libgphoto2` as a low-level USB/PTP transport library; it does **not** use `indiserver`, INDI XML, ASCOM or an OAL compatibility adapter. Original DSLR files may be spooled locally while a decoded JPEG/embedded RAW preview is published through `publishFrame()` for autofocus/plate solving.
