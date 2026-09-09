# Night Vision and Strict Night Mode

Current source checkpoint: **OpenAstroLink / OpenAstroSuite v0.2.10.59**.

OpenAstroSuite has three persistent desktop display modes under **View → Night vision mode**:

- **Normal** — the ordinary desktop palette and the existing semantic colours.
- **Night Vision** — the Qt Widgets chrome, menus, dialogs, controls, tooltips, editors and neutral plotting surfaces are converted to a low-luminance black/red palette. Camera pixels are not recoloured. Semantic custom graphics may retain their identity colours where that helps operation.
- **Strict Night Mode** — includes Night Vision and additionally maps custom GUI graphics to red-only values: Sky Map infrastructure and markers, histogram traces, astrometry/star overlays, reticles, grids, target markers, status accents and other hard-coded custom colours. The purpose is maximum protection of dark adaptation.

`Ctrl+Shift+N` cycles **Normal → Night Vision → Strict Night Mode → Normal**. The selected mode is stored in `QSettings` and is applied before the observatory-core chooser opens, preventing a bright startup dialog after an observing session has already selected a night mode.

## Live View policy

Night modes do **not** modify camera pixels by default. Display colour is part of observation/diagnostics and must not silently alter what the camera delivered.

The Live/Finder tab provides a separate persistent option:

> **Black→red palette for monochrome/raw preview (display only; Debayer must be off)**

When enabled, a preview that is actually monochrome (including a one-channel RAW/CFA frame displayed without debayer) is rendered with intensity on the red channel only. A true colour preview is left in colour. Enabling debayer disables the black→red transform for the Main preview.

This transform is presentation-only. It does **not** change FITS, RAW, SER, OALV/WebRTC transport payloads, autofocus input, histogram/controller input, or camera acquisition.

## Qualification boundary

v0.2.10.59 is a source/static checkpoint until a fresh Windows MSVC build and desktop HIL are completed. The last confirmed fresh Windows build is the v0.2.10.58-buildfix9 lineage.
