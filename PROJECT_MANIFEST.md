# Project manifest

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostic: `oal-hardware-probe`
- Core library: `oas_core`
- Protocol/driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.6-native-driver-foundation`
- Language: C++20
- UI/framework: Qt 6.4+
- Image processing: OpenCV 4
- REST: Qt HTTP Server
- Events: Qt WebSockets
- Async execution: Qt Concurrent + node-local `OperationManager`
- Serial: Qt SerialPort
- Native driver ABI: **C ABI v2 reference**, v1 compatibility retained
- Native frame boundary: host callback (`OalFrameDescriptorV2`), no Base64 driver payload

## Reference architecture

Native OAL drivers are preferred. INDI/Alpaca/LX200 are compatibility adapters and must not constrain OAL capabilities or semantics.

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
- Native reference simulator: `drivers/reference_simulated/*`
- Native QHY driver: `drivers/qhy/*`
- Manifest schema: `schemas/driver-manifest-v2.schema.json`
- Capability schema baseline: `schemas/native-capabilities-v1.schema.json`
- Compatibility INDI: `src/backends/indi_devices.*`
- Compatibility Gemini profile: `src/backends/gemini_eaf_focuser.*`
- ASTAP adapter: `src/algorithms/astap_solver.*`
- OAL REST: `src/oal/oal_server.*`
- OAL WebSocket: `src/oal/oal_ws_server.*`
- OpenAPI: `docs/openapi.yaml`
- RPi deployment: `docs/RPI_NODE.md`, `docs/RPI_FIRST_HARDWARE.md`, `packaging/systemd/*`
