# Validation — v0.2.6 Native OAL Driver Foundation

Validation updated 19 August 2026.

No claim is made that the real Raspberry Pi/QHY/Gemini/mount hardware has passed HIL in the consolidation environment. The purpose of this package is to establish the native OAL driver boundary and make QHY the first native hardware implementation.

## Static/structural gates

Run from repository root:

```bash
python3 tools/project_smoke_check.py
python3 tools/node_architecture_check.py
python3 tools/runtime_state_ui_check.py
python3 tools/operation_model_check.py
python3 tools/async_exposure_check.py
python3 tools/rpi_hardware_path_check.py
python3 tools/native_driver_foundation_check.py
bash -n scripts/bootstrap_rpi_observatory.sh
bash -n scripts/install_indi_service.sh
bash -n scripts/install_rpi_node.sh
```

Also parse:

- `docs/openapi.yaml`;
- `CMakePresets.json`;
- native driver manifest/schema JSON.

## ABI-v2 reference driver smoke

The dependency-free `oal.simulated` shared library was compiled as a standalone `.so`, loaded with a minimal ABI-v2 host using `dlopen`, enumerated successfully, connected a simulated camera and published one 1280×960 MONO16 frame through `publishFrame()`.

Observed copied pixel payload:

```text
2,457,600 bytes
```

This validates the ABI table, host callbacks and out-of-band frame contract independently of Qt/OpenCV.

## QHY compile-shape check

`drivers/qhy/oal_driver_qhy.cpp` was compiled against a local stub `qhyccd.h` containing the exact API signatures used by the plugin. This is a C++ syntax/API-shape check only.

It does **not** substitute for:

- linking against the real ARM64 QHYCCD SDK;
- running udev/USB access on the Pi;
- camera HIL;
- exposure-cancel HIL;
- bit-depth/ROI/model-specific validation.

## Required target HIL gates

1. RPi64 release configure/build with native QHY and optional INDI compatibility enabled.
2. Native driver registry discovers `oal.qhy` from installed manifest/library.
3. Exact QHY ID connect/reconnect/reboot persistence.
4. Short and long exposures, abort, ROI/binning/gain/offset, 16-bit path where supported.
5. No INDI server running: native QHY still works. This is a required architecture test.
6. INDI mount compatibility: status → small GOTO → abort → tracking → sync → park where supported.
7. Gemini compatibility: absolute position → move → halt → reconnect; document exact driver/firmware.
8. ASTAP repeated real-star solves with plausible RA/DEC/rotation/scale.
9. Combined autofocus: native QHY + Gemini path while mount control remains responsive.
10. Local GUI and remote GUI observe the same node/device/operation state.

## Known boundaries

- Native Gemini and native mount direct drivers are not claimed yet.
- Sandboxed out-of-process driver host is not implemented; an ABI-v2 manifest requesting it is rejected rather than downgraded.
- QHY live streaming/SER remains pending and is reported as unsupported.
- ASTAP solve remains synchronous at the current HTTP endpoint.
- Durable operation persistence, idempotency, RFC 9457, sequenced event replay, security/safety and final science data plane remain pending.
