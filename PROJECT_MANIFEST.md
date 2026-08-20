# Project manifest

- Application: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Hardware diagnostic: `oal-hardware-probe`
- Core library: `oas_core`
- Protocol/driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.5-rpi-first-hardware`
- Language: C++20
- UI/framework: Qt 6.4+
- Image processing: OpenCV 4
- REST: Qt HTTP Server
- Events: Qt WebSockets
- Async execution: Qt Concurrent + node-local `OperationManager`
- Serial: Qt SerialPort
- Native driver ABI: stable C ABI v1, JSON payloads

## Entry points

- GUI executable: `src/app/main.cpp`
- Headless node: `src/node/main.cpp`
- Hardware probe: `src/tools/hardware_probe.cpp`
- GUI control contract: `src/core/observatory_controller.h`
- Local core: `src/core/application_controller.*`
- Remote GUI proxy: `src/core/remote_observatory_controller.*`
- Async operation manager: `src/core/operation_manager.*`
- ASTAP adapter: `src/algorithms/astap_solver.*`
- INDI mount/focuser: `src/backends/indi_devices.*`
- QHY direct SDK camera: `src/backends/qhy_camera.*`
- Gemini compatibility profile: `src/backends/gemini_eaf_focuser.*`
- OAL REST: `src/oal/oal_server.*`
- OAL WebSocket: `src/oal/oal_ws_server.*`
- Driver SDK: `include/oal/driver_api.h`
- OpenAPI: `docs/openapi.yaml`
- RPi deployment: `docs/RPI_NODE.md`, `docs/RPI_FIRST_HARDWARE.md`, `packaging/systemd/*`
