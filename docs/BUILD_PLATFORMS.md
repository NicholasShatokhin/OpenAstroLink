# Платформи

## Raspberry Pi 4 — primary deployment target for v0.2.3

Use 64-bit Raspberry Pi OS, Release build and preferably Ninja. Build both the headless node and the GUI if a local monitor/keyboard may be attached:

```bash
cmake -S . -B build-rpi -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOAS_BUILD_NODE=ON \
  -DOAS_BUILD_GUI=ON \
  -DOAS_ENABLE_QHY=ON \
  -DOAS_ENABLE_INDI=ON
cmake --build build-rpi -j$(nproc)
```

Install `openastrolink-node` as a systemd service using `scripts/install_rpi_node.sh`. The local GUI should normally connect to `http://127.0.0.1:8080`; a remote GUI connects to the Pi LAN/VPN address. This ensures the hardware/core survives GUI closure and does not depend on an X/Wayland login session.

QHY requires an ARM-compatible QHYCCD SDK installation visible to CMake. INDI mode in this repository is a minimal built-in XML/TCP client, so an `indiserver` plus the appropriate vendor/mount/focuser drivers still runs separately.

Do not expose current unauthenticated OAL ports to the public Internet.

## Windows

Qt 6.4+ MSVC kit and OpenCV of the same ABI are recommended. The GUI can run either with an embedded development core or as a remote client to the Raspberry Pi:

```text
OpenAstroSuite --node http://<rpi-address>:8080
```

ASCOM equipment can still be reached through Alpaca/ASCOM Remote where appropriate.

## Linux desktop

Same model as Raspberry Pi. `openastrolink-node` can run separately from the GUI, or the GUI can use embedded developer mode. Serial devices require permissions for `/dev/ttyUSB*`/`/dev/ttyACM*`.

## macOS

The GUI can be used primarily as a remote OAL node client. Local embedded backends depend on the availability of Qt/OpenCV/vendor SDKs for the platform.


## v0.2.3 dependency note

The P0 operation manager uses `Qt6::Concurrent`; install the Qt Concurrent development component together with the existing Core/Network/HTTP Server/WebSockets modules.
