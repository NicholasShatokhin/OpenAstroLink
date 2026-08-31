# Planetary SER recording

OpenAstroLink v0.2.10.38 can record Live View directly to uncompressed SER for lunar, planetary and lucky-imaging workflows.

- Recording is optional and controlled from **Live / Finder**.
- Frames are written **before** preview debayer and auto-stretch. Bayer/mono camera samples therefore remain raw in the SER file.
- Preview debayer, crosshair, Mil-Dot and angular overlays are display-only and are never burned into SER frames.
- The writer supports 8/16-bit mono/Bayer and BGR streams. A stream geometry or pixel-format change closes the current recording with an error rather than silently producing a mixed-format file.
- Per-frame UTC timestamps are appended in the SER timestamp trailer.
- Default output is `Pictures/OpenAstroLink/SER/Live_<UTC>.ser` when no path is entered.

For QHY/ASI, use native streaming for planetary capture. Science FITS still-image capture remains a separate workflow.
