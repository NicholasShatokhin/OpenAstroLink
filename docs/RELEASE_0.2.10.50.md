# OpenAstroLink v0.2.10.50 release checkpoint

This release turns the recent build qualification and mount HIL result into the canonical project baseline.

## Confirmed build targets

- Windows x64 / MSVC 2022 + Ninja: confirmed full observatory build.
- Linux x86_64: confirmed native observatory build, including managed Qt bootstrap on Ubuntu 22.04/Jammy.
- Linux ARM64 / Raspberry Pi: confirmed full cross-build for node, hardware probe and native QHY 26.06.04, Canon EDSDK, ZWO ASI/EAF, Gemini, Sky-Watcher and EQDrive drivers.
- Raspberry Pi 5 uses the same generic `aarch64` target; physical Pi 5 runtime/HIL is still pending.
- macOS Apple Silicon/Intel presets and dependency bootstrap are implemented; physical build/signing/HIL remain pending.

## Mount v9

The direct Motor Controller coordinate model v9 remains HIL-qualified and frozen (`Axis1Sign=+1`, `Axis2Sign=-1`). v0.2.10.50 removes the temporary hidden EQDrive `maxNativeGotoDeg` / 15° driver qualification gate. No geometry equations, polarity, Home/Park conventions or transport-direction logic were changed.

Operator safety remains explicit at higher layers: `maxGotoSkyDeltaDeg` is a Core/profile true-sky-separation policy, and raw `mount.gotoAxes` requests retain their explicit `maxAxisDeltaDeg` mechanical guard.

## Driver policy

Native OAL drivers are enabled by default. INDI is an explicit opt-in compatibility layer only.

## Nearest Beta sequence

1. HIL autofocus.
2. Auto-exposure HIL.
3. Scheduler HIL.
4. Mosaic HIL.
5. Polar Alignment HIL.

Smart Telescope UX remains OAL 1.0 scope.
