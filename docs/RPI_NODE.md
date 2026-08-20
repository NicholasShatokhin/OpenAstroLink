# Raspberry Pi 4 observatory node

Version 0.2.5 keeps the v0.2.2 process boundary and adds the first RPi hardware path for QHY + INDI mount/Gemini + ASTAP.

## Runtime model

```text
RPi4
  openastrolink-node (systemd, headless)
      ApplicationController
      algorithms + active devices
      HTTP :8080 / WebSocket :8090
           ▲
           │ localhost or LAN/VPN
           │
  OpenAstroSuite GUI (optional local monitor/keyboard)

Remote PC
  OpenAstroSuite GUI ───────────────► same node
```

The GUI is not the owner of hardware in node mode. `capture`, `solve`, `autofocus`, guiding, polar-alignment sampling/estimation and session state are invoked through the node API and execute on the Raspberry Pi. Closing the remote GUI therefore does not destroy the node process or release its devices. The GUI also performs a basic WebSocket reconnect if the event channel drops; sequence/replay recovery remains the later P0 event-hardening task.

An **Embedded core** mode is retained for development and offline desktop use, but the recommended Raspberry Pi deployment is the systemd node plus a GUI connected to `http://127.0.0.1:8080`.

## Build on Raspberry Pi OS 64-bit

Install Qt 6/OpenCV development packages available for your Raspberry Pi OS release, then configure the project. QHY additionally requires the vendor QHYCCD SDK headers/library on the Pi.

```bash
cmake -S . -B build-rpi -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOAS_BUILD_GUI=ON \
  -DOAS_BUILD_NODE=ON \
  -DOAS_ENABLE_QHY=ON \
  -DOAS_ENABLE_INDI=ON \
  -DOAS_ENABLE_GPHOTO2=OFF
cmake --build build-rpi -j$(nproc)
```

For the first architecture test, build with QHY/INDI disabled and use three simulated devices. Then enable hardware backends.

The systemd user must have permission to open the telescope interfaces. On Raspberry Pi OS this normally means membership in the relevant serial/USB groups (commonly `dialout`, and depending on device/udev rules also `plugdev`/`video`) plus the QHY vendor udev rules. Verify actual permissions with `ls -l /dev/ttyUSB* /dev/ttyACM*` and the QHY SDK installation rather than assuming a group name.

## Install the service

```bash
sudo ./scripts/install_rpi_node.sh build-rpi
systemctl status openastrolink-node
journalctl -u openastrolink-node -f
```

The service template starts:

```text
openastrolink-node --http-port 8080 --ws-port 8090
```

The node listens on the LAN. The current v0.2.5 API has **no production authentication/TLS yet**, so do not expose ports 8080/8090 directly to the public Internet. Use a trusted isolated LAN or VPN until P0 security is implemented.

## Local GUI with monitor and keyboard

Run from the build tree:

```bash
./build-rpi/OpenAstroSuite
```

If installed with `scripts/install_rpi_node.sh`, the installer also installs a desktop launcher **OpenAstroSuite (Local Observatory)** when the GUI binary is present.

Choose **This computer — local OAL node**. The GUI connects to `http://127.0.0.1:8080`; the core remains the systemd service.

This is the preferred mode on a Raspberry Pi with an attached monitor because the same core remains available to remote clients and continues running if the GUI is closed.

## Remote GUI

On another computer run the same GUI and choose **Remote OAL node**, for example:

```text
http://openastrolink-rpi.local:8080
```

or launch directly:

```bash
OpenAstroSuite --node http://192.168.1.50:8080
```

The GUI discovers available camera/mount/focuser/solver backends from the node. Connecting a device from the remote GUI configures the device **on the Raspberry Pi**, not on the client computer.

## Persistent device bindings

After a successful camera, mount or focuser connection the node persists:

- backend name;
- endpoint;
- `autoConnect=true`.

At the next boot `openastrolink-node` restores those bindings. If an INDI server or USB device is not ready yet, the node remains reachable and retries missing persisted devices every 10 seconds. Use `--no-autoconnect` for maintenance/troubleshooting.

Typical Raspberry configuration for the planned hardware is:

```text
Camera:  qhy        endpoint: 0
Focuser: gemini-eaf endpoint: indi:127.0.0.1:7624/<exact Gemini INDI device name>
Mount:   indi       endpoint: 127.0.0.1:7624/<exact mount INDI device name>
```

Exact INDI device names must match what the installed INDI drivers publish.

## v0.2.5 first-hardware update

The process/node architecture above is now paired with:

- `AstapSolver` CLI adapter with executable/database environment overrides;
- exact-name INDI discovery and standard mount/focuser mappings;
- per-call INDI TCP sessions safe for operation worker threads;
- direct QHY SDK camera selection by index or exact camera ID;
- process-lifetime QHY SDK initialization and cancellable single-frame acquisition;
- `oal-hardware-probe`;
- bootstrap and persistent INDI systemd helpers.

See `docs/RPI_FIRST_HARDWARE.md` for the target installation and HIL qualification sequence. Planetary live/SER, async solve, the automated polar wizard, durable DSO execution, FITS/RAW data plane and security/safety hardening remain pending.
