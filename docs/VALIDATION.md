## v0.2.10.51 validation focus

Build qualification is confirmed for Windows x64, native Linux x86_64 and the Linux/WSL→ARM64 Raspberry Pi node/probe/native-driver target. Keep those builds as regression gates. The next release gate is HIL workflow behavior, starting with autofocus. For the HIL-qualified direct-MC mount, retain a small-motion sanity check after hardware changes, then exercise normal supervised full-range GOTO; there is no longer a hidden 15° driver qualification cap. Profile-level sky-safety may still be enabled by the operator.

# Validation plan — v0.2.10.51

This release is primarily a build/HIL qualification checkpoint.

## A. Repository/static gates

Run from repository root:

```bash
python3 tools/project_smoke_check.py
python3 tools/node_architecture_check.py
python3 tools/runtime_state_ui_check.py
python3 tools/operation_model_check.py
python3 tools/async_exposure_check.py
python3 tools/native_driver_foundation_check.py
python3 tools/native_telescope_hardware_pack_check.py
python3 tools/native_canon_driver_check.py
python3 tools/zwo_native_driver_check.py
python3 tools/dual_camera_optics_check.py
python3 tools/stellarium_bridge_check.py
python3 tools/cross_platform_build_check.py
python3 tools/qhy_header_isolation_check.py
python3 tools/rpi_hardware_path_check.py
python3 tools/cmake_presets_compat_check.py
python3 tools/scheduler_dso_executor_v046_check.py
python3 tools/mount_v9_unrestricted_goto_check.py
```

Also verify:

```bash
cmake --list-presets
python3 -m json.tool CMakePresets.json >/dev/null
python3 -m json.tool CMakeUserPresets.example.json >/dev/null
```

## B. Build environment gates

### Windows

```powershell
.\scripts\check_build_environment.ps1
```

PASS requires:

- CMake >=3.20;
- Ninja available;
- MSVC `cl.exe` selected;
- no MinGW/Strawberry compiler in the configured build;
- Qt/OpenCV/vendor libraries use compatible x64 MSVC ABI.

### Linux

Recommended native gate:

```bash
./scripts/build_linux.sh my-linux-observatory --bootstrap-deps --clean
```

PASS requires:

- CMake >=3.20 and a working native C++ compiler;
- Qt >=6.4 including HttpServer/WebSockets/SerialPort/Positioning (system Qt or the per-user OAL bootstrap);
- OpenCV and target-architecture vendor `.so` files;
- full native observatory build success.

Ubuntu 22.04/Jammy system Qt 6.2.4 is intentionally not sufficient; the wrapper can install a per-user Qt through `aqtinstall`. Linux presets do not require Ninja.

## C. Windows build gates

First build without Canon EDSDK if EDSDK is unavailable:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

Inspect QHY compile commands: the QHY SDK include directory must **not** be present as a normal `/I` path. The generated QHY wrapper path should be present instead.

Later:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory-edsdk -Clean
```

## D. Linux/WSL build gates

For a current checkout:

```bash
rm -f CMakeUserPresets.json
cp CMakeUserPresets.example.json CMakeUserPresets.json
# edit local SDK paths
./scripts/build_linux.sh my-linux-observatory
```

If preset parsing fails, capture:

```bash
cmake --version
head -20 CMakePresets.json
head -20 CMakeUserPresets.json
```

## E. Hardware-in-the-loop gates

Run native-only first where practical, then re-run with compatibility enabled.

### Mount

1. connect/discover and confirm coordinate model v9 diagnostics;
2. read RA/Dec/status and mechanical axes;
3. after any hardware/configuration change, perform one small supervised sanity GOTO;
4. perform normal supervised full-range GOTO across representative east/west targets — the driver no longer has the temporary 15° qualification gate;
5. abort during GOTO and confirm both axes stop;
6. tracking off/on;
7. sync/plate-solve refinement;
8. Park/Unpark using the calibrated physical Home/Park workflow;
9. disconnect/reconnect and node restart/reconnect;
10. keep automatic meridian-flip orchestration out of unattended PASS until separately HIL-qualified.

If the profile-level `maxGotoSkyDeltaDeg` safety policy is enabled, set it deliberately for the test rather than confusing it with a transport-driver limitation.

### Focuser

1. read position;
2. ±100 and ±500 step movements;
3. absolute movement;
4. moving state;
5. halt where capability is advertised;
6. temperature/limits if supported;
7. reconnect/power-cycle behavior;
8. repeated autofocus sweeps.

### QHY/ZWO camera

1. enumerate exact hardware IDs;
2. 10 ms / 1 s / 5 s / 30 s exposures;
3. gain/offset;
4. ROI/binning;
5. 8/16-bit mode where supported;
6. cancel exposure;
7. 20–100 sequential frames;
8. unplug/replug;
9. node restart;
10. main+guide simultaneous operation for two-camera configurations.

### Canon

Linux/libgphoto2 or Windows/EDSDK:

1. discover/open session;
2. short capture;
3. original CR2/CR3/JPEG transfer;
4. Bulb exposure;
5. Bulb cancel;
6. preview;
7. repeated capture;
8. unplug/replug.


### Scheduler / DSO executor v0.2.10.46

Use a short, safe real-sky plan before attempting long unattended sequences:

1. connect a qualified mount, main camera, focuser and ASTAP solver;
2. create one DSO block with 2–3 short science frames;
3. enable recenter before the first frame with a 1–2 arcmin tolerance;
4. enable autofocus before the first frame;
5. confirm the state sequence `slew -> solve -> recenter-slew (if needed) -> solve -> autofocus -> capture`;
6. confirm each successful science exposure has a durable FITS/RAW artifact;
7. repeat with `recenter every 1 frame` and `autofocus every 1 frame` to exercise the aggressive policy;
8. press Stop during slew, solve, autofocus and exposure in separate runs and confirm the active child operation is cancelled and the session becomes `stopped`;
9. inject a solve failure / excessive pointing error and confirm the retry limit ends in `failed` with `lastError`;
10. submit a legacy `targets` session and confirm it still performs slew + science capture without silently requiring solver/focuser policy that the old schema could not express.

Planetary HIL: use a bright planet or lunar feature. Start with mount corrections OFF: GOTO → full-frame detection → autofocus → 10–20 s ROI SER; verify SER, `.txt`, and `.roi.jsonl`, then induce enough drift to force at least one ROI shift while frame dimensions stay constant. Test calibrated mount corrections separately only after the mount backend is known-safe; verify the calibration micro-slews return to target and every correction is recorded.

### ASTAP / real sky

1. solve known archived image;
2. solve real frame using configured optical profile;
3. compare solved image scale to configured image scale;
4. repeated solve near target;
5. closed-loop correction test.

## F. Supervised first-light acceptance

Minimum acceptable supervised use:

- clean build/package on the observatory host;
- hardware probe successful;
- mount/focuser/camera HIL successful;
- ASTAP successful on real frames;
- autofocus repeats reliably;
- abort paths verified.

## G. Not yet an unattended acceptance gate

Do not mark unattended production PASS until reliable event replay, idempotency, durable science storage, production guiding/session recovery, security/auth/audit, weather/roof/power safety, driver crash isolation and public conformance have been implemented and tested.

## Sky Map MVP — v0.2.10.51

- Confirm `Imaging / Sky Map` appears in the left workspace on Windows, Linux and Raspberry Pi GUI builds.
- With a configured site, select a bright star and compare displayed Alt/Az against an independent reference.
- Confirm a Sky Map GOTO and the Mount-tab GOTO for the same J2000 coordinate generate the same active-backend target.
- Confirm double-click GOTO can be aborted and Park/Unpark are routed through the active controller.
- After a plate solve, confirm the green solved marker follows the solved J2000 center.
- Confirm the red telescope marker updates from mount state and approximate FOV changes when the optical profile changes.
- Confirm `Use in Scheduler` copies the selected J2000 coordinate exactly.
