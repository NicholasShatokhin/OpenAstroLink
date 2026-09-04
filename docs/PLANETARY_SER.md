# Planetary SER recording and autonomous blocks — v0.2.10.47


> **Current release:** v0.2.10.50. Planetary SER remains implemented; real-hardware workflow hardening continues under the Beta HIL plan.

`planetary-ser` is now executable inside an `ObservationPlan`, not only a manual Live View recorder. The supervised block flow is `GOTO → full-frame planet detection → optional planetary autofocus → reacquire → hardware ROI → finite SER`. QHY and ZWO ASI native streaming accept the ROI. During a run, a fast loop may shift the same-size ROI to follow the target; every change is written to `<basename>.roi.jsonl` with the first affected frame number. An optional slow mount loop can calibrate the local RA/DEC-to-image response with small slews and perform bounded recenter corrections. Slow mount correction defaults OFF until HIL-qualified; ROI-only tracking remains the safe default.

OpenAstroLink v0.2.10.47 can record Live View directly to uncompressed SER for lunar, planetary and lucky-imaging workflows.

- Recording is optional and controlled from **Live / Finder**.
- Frames are written **before** preview debayer and auto-stretch. Bayer/mono camera samples therefore remain raw in the SER file.
- Preview debayer, crosshair, Mil-Dot and angular overlays are display-only and are never burned into SER frames.
- The writer supports 8/16-bit mono/Bayer and BGR streams. A stream geometry or pixel-format change closes the current recording with an error rather than silently producing a mixed-format file.
- Per-frame UTC timestamps are appended in the SER timestamp trailer.
- Default output is `Pictures/OpenAstroLink/SER/Live_<UTC>.ser` when no path is entered.
- A same-basename `.txt` sidecar is finalized beside each SER. It records requested and first-frame actual exposure/gain/offset/binning, target/measured FPS, dimensions/bit depth/SER color ID, CFA/Bayer state, optical-train values, site coordinates, UTC start/end/duration and timestamp-trailer presence. This is intentionally human-readable and FireCapture-like rather than hiding acquisition provenance only inside the SER header.

For QHY/ASI, use native streaming for planetary capture. Science FITS still-image capture remains a separate workflow.
