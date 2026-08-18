# Матриця зрілості — v0.2.2

| Модуль | Статус | Коментар |
|---|---|---|
| `oas_core` separation | Реалізовано | Core/algorithms/backends/OAL no longer require Qt Widgets |
| `openastrolink-node` | Реалізовано structurally | Headless `QCoreApplication`, HTTP/WS, device auto-connect; target build still required on RPi |
| GUI local-node mode | Реалізовано | Connects to `127.0.0.1:8080`; same UI can run on RPi monitor/keyboard |
| GUI remote-node mode | Реалізовано | Thin HTTP/WS client; algorithms execute on node |
| Embedded GUI core | Збережено | Developer/simulator fallback, not preferred production RPi topology |
| Persistent device bindings | Реалізовано | Successful camera/mount/focuser connections are restored on node boot |
| Simulated camera/mount/focuser | Робочий baseline | First node/API test target |
| OpenCV/UVC camera | Робочий baseline | Exposure behaviour depends on OS/driver |
| Serial LX200 | Робочий baseline | GOTO/Sync; capability hardening pending |
| ASCOM Alpaca mount/focuser | Реалізовано | Mainly relevant outside native Linux/INDI path |
| GeminiAstro EAF | Compatibility profile | INDI/Alpaca bridge; real Gemini hardware validation still pending |
| QHY SDK | Опційний | Single-frame path exists; RPi/QHY HIL and continuous planetary mode pending |
| INDI XML backend | Експериментальний | Suitable for first RPi integration tests; exact driver properties need HIL validation |
| OAL HTTP/WS | Expanded | Node discovery/config/profile/solver/device control plus existing astronomy endpoints |
| Star detector | Реалізовано | Existing implementation |
| Triangle solver | Prototype | ASTAP adapter still required for production first-light workflow |
| Autofocus | Algorithmically implemented | Runs on node in remote mode; hardware tuning pending |
| Polar alignment math | Реалізовано | Runs on node in remote mode; automated wizard/orchestration still pending |
| Guiding | Basic | Calibration/dither/recovery pending |
| Scheduler | State model only | Not yet durable execution engine |
| Security/TLS/auth | P0 pending | **Do not expose node directly to public Internet** |
| Async operations/locks | P0 pending | Current long commands are still synchronous HTTP requests |
| FITS/RAW data plane | P0 pending | Preview/Base64 path remains for this architecture increment |
