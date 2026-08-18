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

## Hotfix 4 — GUI state rehydration and focuser feedback

Observed on the Windows validation machine (Qt 6.10.0 MSVC2022 x64 / OpenCV 4.14 debug):

- `openastrolink-node` starts and listens on HTTP 8080 / WebSocket 8090.
- local remote-mode GUI connects to the event stream.
- simulated camera, mount and focuser connect on the node.
- `/api/v1/devices` reports all three devices as connected.
- simulated mount slew is accepted and simulated camera capture completes.
- closing the GUI leaves the node and device connections alive.

The first reconnect exposed two UI/state bugs: the new GUI did not rehydrate already-connected device state, and focuser moves had no visible status. Hotfix 4 addresses these by adding an explicit full-state refresh path, restoring device backend/endpoint/status from `/api/v1/state`, refreshing after WebSocket reconnect, and adding focuser position/moving/temperature status plus HALT/refresh controls. Explicit **Disconnect all** now also disables persisted auto-connect, while normal node shutdown preserves the bindings for the next boot.

Static validation after Hotfix 4:

- `python tools/project_smoke_check.py` — PASS.
- `python tools/node_architecture_check.py` — PASS.
- `docs/openapi.yaml` YAML parse — PASS.
- state-refresh/device-status/focuser-status structural checks — PASS.

A complete Hotfix 4 compile is still to be performed on the Windows validation machine; the consolidation container still lacks the Qt SDK.

## v0.2.2 hotfix 5 runtime-state fixes

- Mount Tracking/Parked controls are now driven from the node's authoritative state snapshot and updated with signal blocking, so reconnect does not accidentally send commands.
- Focuser target position is separate from actual position; state refresh no longer overwrites an operator-edited target.
- Autofocus temporarily disables the command tabs in the synchronous v0.2.2 transport to prevent nested HTTP requests while camera/focuser are reserved. This is a temporary guard until P0 asynchronous operation resources/resource locks replace synchronous autofocus.
- Control-plane HTTP requests use `Connection: close` in this prototype to avoid reusing a potentially stale keep-alive socket after a long synchronous operation.
