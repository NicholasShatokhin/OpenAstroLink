# Project manifest

- Application: `OpenAstroSuite`
- Protocol/driver framework: `OpenAstroLink (OAL)`
- Version: `0.2.1-gemini-eaf`
- Language: C++20
- UI/framework: Qt 6.4+
- Image processing: OpenCV 4
- REST: Qt HTTP Server
- Events: Qt WebSockets
- Serial: Qt SerialPort
- Native driver ABI: stable C ABI v1, JSON payloads

## Entry points

- GUI executable: `src/app/main.cpp`
- Main orchestration: `src/core/application_controller.*`
- OAL REST: `src/oal/oal_server.*`
- OAL WebSocket: `src/oal/oal_ws_server.*`
- Driver SDK: `include/oal/driver_api.h`
- Example native driver: `examples/oal_sim_driver.cpp`
- OpenAPI: `docs/openapi.yaml`
