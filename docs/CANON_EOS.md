# Native Canon EOS driver — `oal.canon`

## Role in OpenAstroLink

`oal.canon` is a native OpenAstroLink ABI-v2 camera driver. It does not pass camera control through INDI, ASCOM/Alpaca or LX200. On Raspberry Pi/Linux the current transport implementation links `libgphoto2` directly and uses its Canon/PTP2 USB support as the low-level hardware-access library.

This is the same architectural rule used by `oal.qhy`: a vendor/protocol library may sit below a native OAL driver, but capabilities, operations, cancellation, events and frame semantics belong to OAL.

Canon also publishes EDSDK for supported models and states that EDSDK supports Raspberry Pi OS/Linux. A future EDSDK transport can therefore be added under the same `oal.canon` device/capability contract without changing OAL clients.

## Implemented in v0.2.9

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
