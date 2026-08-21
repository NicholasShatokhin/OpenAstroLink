# Validation — v0.2.9 ZWO / Stellarium / dual-camera increment

Validation updated 21 August 2026.

No claim is made that the real Raspberry Pi/QHY/Canon/Gemini/Sky-Watcher hardware has passed HIL in the consolidation environment. The package provides native OAL paths for the current telescope set and adds native ZWO ASI/EAF support. Hardware qualification is still required on the exact devices, SDK versions, firmware, USB topology, and Raspberry Pi deployment before production labels are assigned.

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

1. RPi64 release configure/build with native QHY + Canon EOS + Gemini + Sky-Watcher; repeat once with INDI compatibility disabled and once enabled.
2. Native driver registry discovers the installed manifests/libraries. For the current telescope this includes `oal.qhy`, `oal.canon`, `oal.gemini`, and `oal.skywatcher`; ZWO qualification additionally requires `oal.zwo.asi` and/or `oal.zwo.eaf` as applicable.
3. Exact QHY ID connect/reconnect/reboot persistence; short/long exposures, abort, ROI/binning/gain/offset and 16-bit path where supported.
4. Canon EOS connect/reconnect, 1–5 s capture, RAW+JPEG original-file spool + preview, 35–60 s Bulb and cooperative Bulb cancellation.
5. Gemini native serial: absolute position → small moves → temperature/status → reconnect; document exact firmware. Do not claim HALT until target-firmware stop semantics are verified.
6. Sky-Watcher native SynScan: status → small GOTO → abort → tracking → sync → pulse guide; validate pier-side/alignment reporting. Park remains unsupported in this profile unless a verified transport/profile supplies it.
7. Stop `indiserver` completely and repeat native QHY/Canon/Gemini/Sky-Watcher discovery/control. This is the required proof that INDI is optional rather than part of the native path.
8. Re-enable INDI and verify an additional compatibility-only device can coexist with the native telescope hardware.
9. ASTAP repeated real-star solves with plausible RA/DEC/rotation/scale.
10. Combined autofocus with a native camera + native Gemini while mount control remains responsive.
11. Local GUI and remote GUI observe the same node/device/operation state.


## ZWO-specific HIL gates

1. Link `oal.zwo.asi` against the real target ASI SDK and enumerate every connected camera by distinct camera ID.
2. Run at least 100 repeated ASI exposures, then validate abort, ROI, supported bin values, RAW8/RAW16 as applicable, gain/offset bounds, USB unplug/replug, and node restart persistence.
3. Connect two ASI cameras simultaneously and verify independent device identities plus main/guide role assignment without cross-talk.
4. Link `oal.zwo.eaf` against the real EAF SDK; validate position, safe-range absolute/relative motion, halt, max step, temperature, reverse/backlash reporting, disconnect/reconnect, and autofocus repeatability.
5. Run `oal-hardware-probe --require-zwo` and require both ASI and EAF when testing a complete ZWO pair.

## Stellarium interoperability gate

Connect a current Stellarium Telescope Control client to the node's TCP bridge, verify continuous mount-position updates and several small safe GOTO commands, then repeat while a second OAL GUI is connected. Camera/focuser controls are intentionally outside the standard Stellarium telescope protocol and are not part of this gate.

## Dual-camera / optical-profile gates

1. Connect distinct main and guide cameras at the same time, including two cameras exported by one multi-device native driver.
2. Start simultaneous main and guide exposures and verify independent `camera` / `camera.guide` locks and frame IDs.
3. Restart only the GUI and then the node; verify role bindings and optical-train profiles are restored correctly.
4. Confirm main and guide f-ratio/image-scale calculations against independently calculated values.

## Known boundaries

- Native Gemini and native Sky-Watcher protocol paths exist, but are still HIL-pending on the user's exact hardware/firmware. Gemini HALT is deliberately not advertised until its target-firmware stop command is verified; the current SynScan profile deliberately does not invent a park command.
- Native Canon EOS is implemented through the OAL ABI-v2 plug-in boundary using libgphoto2 as the low-level USB/PTP transport; real EOS/RPi HIL is still pending.
- Sandboxed out-of-process driver host is not implemented; an ABI-v2 manifest requesting it is rejected rather than downgraded.
- QHY live streaming/SER remains pending and is reported as unsupported.
- ASTAP solve remains synchronous at the current HTTP endpoint.
- Durable operation persistence, idempotency, RFC 9457, sequenced event replay, security/safety and final science data plane remain pending.

## v0.2.9 native Canon EOS increment

- `tools/native_canon_driver_check.py`: PASS (static integration/capability assertions).
- `drivers/canon/oal_driver_canon.cpp`: compiled locally with C++20 `-Wall -Wextra -Wpedantic -Werror` against a minimal libgphoto2 API-shape stub and the system libjpeg headers. This validates C++/ABI/library-call shape, not a real EOS/libgphoto2 hardware session. Canon Bulb control now probes actual `eosremoterelease` choices instead of assuming one fixed label and falls back to legacy `bulb`.
- All existing project smoke/operation/native-telescope/RPi checks pass after the Canon integration.
- `tests/native_protocol_smoke.cpp`: PASS with `-Wall -Wextra -Wpedantic -Werror`.
- All repository JSON files parse and all shell scripts pass `bash -n`.

Still pending and must not be presented as passed: real Raspberry Pi compile against distro `libgphoto2-dev`, Canon EOS USB HIL, Bulb cancel on the user's exact model, RAW/CR2/CR3 preview behavior, and multi-camera/USB reconnect qualification.

## v0.2.9 validation gates

The release adds `zwo_native_driver_check.py`, `dual_camera_optics_check.py` and `stellarium_bridge_check.py`. The ZWO sources are additionally compiled against SDK-compatible API stubs with warnings treated as errors; this is not a substitute for real ZWO SDK linking or hardware validation. The Stellarium bridge still requires an interoperability test with a real Stellarium build and mount.
