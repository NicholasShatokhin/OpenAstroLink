# OpenAstroLink v0.2.10.59 — Night Vision source checkpoint

This checkpoint adds desktop **Night Vision**, **Strict Night Mode**, and an optional black→red display palette for monochrome/raw non-debayered Live View. It also records the OAL 1.0 plan for a separate mobile Qt Quick/QML frontend.

## Desktop themes

- `View → Night vision mode → Normal / Night Vision / Strict Night Mode`.
- `Ctrl+Shift+N` cycles the three modes.
- Theme choice is persistent and is applied before the core-selection dialog.
- Strict mode maps custom graphics/overlays to red-only; ordinary Night Vision keeps semantic custom colours where useful.

## Camera preview

- Live View camera pixels are never recoloured merely because Night/Strict mode is active.
- Optional `Black→red palette for monochrome/raw preview` applies only to displays that are actually monochrome and only when Main debayer is off.
- Science FITS/RAW/SER, camera acquisition and OAL video transport remain unchanged.

## Future GUI

OAL 1.0 plans a separate mobile/touch Qt Quick/QML client on top of the same node/API/WebRTC architecture. The existing Qt Widgets OpenAstroSuite remains the expert/engineering desktop client.

## Qualification

Source/static qualification only until a fresh Windows build and UI HIL are completed. v0.2.10.58-buildfix9 remains the last fresh Windows build-qualified checkpoint.
