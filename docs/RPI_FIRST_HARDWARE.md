# Raspberry Pi 4 first-hardware path — v0.2.5

This increment is the first supervised hardware-in-the-loop path for the planned telescope node:

```text
OpenAstroSuite GUI (local RPi monitor or remote PC)
                    │
                    ▼
           openastrolink-node
        HTTP :8080 / WS :8090
          │          │        \
          │          │         \ ASTAP / astap_cli
          │          │
      QHY SDK      INDI server :7624
       camera        │       │
                 mount    Gemini EAF
```

The node remains the owner of all devices. The GUI may run on the Raspberry Pi and connect to `127.0.0.1`, or on another machine and connect to the Pi over the LAN/VPN.

## 1. Base Raspberry Pi packages

On a 64-bit Raspberry Pi OS / Debian-family installation:

```bash
sudo ./scripts/bootstrap_rpi_observatory.sh
```

This installs the build dependencies, Qt 6 modules, OpenCV and the INDI server/library packages available from the OS repositories. It also adds the operator to available `dialout`, `plugdev` and `video` groups. Log out or reboot after group changes.

The bootstrap intentionally does **not** download vendor binaries or choose a mount driver. Install the following separately:

1. QHYCCD SDK for the Pi architecture, including its udev rules. Either install `qhyccd.h` + `libqhyccd.so` under normal system paths or remember the SDK root for `QHYCCD_ROOT`.
2. The INDI driver for the exact mount.
3. The GeminiAstro focuser INDI driver. OAL uses the standard INDI focuser properties and does not assume an undocumented Gemini USB command protocol.
4. ASTAP/`astap_cli` for the Pi architecture and one ASTAP star database.

## 2. Start and identify INDI devices

Do not guess the INDI device names. Start the actual driver executables and inspect what they publish.

The helper installs a persistent `indiserver` service only after all requested driver executable names are present:

```bash
sudo ./scripts/install_indi_service.sh <mount-driver-executable> <gemini-driver-executable>
systemctl status openastrolink-indi
journalctl -u openastrolink-indi -f
```

Then build the project and run:

```bash
./build/rpi4-observatory/oal-hardware-probe --no-qhy --no-astap
```

The probe prints every discovered INDI device, the standard properties it exposes and a ready-to-copy endpoint such as:

```text
127.0.0.1:7624/Exact Mount Device Name
127.0.0.1:7624/Exact Focuser Device Name
```

For the Gemini profile use:

```text
indi:127.0.0.1:7624/Exact Focuser Device Name
```

The current OAL autofocus path requires the focuser to publish the INDI standard `ABS_FOCUS_POSITION` property. Mount control uses standard `EQUATORIAL_EOD_COORD`, `ON_COORD_SET`, `TELESCOPE_ABORT_MOTION` and optional park/tracking/pulse-guide properties.

## 3. ASTAP

`AstapSolver` searches, in order, for:

- `OAL_ASTAP_EXECUTABLE`;
- `astap_cli` / `astap` in `PATH`;
- common `/opt/astap`, `/usr/bin` and `/usr/local/bin` locations.

If the star database is not in ASTAP's normal location, set:

```bash
export OAL_ASTAP_DATABASE=/path/to/astap/database
```

An explicit executable can be supplied with:

```bash
export OAL_ASTAP_EXECUTABLE=/opt/astap/astap_cli
```

The adapter writes an 8/16-bit PNG from the current camera frame and uses ASTAP's documented `-f`, `-o`, `-fov`, `-ra`, `-spd`, `-r`, `-z` and optional `-d` arguments. It parses the resulting INI solution into OAL RA/DEC, rotation and image scale. If ASTAP is available when the node starts, it is the preferred solver backend; otherwise the prototype catalog solver remains available.

For reliable hints, enter the telescope focal length and camera pixel size in the OAL telescope profile before solving.

## 4. QHY direct SDK path

Build with `OAS_ENABLE_QHY=ON`. The backend can select the camera either by scan index or by the exact QHY camera ID, so a persistent deployment should prefer the exact ID after the first probe.

```bash
./build/rpi4-observatory/oal-hardware-probe --no-indi --no-astap
```

Example output:

```text
QHY: OK  1 camera(s)
  - index 0: <exact QHY ID>
```

Use Devices → Camera:

```text
Backend:  qhy
Endpoint: <exact QHY ID>
```

The current backend initializes the QHY SDK once per node process, selects single-frame mode, requests 16-bit transfer where supported, keeps raw Bayer/mono pixels, applies binning/ROI/exposure/gain/offset and uses the QHY exposure-abort API for operation cancellation.

Planetary continuous/live acquisition and SER writing are **not** part of v0.2.5 yet; this increment validates QHY single-frame capture for autofocus, ASTAP and DSO first-light work.

## 5. Build

When the QHY SDK is in a standard include/library location:

```bash
cmake --preset rpi4-observatory-release
cmake --build --preset rpi4-observatory-release -j$(nproc)
```

If the SDK is unpacked elsewhere:

```bash
cmake --preset rpi4-observatory-release -DQHYCCD_ROOT=/opt/qhyccd
cmake --build --preset rpi4-observatory-release -j$(nproc)
```

Before installing the node, run the full software/hardware probe:

```bash
./build/rpi4-observatory/oal-hardware-probe
```

Only after ASTAP, INDI and QHY all report `OK` should the real device bindings be saved in the node.

Install the node:

```bash
sudo ./scripts/install_rpi_node.sh build/rpi4-observatory
systemctl status openastrolink-node
journalctl -u openastrolink-node -f
```

## 6. First hardware qualification sequence

Use the local GUI first (`http://127.0.0.1:8080`), then repeat from the remote PC.

1. **Mount only.** Connect the exact INDI endpoint. Verify authoritative RA/DEC, tracking and parked state. Perform a very small safe slew, then test `ABORT MOUNT MOTION`. Do not start with a large unattended GOTO.
2. **Gemini EAF only.** Connect `gemini-eaf` through the exact INDI endpoint. Read absolute position, move a small amount in both directions, test HALT, then verify that reconnect preserves a sensible position.
3. **QHY only.** Connect by exact ID. Capture short frames, then 1 s / 5 s / 30 s frames. Cancel a longer exposure. Verify dimensions/bit depth and that reconnect works.
4. **ASTAP.** Capture a star field and solve it with the correct profile. Verify that repeated solves give plausible center, rotation and scale.
5. **Combined autofocus.** With QHY + Gemini connected, run a conservative star autofocus range. Camera and focuser are locked by the autofocus operation while mount safety controls remain available.
6. **Closed-loop operations later.** Automated target resolver/recenter, the complete polar-alignment wizard, durable DSO sequence storage and planetary SER are subsequent increments. The existing polar-axis estimator remains available for manual capture/solve/sample orchestration.

## 7. Threading note for INDI

Long OAL operations execute in worker threads. v0.2.5 therefore does not keep one `QTcpSocket` inside an INDI device object. Each INDI query/command uses a short-lived client socket created in the calling thread, while the INDI driver maintains the actual hardware connection. This avoids Qt socket thread-affinity violations when a worker operation and a status/safety request occur concurrently.

## 8. Safety boundary

v0.2.5 is a **supervised first-hardware build**, not an unattended observatory release. Authentication/TLS, weather/roof interlocks, durable operations and the science FITS/RAW data plane are still P0 work. Keep the node on a trusted LAN/VPN and keep physical access to mount power/stop controls during the first HIL tests.
