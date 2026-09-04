# Live View, Scene Autofocus, and Finder Alignment


> **Current release:** v0.2.10.50. The Live View/finder/scene-autofocus workflow described here remains implemented; nearest-Beta work is HIL convergence and repeatability.

OpenAstroLink v0.2.10.35 includes an operational preview workflow intended for telescope setup, target acquisition, finder alignment, lunar/planetary framing, and daytime focusing.

## Live View operation

`POST /api/v1/cameras/main/live-view` starts a cancellable `camera.live-view` operation. The operation owns the main-camera resource until cancelled, repeatedly acquires short frames, and publishes them through the normal `frameReady` WebSocket + preview HTTP path.

Live frames are **preview-only**:

- `saveRaw=false` is forced;
- QHY/ASI-style frames are not spooled to FITS;
- they update the in-memory preview cache and the GUI;
- the daylight-safe default GUI profile is 1 ms, gain 0, 2x2 bin, target 5 fps.

The actual frame rate is bounded by exposure, camera readout, PNG preview encoding, network latency, and the selected binning.

For native QHY cameras, v0.2.10.34 uses the QHYCCD continuous-stream API (`BeginQHYCCDLive` / `GetQHYCCDLiveFrame` / `StopQHYCCDLive`) and restores single-frame mode when Live View stops. Health probing is suspended while the camera resource is locked by Live View or autofocus.

## Science files and remote GUI

Live View and autofocus acquisition frames are intentionally preview-only. A normal user **Capture** can request science preservation. In v0.2.10.34 the remote GUI forwards `saveRaw` and optional `savePath` to the node; native host-frame cameras such as QHY are therefore written by the node as FITS under the configured/default science spool, while Canon continues to preserve its camera-original RAW file.

### Canon DSLR safety

v0.2.10.33 intentionally rejects Live View on the native Canon EDSDK still-capture path. Repeating CR2 captures at several frames per second would actuate the mechanical shutter and is not an acceptable substitute for EVF. A later Canon driver revision should implement the EDSDK EVF output-device/EVF-image transport.

## Live / Finder GUI

The **Live / Finder** tab provides:

- exposure, gain, binning and target-FPS controls;
- automatic preview stretch;
- a camera-center crosshair;
- brightest-region detection and an approximate pixel offset from image center;
- one-click Scene autofocus using the Live View camera settings;
- a five-step Finder Alignment wizard.

Bright-region detection is an acquisition aid, not an astrometric centroid. It uses a low-resolution block-energy search designed to ignore isolated hot pixels better than a single brightest-pixel test.

## Scene autofocus

`AutofocusMode::Scene` focuses on structured non-stellar images using image-gradient energy. Typical targets include:

- a distant antenna or mast during daytime setup;
- lunar surface features;
- a planetary disk when star PSFs are unavailable;
- other sufficiently textured optical targets.

Autofocus exposure and gain are now explicit request parameters rather than fixed at 50 ms / gain 0.

Star autofocus remains PSF-oriented. It now requires at least `minStars` suitable stars per focus frame. If this criterion is never met, the operation fails with a clear `No suitable stars detected` message instead of returning a noise-derived focus position.


## Optional camera-neutral debayer (v0.2.10.35)

Live View can optionally software-debayer a one-channel CFA preview. This is **preview-only**: raw sensor pixels and science FITS/RAW files are never modified.

- `AUTO` uses `bayerPattern` metadata published by the active native driver.
- QHY derives the sequence from the SDK `CAM_COLOR` result; this supports different QHY color models rather than hard-coding one camera.
- ZWO ASI derives the sequence from `ASI_CAMERA_INFO::BayerPattern`.
- RGGB, BGGR, GRBG and GBRG can be selected explicitly as a vendor-neutral fallback for other RAW/Bayer cameras.
- Debayer requires the native 1x1 CFA lattice, so Live View forces 1x1 when software debayer is enabled.
- Already-RGB camera previews pass through unchanged.

Saturated and underexposed frames remain valid camera frames. The GUI may show an exposure-quality warning, but this does not indicate a transport failure or device disconnect.

## Autofocus visual feedback (v0.2.10.35)

Autofocus now publishes one preview frame per sampled focuser position through the normal operational preview path. These frames are displayed in the main camera pane but do not replace the last user science frame. The Focus tab also provides manual coarse/fine jog buttons plus STOP.

Scene autofocus keeps the coarse peak when a later fine scan is weaker, rejects nearly flat curves, and can extend a scan whose best sample lies on a boundary. This makes daylight focusing on terrestrial texture more repeatable.

## Finder-alignment workflow

1. Point the telescope at a distant, contrast-rich terrestrial target and start Live View.
2. Stop Live View and run Scene autofocus. Restart Live View when focusing completes.
3. Move the telescope until a recognizable detail is exactly under the camera crosshair.
4. Do not move the telescope. Adjust only the finder-scope screws until the same detail is centered in the finder.
5. Verify by moving away and returning. At night, refine on a bright star and run Star autofocus at infinity.

Never point an unfiltered telescope or camera at the Sun.

## Future acquisition work

The v0.2.10.33 brightest-region detector is groundwork for an `Acquire bright target` workflow. Automatic mount centering and spiral search remain deliberately disabled until the native mount coordinate/sign/tracking model completes hardware qualification.
