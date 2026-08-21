# Raspberry Pi 4 first-hardware path — v0.2.9 native observatory pack

## Target topology

```text
OpenAstroSuite GUI (local RPi monitor or remote PC)
                    │
                    ▼
           openastrolink-node
        HTTP :8080 / WS :8090
                    │
      ┌─────────────┼───────────────┐
      │             │               │
   oal.qhy      oal.canon      oal.gemini     oal.skywatcher
      │             │               │
  QHYCCD SDK    USB serial       SynScan serial
      │             │               │
     QHY         Gemini EAF    Sky-Watcher mount
                    │
                  ASTAP

Optional compatibility sidecar:
indiserver → any additional INDI-only equipment
```

The node remains hardware owner. Closing either GUI does not stop the devices or active node operations.

## 1. Base Pi

Use 64-bit Raspberry Pi OS / Debian-family and install native dependencies:

```bash
sudo ./scripts/bootstrap_rpi_observatory.sh
```

If INDI compatibility is already needed:

```bash
sudo ./scripts/bootstrap_rpi_observatory.sh --with-indi
```

INDI can also be added later with `sudo ./scripts/enable_indi_compat.sh`.

## 2. Stage the QHY SDK

From the Windows machine holding the SDK:

```powershell
.\scripts\copy_qhy_sdk_to_rpi.ps1 -RemoteHost <RPI-IP>
```

Defaults used by the helper:

```text
C:\workspace\astro\QHYCCD_Linux
C:\workspace\astro\qhysdk\lib\libqhy.so.0.1.8
```

On the Pi:

```bash
sudo ./scripts/stage_qhy_sdk.sh /tmp/oal-qhy-sdk
```

The staging helper calls `file` and refuses a non-AArch64 library on an `aarch64` Pi. It installs headers/library under `/opt/qhyccd` but vendor udev rules still need to be installed from the vendor package if supplied.

## 3. Install ASTAP

Install `astap_cli`/ASTAP plus a compatible star database. Optional overrides:

```bash
OAL_ASTAP_EXECUTABLE=/opt/astap/astap_cli
OAL_ASTAP_DATABASE=/opt/astap/database
```

Verify a known FITS image independently before telescope HIL.

## 4. Native-only vs native+compatibility build

Native-only qualification build:

```bash
cmake --preset rpi4-native-release \
  -DQHYCCD_INCLUDE_DIR=/opt/qhyccd/include \
  -DQHYCCD_LIBRARY=/opt/qhyccd/lib/libqhy.so
cmake --build --preset rpi4-native-release -j$(nproc)
```

Recommended observatory build, where INDI is available for unrelated devices but not required for the primary telescope:

```bash
cmake --preset rpi4-observatory-release \
  -DQHYCCD_INCLUDE_DIR=/opt/qhyccd/include \
  -DQHYCCD_LIBRARY=/opt/qhyccd/lib/libqhy.so
cmake --build --preset rpi4-observatory-release -j$(nproc)
```

Compiling INDI support does not start `indiserver` and does not insert INDI into the QHY/Gemini/Sky-Watcher reference path.

## 5. Bind serial devices deterministically

First inspect:

```bash
ls -l /dev/serial/by-id/
```

Prefer stable by-id paths in `/etc/openastrolink/node.env`:

```bash
OAL_GEMINI_PORT=/dev/serial/by-id/<Gemini-EAF>
OAL_SKYWATCHER_PORT=/dev/serial/by-id/<SynScan-controller>
```

If unset, drivers actively probe available serial ports. Explicit bindings are preferred in a permanent observatory because they eliminate accidental probing of unrelated USB serial equipment.

## 6. Hardware probe

```bash
./build/rpi4-observatory/oal-hardware-probe --require-native-telescope
```

Expected structure:

```text
ASTAP: OK
Native OAL: ...
  driver oal.qhy
  driver oal.gemini
  driver oal.skywatcher
  ... QHY camera ...
  ... Gemini EAF ...
  ... Sky-Watcher ...
Native QHY: OK
Native Gemini EAF: OK
Native Sky-Watcher: OK
Native telescope pack: OK
INDI compatibility: ... or disabled/no devices
```

## 7. Gemini EAF qualification

The driver uses a persistent 9600-baud serial session and MyFocuserPro2-compatible commands. First test with the focuser mechanically away from both end stops:

```text
connect
→ status / position
→ +50 or +100 steps
→ back to original position
→ several absolute positions inside a safe narrow range
→ temperature/status polling
→ disconnect/reconnect
```

The driver enforces controller-reported `0..maxPosition`. Position is tagged `controller-reported`; after power loss the mechanical reference may require operator verification.

**HALT is deliberately capability=false in v0.2.9.** Until the exact stop command is HIL-verified on the target Gemini firmware, do not start a large unattended focuser move. Autofocus can be tested once small moves and range semantics are confirmed.

## 8. Sky-Watcher qualification

This profile talks directly to the SynScan hand controller at 9600 8N1. It uses precise RA/DEC commands and assumes J2000 coordinates after alignment.

Safe first sequence:

```text
connect
→ read model/firmware/alignment
→ read RA/DEC
→ tracking OFF/ON
→ tiny safe GOTO
→ ABORT during another tiny GOTO
→ sync on a known solved field
→ pulse-guide only after equatorial model is confirmed
```

RA/DEC GOTO is rejected while SynScan reports alignment incomplete.

`park` is not advertised because SynScan serial protocol v3.3 has no normative park command. Do not interpret the GUI park control as available until capability-driven GUI handling is completed.

The separate `motor_controller_protocol.h` is only groundwork for a future direct-axis profile. Sky-Watcher publishes that lower-level protocol, including Wi-Fi UDP 11880, but raw axis control is not exposed as RA/DEC GOTO until OAL owns a calibrated alignment/coordinate model.

## 9. QHY qualification

1. exact hardware ID survives reconnect/reboot;
2. 0.1 s / 1 s / 10 s exposures;
3. verify dimensions and bit depth;
4. ROI/binning/gain/offset;
5. cancel long exposure;
6. 50–100 repeated frames;
7. only then autofocus with Gemini.

## 10. Enable INDI for additional equipment

INDI is not removed. For a filter wheel, weather station, dome or any device lacking a native OAL driver:

```bash
sudo ./scripts/enable_indi_compat.sh
sudo ./scripts/install_indi_service.sh <driver1> [driver2 ...]
```

Use `rpi4-observatory-release` or `-DOAS_ENABLE_INDI=ON`. Native telescope devices stay native; only the additional device travels through the INDI compatibility adapter.

## 11. Install the node

```bash
sudo ./scripts/install_rpi_node.sh build/rpi4-observatory
```

Native drivers/manifests are installed to `/usr/local/lib/openastrolink/drivers` and discovered automatically.

## 12. Current boundaries

- all three native hardware paths require real HIL qualification on the exact devices;
- Gemini hardware halt is intentionally unavailable pending verification;
- Sky-Watcher native park is unavailable in the SynScan-v3.3 profile;
- direct Motor Controller axis driver is not yet a sky-coordinate driver;
- QHY continuous planetary streaming/SER remains pending;
- ASTAP solve is not yet an async operation;
- automatic closed-loop GOTO/recenter and automatic polar wizard remain next increments;
- current remote deployment is trusted LAN/VPN only until TLS/auth/safety P0 lands.


## Canon EOS native path

Install/runtime dependency: `libgphoto2-dev` (bootstrap does this) plus the distro libgphoto2 udev rules. The plugin is `oal.canon`; no INDI process is involved. Keep desktop photo-import services from auto-claiming the USB camera. The raw/original spool defaults to `/var/lib/openastrolink/captures/canon`, created by `install_rpi_node.sh`. Run `oal-hardware-probe --require-native-observatory` to require Canon together with QHY/Gemini/Sky-Watcher.

## Additional v0.2.9 HIL steps

After validating the main camera, connect a distinct guide camera and confirm both remain connected across GUI reconnect and node restart. Verify one main exposure and one guide exposure can coexist without sharing a resource lock. For ZWO, validate ASI exposure/abort/ROI/binning and EAF move/halt/status/temperature. Finally connect Stellarium to TCP 10000 and verify position feedback and a small safe GOTO.
