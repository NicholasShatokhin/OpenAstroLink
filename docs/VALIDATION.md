# Validation — v0.2.5 Raspberry Pi / INDI / ASTAP / QHY hardware path

Validation updated 18 August 2026.

The consolidation environment does not contain the Raspberry Pi Qt/QHY toolchain or physical telescope hardware, so no claim of RPi/QHY/Gemini/mount HIL success is made here. This package is the code/configuration increment that must now be compiled and qualified on the target Pi.

## Static/structural gates for this package

Run from the repository root:

```bash
python3 tools/project_smoke_check.py
python3 tools/node_architecture_check.py
python3 tools/runtime_state_ui_check.py
python3 tools/operation_model_check.py
python3 tools/async_exposure_check.py
python3 tools/rpi_hardware_path_check.py
bash -n scripts/bootstrap_rpi_observatory.sh
bash -n scripts/install_indi_service.sh
bash -n scripts/install_rpi_node.sh
```

Also parse `docs/openapi.yaml` and `CMakePresets.json` before packaging.

## New v0.2.5 runtime gates

1. RPi64 release configure/build with `OAS_ENABLE_QHY=ON` and `OAS_ENABLE_INDI=ON`.
2. `oal-hardware-probe` reports ASTAP executable, at least one QHY exact ID, and the exact INDI device names/properties for mount and Gemini focuser.
3. INDI mount: status → small GOTO/TRACK → abort → sync → park/unpark as supported.
4. Gemini: absolute position → small absolute/relative moves → halt → reconnect. The device must expose `ABS_FOCUS_POSITION` for the current OAL autofocus path.
5. QHY: short/long single-frame exposures, exact-ID reconnect, cancel exposure, ROI/binning, 16-bit frame when the camera supports it.
6. ASTAP: repeated real-star solves with plausible RA/DEC/rotation/scale using the actual telescope profile.
7. Combined AF: QHY + Gemini star autofocus while mount control/safety status remains responsive.
8. Repeat local GUI tests from a remote computer over LAN/VPN without moving hardware ownership out of the node.

## Known boundaries

- QHY continuous planetary streaming/SER is not implemented in v0.2.5.
- ASTAP solve is currently synchronous at the OAL `/solve` call; migration to a cancellable `solve` operation is still required before unattended use.
- The current frame preview is not the final FITS/RAW science data plane.
- Polar-alignment mathematics exists, but the fully automated capture/RA-rotation/solve/live-adjust wizard is still pending.
- Security/TLS/safety interlocks and durable recovery are not finished.
