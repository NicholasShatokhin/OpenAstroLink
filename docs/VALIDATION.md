# Validation — v0.2.2 Raspberry Pi node split

Validation updated 18 August 2026 for the headless-node / local-or-remote GUI increment.

The consolidation container does not contain Qt 6 development packages, so a complete Qt application build/link cannot be performed here. CMake reaches `find_package(Qt6)` and stops specifically because `Qt6Config.cmake` is absent. This package is therefore **structurally validated, not target-built or hardware-validated**.

Checks completed:

1. `python3 tools/project_smoke_check.py` — PASS.
2. `python3 tools/node_architecture_check.py` — PASS.
3. All project-local quoted includes resolve under `src/` or `include/`.
4. CMake now defines separate `oas_core`, `OpenAstroSuite`, and `openastrolink-node` targets.
5. `MainWindow` depends on `ObservatoryController`, not directly on `ApplicationController`.
6. `RemoteObservatoryController` proxies capture, solve, autofocus, mount/focuser control, guiding, polar alignment, profile/solver setup and sessions to OAL endpoints.
7. `openastrolink-node` restores persisted device bindings and starts HTTP/WebSocket independently of any GUI.
8. OAL reference server exposes node bootstrap/backend/profile/device-configuration endpoints required by the remote GUI.
9. A systemd template and RPi install helper are present.
10. Existing Gemini EAF compatibility profile remains wired through Alpaca/INDI.
11. No QHY, Gemini, mount, INDI-server or Raspberry Pi hardware-in-the-loop test has been performed in this environment.
12. P0 security, async operation resources/locks and the binary science-frame data plane are still pending; the current network API must stay on a trusted LAN/VPN.

Target validation sequence on the Raspberry Pi should be:

```text
build node+GUI
→ start node with --no-autoconnect
→ connect GUI to localhost
→ simulated camera/mount/focuser
→ reboot and verify systemd persistence
→ remote GUI from second computer
→ INDI mount/focuser
→ QHY single-frame capture
```

Only after those gates pass should the telescope be moved under real sky control.
