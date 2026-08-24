# Validation plan — v0.2.10.5

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

```bash
./scripts/check_build_environment.sh
```

PASS requires:

- CMake >=3.20;
- Ninja available;
- native compiler works;
- vendor `.so` architecture matches `uname -m`;
- required Qt/OpenCV/libgphoto2 development packages are visible.

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

1. connect/discover;
2. read RA/Dec/status;
3. small safe GOTO;
4. abort during GOTO;
5. tracking off/on;
6. sync;
7. pier-side query if supported;
8. short pulse guide;
9. disconnect/reconnect;
10. node restart/reconnect.

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
