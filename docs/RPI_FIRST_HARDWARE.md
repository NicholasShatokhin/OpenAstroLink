# Raspberry Pi 4 first-hardware path — v0.2.6 native-first

## Target topology

```text
OpenAstroSuite GUI (local RPi monitor or remote PC)
                    │
                    ▼
           openastrolink-node
        HTTP :8080 / WS :8090
          │                │
          │                └── ASTAP / astap_cli
          │
          ├── Native OAL driver registry
          │       └── oal.qhy → QHYCCD SDK → QHY camera
          │
          └── Compatibility path for hardware not yet native
                  └── INDI :7624
                       ├── mount
                       └── Gemini EAF
```

**QHY no longer requires INDI in the reference path.** INDI is retained for the mount and Gemini EAF only until verified native OAL drivers are available for those exact devices/protocols.

The node remains hardware owner. A GUI on the Pi connects to `127.0.0.1`; a remote GUI connects to the Pi over LAN/VPN.

## 1. Base Pi

Use a 64-bit Raspberry Pi OS / Debian-family image and run:

```bash
sudo ./scripts/bootstrap_rpi_observatory.sh
```

Install separately:

1. QHYCCD ARM64 SDK + udev rules. This is required to build `oal.qhy`; no INDI QHY driver is needed.
2. ASTAP/`astap_cli` + one appropriate star database.
3. Until native replacements exist, the INDI driver executable for the exact mount.
4. Until native `oal.gemini` exists, the Gemini EAF INDI driver if available for the installed Linux/firmware combination.

## 2. Configure/build native QHY

If QHYCCD SDK is installed under standard include/library paths:

```bash
cmake --preset rpi4-observatory-release
cmake --build --preset rpi4-observatory-release -j$(nproc)
```

If it lives under `/opt/qhyccd`:

```bash
cmake --preset rpi4-observatory-release -DQHYCCD_ROOT=/opt/qhyccd
cmake --build --preset rpi4-observatory-release -j$(nproc)
```

The build should contain:

```text
build/rpi4-observatory/drivers/
  oal_driver_simulated.so
  oal_driver_simulated.manifest.json
  oal_driver_qhy.so
  oal_driver_qhy.manifest.json
```

## 3. Optional INDI compatibility for mount/Gemini

Only after the exact executable names are known:

```bash
sudo ./scripts/install_indi_service.sh \
    <mount-driver-executable> \
    <gemini-driver-executable>

systemctl status openastrolink-indi
journalctl -u openastrolink-indi -f
```

Do not invent INDI device names. `oal-hardware-probe` prints the exact published names/endpoints.

## 4. ASTAP

`AstapSolver` searches `OAL_ASTAP_EXECUTABLE`, then `astap_cli`/`astap` in PATH and common install locations. Optional:

```bash
export OAL_ASTAP_EXECUTABLE=/opt/astap/astap_cli
export OAL_ASTAP_DATABASE=/path/to/astap/database
```

Verify ASTAP independently on a known FITS frame before first hardware night.

## 5. Native-first hardware probe

Run:

```bash
./build/rpi4-observatory/oal-hardware-probe
```

Expected high-level result:

```text
ASTAP: OK
Native OAL registry: OK
  driver oal.qhy
  device qhy:<exact-hardware-id> [camera]
INDI compatibility: OK
  <exact mount device>
  <exact Gemini device>
```

If mount/Gemini INDI is intentionally not installed:

```bash
./build/rpi4-observatory/oal-hardware-probe --no-indi
```

If QHY is temporarily absent:

```bash
./build/rpi4-observatory/oal-hardware-probe --no-qhy
```

## 6. Install node and native drivers

```bash
sudo ./scripts/install_rpi_node.sh build/rpi4-observatory
```

The helper installs native driver libraries/manifests into:

```text
/usr/local/lib/openastrolink/drivers
```

The loader also accepts an additional search path:

```bash
OAL_DRIVER_PATH=/some/custom/driver/directory
```

## 7. GUI selection

On the Devices tab native entries are grouped above compatibility entries:

```text
── Native OpenAstroLink ──
native:oal.qhy/qhy:<hardware-id>

── Compatibility / embedded ──
indi
ascom-alpaca
serial-lx200
...
```

A native device owns its transport; the GUI does not ask for an INDI/Alpaca endpoint for it.

## 8. First HIL qualification order

Do these gates sequentially:

1. **Native registry:** node sees `oal.simulated` and `oal.qhy`.
2. **QHY connect:** exact hardware ID persists through reconnect/reboot.
3. **QHY exposure:** 0.1 s, 1 s, 10 s, repeated sequence; verify dimensions/bit depth.
4. **QHY abort:** cancel a long exposure; operation becomes cancelled and camera remains usable.
5. **QHY ROI/bin/gain/offset:** verify against camera-supported ranges.
6. **Mount compatibility:** small slew, status, tracking, abort, sync, park only where advertised.
7. **Gemini compatibility:** absolute position, small move, halt, reconnect; autofocus requires absolute position support.
8. **ASTAP:** real-star frame repeatedly solves to plausible WCS.
9. **Combined autofocus:** native QHY + Gemini compatibility path while mount safety/control stays responsive.
10. **Remote GUI:** repeat core actions over LAN/VPN; close/reopen GUI while node retains device/operation state.

Only after these pass should the system be used for supervised first-light operation.

## 9. Native Gemini and native mount policy

They are explicitly planned, but v0.2.6 does not fabricate undocumented hardware commands. Native direct drivers will be added after obtaining a trustworthy vendor protocol specification or a validated protocol matrix from hardware captures. Until then INDI/Alpaca/LX200 remain the migration paths.

## 10. Current boundaries

- QHY continuous planetary streaming/SER is not implemented; capability reports `supported:false`.
- ASTAP solve is not yet an async cancellable operation.
- Full FITS/RAW science data persistence is pending.
- Automated closed-loop GOTO/recenter and automatic polar-alignment wizard are pending.
- TLS/auth/safety policy is incomplete: trusted LAN/VPN only.
