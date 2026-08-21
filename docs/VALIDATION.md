# Validation — v0.2.10 cross-platform desktop observatory

English is canonical. Ukrainian mirror: `docs/uk/VALIDATION.md`.

Validation deliberately separates source/static checks, SDK API-shape checks, simulator tests, platform builds, and real hardware-in-the-loop (HIL). A driver is not called production-ready merely because it compiles.

## Source/static regression

The v0.2.10 package must pass:

```bash
python3 tools/project_smoke_check.py
python3 tools/node_architecture_check.py
python3 tools/runtime_state_ui_check.py
python3 tools/operation_model_check.py
python3 tools/async_exposure_check.py
python3 tools/rpi_hardware_path_check.py
python3 tools/native_driver_foundation_check.py
python3 tools/native_telescope_hardware_pack_check.py
python3 tools/native_canon_driver_check.py
python3 tools/canon_edsdk_compile_check.py
python3 tools/zwo_native_driver_check.py
python3 tools/dual_camera_optics_check.py
python3 tools/stellarium_bridge_check.py
python3 tools/cross_platform_build_check.py
```

Also validate:

- `CMakePresets.json` and `CMakeUserPresets.example.json` parse as JSON;
- `cmake --list-presets` succeeds;
- shell scripts pass `bash -n`;
- PowerShell build/package scripts are exercised on Windows;
- `docs/openapi.yaml` parses successfully.

## v0.2.10 cross-platform build gates

### Windows x64 / MSVC 2022

Required gates:

1. `windows-core-release` configures and compiles with Qt 6 MSVC + OpenCV and no vendor SDKs.
2. `windows-native-release` compiles with Windows x64 QHY, ZWO ASI/EAF, and Canon EDSDK SDK artifacts.
3. `windows-observatory-release` compiles with the INDI compatibility adapter enabled in parallel.
4. `scripts/package_windows.ps1` produces a self-contained Qt/OpenCV application package when the required runtime DLL directories are supplied.
5. The packaged node starts outside Qt Creator without manually editing `PATH`.

### Linux x86_64

Required gates:

1. `linux-native-release` configures and compiles with x86_64 QHY/ZWO SDKs and native Canon/libgphoto2.
2. `linux-observatory-release` repeats with INDI compatibility enabled.
3. `linux-node-release` builds without the GUI.
4. `cmake --install` and `scripts/package_linux.sh` produce the documented installation layout.
5. `ldd` reports no unresolved runtime dependencies for OAL executables and native driver `.so` files after vendor runtimes are installed.

### Linux ARM64 / Raspberry Pi

The same gates apply with ARM64/AArch64 vendor libraries. `file` must confirm the architecture of every vendor SDK library before linking.

## Canon transport checks

### libgphoto2 transport

The existing native Canon implementation remains the default Linux path. Its source/API integration checks pass, but real EOS USB/PTP HIL is still required.

### Canon EDSDK transport

`drivers/canon/oal_driver_canon_edsdk.cpp` has a dependency-light API-shape compile gate using `tests/stubs/edsdk/` with `-Wall -Wextra -Wpedantic -Werror`. The implementation covers SDK init/enumeration/session open, host transfer, timed Bulb/cancel, original-file storage, thumbnail preview publication, and OAL ABI-v2 frame publication.

This is **not** a substitute for linking to the user's actual Canon EDSDK or testing a real camera. Real Windows HIL must validate EDSDK version compatibility, transfer callbacks/message-loop behavior, body mode requirements for Bulb, CR2/CR3 transfer, reconnect, and cancellation.

## Native hardware HIL

For each target OS that will directly own hardware:

1. discover and connect the exact QHY camera by stable identity;
2. discover and connect Canon EOS with the platform-selected native transport;
3. discover every attached ZWO ASI camera distinctly, including simultaneous main+guide cameras;
4. validate ZWO EAF and Gemini safe-range movement and status;
5. validate Sky-Watcher small GOTO, abort, tracking, sync, guide pulses and reported coordinates;
6. stop `indiserver` and repeat the native telescope test to prove the native path is independent;
7. re-enable INDI and connect at least one compatibility-only device alongside the native devices;
8. run ASTAP on repeated real-star frames;
9. verify local GUI and remote GUI observe the same node state;
10. verify Stellarium position/GOTO interoperability.

## Known boundaries after v0.2.10

- Windows Canon EDSDK transport is implemented but HIL-pending.
- Native Gemini and native Sky-Watcher are still HIL-pending on the exact hardware/firmware; unsupported semantics remain deliberately unadvertised.
- QHY/ZWO high-rate planetary data plane/SER is not yet production-complete.
- Guiding, durable scheduler/session recovery, final FITS/RAW data plane, RFC 9457/idempotency/replay, security/safety, and out-of-process driver sandboxing remain future hardening work.
