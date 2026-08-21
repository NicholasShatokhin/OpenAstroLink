# Project manifest

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostic: `oal-hardware-probe`
- Core library: `oas_core`
- Protocol/driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.7-native-telescope-hardware-pack`
- Language: C++20
- UI/framework: Qt 6.4+
- Image processing: OpenCV 4
- REST: Qt HTTP Server
- Events: Qt WebSockets
- Async execution: Qt Concurrent + node-local `OperationManager`
- Serial: Qt SerialPort through per-device persistent native serial sessions
- Native driver ABI: C ABI v2 reference; ABI v1 compatibility fixture retained
- Native frame boundary: host callback (`OalFrameDescriptorV2`), no Base64 driver payload

## Reference architecture

Native OAL drivers are preferred. INDI/Alpaca/LX200 are compatibility adapters and must not constrain OAL capabilities or semantics. INDI support is compiled in by default for general observatory builds but can be completely disabled with `OAS_ENABLE_INDI=OFF` / `rpi4-native-release`.

## Bundled native drivers

- `drivers/reference_simulated/*` — reference simulated camera/mount/focuser.
- `drivers/qhy/*` — native QHYCCD camera via QHYCCD SDK.
- `drivers/gemini/*` — native Gemini EAF / MyFocuserPro2-compatible serial focuser.
- `drivers/skywatcher/*` — native Sky-Watcher SynScan hand-controller serial mount plus experimental Motor Controller codec foundation.
- `drivers/common_blocking_serial_session.h` — persistent, dedicated-thread Qt serial transport helper for native drivers.

## Entry points

- GUI executable: `src/app/main.cpp`
- Headless node: `src/node/main.cpp`
- Hardware probe: `src/tools/hardware_probe.cpp`
- GUI control contract: `src/core/observatory_controller.h`
- Local core: `src/core/application_controller.*`
- Remote GUI proxy: `src/core/remote_observatory_controller.*`
- Async operation manager: `src/core/operation_manager.*`
- Native ABI: `include/oal/driver_api.h`
- Native registry/loader: `src/oal/driver_plugin_loader.*`
- Native IDevice adapters: `src/backends/oal_native_devices.*`
- Compatibility INDI: `src/backends/indi_devices.*`
- Compatibility Gemini profile: `src/backends/gemini_eaf_focuser.*`
- ASTAP adapter: `src/algorithms/astap_solver.*`
- OAL REST: `src/oal/oal_server.*`
- OAL WebSocket: `src/oal/oal_ws_server.*`
- OpenAPI: `docs/openapi.yaml`
- Native protocol smoke test: `tests/native_protocol_smoke.cpp`
- RPi deployment: `docs/RPI_NODE.md`, `docs/RPI_FIRST_HARDWARE.md`, `packaging/systemd/*`
