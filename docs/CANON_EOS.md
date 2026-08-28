# Native Canon EOS driver — `oal.canon`

## Role in OpenAstroLink

`oal.canon` is a native OpenAstroLink ABI-v2 camera driver. It does not pass camera control through INDI, ASCOM/Alpaca or LX200. On Raspberry Pi/Linux the current transport implementation links `libgphoto2` directly and uses its Canon/PTP2 USB support as the low-level hardware-access library.

This is the same architectural rule used by `oal.qhy`: a vendor/protocol library may sit below a native OAL driver, but capabilities, operations, cancellation, events and frame semantics belong to OAL.

Canon also publishes EDSDK for supported models and states that EDSDK supports Raspberry Pi OS/Linux. A future EDSDK transport can therefore be added under the same `oal.canon` device/capability contract without changing OAL clients.

## Implemented in v0.2.10

- USB/PTP discovery through libgphoto2.
- Canon/EOS filtering, including generic PTP devices when the probed manufacturer reports Canon.
- Best-effort serial-number discovery; device IDs prefer the camera serial when available.
- Exact model/port binding for connection.
- Native `device.connect` / `device.disconnect`.
- Exposure request mapping to the closest camera `shutterspeed` choice.
- Long exposure through Canon Bulb controls. The driver probes the camera's actual `eosremoterelease` choices and supports both older labels (`Immediate` / `Release Full`) and newer libgphoto2 Canon labels such as `Press Full MF` / `Release`; it falls back to legacy `bulb` when exposed by the camera.
- Cooperative Bulb cancellation.
- OAL `gain` mapped to nearest camera ISO choice.
- Download of the original camera file.
- Automatic original-file spooling for RAW/non-JPEG captures; `saveRaw=true` also spools JPEG. `savePath` overrides the destination. Default spool: `/var/lib/openastrolink/captures/canon`.
- JPEG decode with libjpeg; if the captured file is RAW/CR2/CR3, the driver asks libgphoto2 for the captured file preview and publishes that RGB8 preview through `publishFrame()` for autofocus/plate solve.
- Optional deletion of the downloaded file from the camera (manifest config `deleteFromCamera`).
- Typed capabilities for shutter choices, Bulb, ISO, image format, raw spooling, unsupported ROI/binning.
- Health and `camera.frameReady` events.

## Intentional limits

- DSLR sensor ROI/binning is not advertised.
- Short non-Bulb captures may be uninterruptible inside the camera/libgphoto2 call; abort is strongest for Bulb mode and capabilities advertise `abortMode=cooperative-bulb-best-effort-short`.
- The driver does not silently convert RAW into science pixels. The original RAW/CR2/CR3 is the science artifact; the OAL in-memory frame is a JPEG/embedded preview intended for autofocus, solving and UI preview. The durable science-file/data-plane API is still a P0/P1 item.
- Live View/video is not yet implemented in this driver.
- Exact setting names/choices vary across EOS generations. Capabilities are probed from the connected camera rather than assumed from the model name.

## Raspberry Pi build

`rpi4-native-release` and `rpi4-observatory-release` enable `OAS_ENABLE_NATIVE_CANON=ON`.

```bash
sudo ./scripts/bootstrap_rpi_observatory.sh --with-indi  # INDI optional
cmake --preset rpi4-observatory-release \
  -DQHYCCD_INCLUDE_DIR=/opt/qhyccd/include \
  -DQHYCCD_LIBRARY=/opt/qhyccd/lib/libqhy.so
cmake --build --preset rpi4-observatory-release -j$(nproc)
```

Bootstrap installs `libgphoto2-dev` and libjpeg development files. `install_rpi_node.sh` creates the default Canon capture spool owned by the OAL service user.

## First HIL qualification

1. Close any desktop photo importer that auto-opens the EOS camera.
2. Put the EOS in a remote-controllable still-photo mode; for Bulb tests use Manual mode where required by the model.
3. Run:
   ```bash
   ./build/rpi4-observatory/oal-hardware-probe --require-native-observatory
   ```
4. Confirm a `driver oal.canon` and a Canon camera device appear.
5. Connect only Canon first; query native capabilities.
6. Capture 1 s JPEG, then 5 s, then a sequence of 20 short frames.
7. Enable RAW+JPEG (preferred initial HIL mode) and capture with `saveRaw=true`; verify the original file is present in `/var/lib/openastrolink/captures/canon` and the preview reaches the GUI.
8. Test 35–60 s Bulb, then cancel a Bulb exposure after several seconds.
9. Disconnect/reconnect USB and ensure the node remains alive and the camera can be rediscovered.
10. Only after this pass use Canon in autofocus/ASTAP or unattended sequences.

## Why libgphoto2 is not the compatibility layer here

The old `canon-gphoto2` object under `src/backends/` remains an optional legacy in-core compatibility implementation controlled by `OAS_ENABLE_GPHOTO2`. The new `drivers/canon/` target is a separately discovered ABI-v2 OAL driver. It uses libgphoto2 only as a linked USB/PTP implementation library and exposes no gphoto2 API to the core or GUI.

## v0.2.10 transport selection

Native Canon is now a cross-platform OAL driver with a build-time transport selection:

- `OAS_CANON_TRANSPORT=AUTO` → **EDSDK on Windows**, **GPHOTO2 on Linux**;
- `OAS_CANON_TRANSPORT=EDSDK` → Canon EDSDK explicitly;
- `OAS_CANON_TRANSPORT=GPHOTO2` → libgphoto2 explicitly.

The OAL-facing driver id remains `oal.canon`, so GUI/core/session semantics do not depend on the low-level transport.

### EDSDK path

The Windows-native implementation enumerates EOS bodies with EDSDK, opens a camera session, saves the original transferred file to the OAL capture spool, and publishes the EDSDK thumbnail as the operational preview frame. Bulb is used for timed astronomy exposures where the body/mode supports it; the camera's current release mode is used as a fallback. Cancellation ends Bulb best-effort.

Required CMake variables:

```text
CANON_EDSDK_INCLUDE_DIR
CANON_EDSDK_LIBRARY
CANON_EDSDK_RUNTIME_DIR   # packaging hint
```

This transport is source/API-shape implemented but remains **hardware-HIL pending** until tested with the exact EOS model and current Canon SDK runtime.

### Linux path

The existing libgphoto2/PTP implementation remains the default Linux transport. It is still a native OAL driver: libgphoto2 is only the hardware access layer, not an INDI/ASCOM backend.

## v0.2.10.26 — automatic EDSDK hot-plug and ISO gain

- The EDSDK `camera-added` callback now emits an OAL `device.discoveryHint`; the application converts it into a driver-scoped asynchronous `oal.canon` rediscovery. On the node, the existing `nativeDiscoveryCompleted` restore path reconnects a persisted auto-connect Canon binding without requiring the global Refresh button.
- OAL camera `gain` on the Windows EDSDK transport now means **ISO**, matching the Linux/libgphoto2 Canon transport. `gain=0` preserves the body's current ISO; positive values are mapped to the nearest ISO advertised by `kEdsPropID_ISOSpeed`. A requested positive ISO that cannot be applied fails explicitly instead of silently capturing at the old ISO.
- Canon RAW capture still preserves the original CR2/CR3 as the science artifact. The in-memory operational frame is the EDSDK/embedded JPEG preview and is the current input to UI preview, autofocus and plate solving. For the tested 550D CR2 path the embedded JPEG is 5184×3456, i.e. full spatial resolution but not 14-bit RAW science pixels.
- Windows default original-file spool remains `%USERPROFILE%\Pictures\OpenAstroLink\Canon`; each capture log includes `original=...`.

## v0.2.10.25 — EOS 550D hot-plug, RAW preview and Bulb compatibility

Follow-up Windows HIL on a Canon EOS 550D established three additional facts:

- The body can emit the EDSDK camera-added callback before `EdsGetCameraList()` exposes it. An explicit **Refresh all native devices** now gives a pending camera-added edge a bounded ~1.2 s settle/retry window, and the edge is not cleared until a camera is actually enumerable. If enumeration still returns zero, the existing explicit-refresh hard driver/EDSDK reload remains the final fallback. There is still no periodic Canon scan.
- Captures from 1–30 s were physically taken and the original files were transferred successfully, but this EOS/RAW combination returned no `EdsDownloadThumbnail()` data. Missing thumbnail data no longer converts a successful exposure into a failed exposure. OAL first tries the EDSDK thumbnail, then a directly decodable JPEG original, then extracts the largest decodable embedded JPEG preview from CR2 while preserving the RAW file untouched. The selected preview source is logged and recorded in frame metadata.
- `EdsSendCommand(BulbStart)` returned EDSDK `0x60` (`EDS_ERR_INVALID_PARAMETER`) on the EOS 550D. Long exposure now prefers the more compatible `Tv=Bulb` + held `PressShutterButton(Completely_NonAF)` + `ShutterButton_OFF` sequence. `BulbStart/BulbEnd` remains only a compatibility fallback for bodies that reject the shutter-hold path.

The normal short-exposure policy remains non-AF: requested exposures up to 30 s are mapped to the nearest supported `kEdsPropID_Tv` value and released with `Completely_NonAF`. Requested and actual exposure times are included in metadata.

The Devices UI now deliberately distinguishes:

- **Apply port & rediscover selected serial driver** — Gemini/Sky-Watcher/EQDrive only.
- **Refresh all native devices (USB / serial)** — vendor-neutral discovery for Canon, QHY, ZWO, serial devices, etc.

Recommended EOS 550D HIL sequence:

1. Start the node without the EOS, attach it, wait for the `camera-added event received` line, then click **Refresh all native devices (USB / serial)** once.
2. Confirm `Canon EDSDK discovery scan sees 1 camera(s)` and that `native:oal.canon/...` appears without restarting the node.
3. Capture 1 s and 10 s. Confirm an original CR2/JPEG is stored and the GUI receives a preview. The node should log `Canon preview source=...`.
4. Capture 45 s. With the body in a Bulb-capable Manual mode, the node should log `Canon Bulb transport=held-non-af-shutter` on EOS 550D.
5. Cancel one long exposure and verify the shutter is released immediately.
6. Run a 20-frame short-exposure sequence to qualify repeated transfer/event stability.

