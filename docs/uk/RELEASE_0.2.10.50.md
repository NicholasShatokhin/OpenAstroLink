# OpenAstroLink v0.2.10.50 — release checkpoint

Цей реліз робить останню build-кваліфікацію та HIL-результат монтування канонічним baseline проєкту.

## Підтверджені build targets

- Windows x64 / MSVC 2022 + Ninja: підтверджений повний observatory build.
- Linux x86_64: підтверджений native observatory build, включно з managed Qt bootstrap на Ubuntu 22.04/Jammy.
- Linux ARM64 / Raspberry Pi: підтверджений повний cross-build node, hardware probe та native QHY 26.06.04, Canon EDSDK, ZWO ASI/EAF, Gemini, Sky-Watcher і EQDrive drivers.
- Raspberry Pi 5 використовує той самий generic `aarch64` target; physical Pi 5 runtime/HIL ще pending.
- macOS Apple Silicon/Intel presets та dependency bootstrap реалізовані; physical build/signing/HIL ще pending.

## Mount v9

Direct Motor Controller coordinate model v9 лишається HIL-qualified і frozen (`Axis1Sign=+1`, `Axis2Sign=-1`). У v0.2.10.50 видалено тимчасовий прихований EQDrive `maxNativeGotoDeg` / 15° driver qualification gate. Geometry equations, polarity, Home/Park conventions і transport-direction logic не змінювалися.

Operator safety лишається явною на вищих рівнях: `maxGotoSkyDeltaDeg` — Core/profile policy реальної кутової відстані по небу, а raw `mount.gotoAxes` має явний mechanical guard `maxAxisDeltaDeg`.

## Driver policy

Native OAL drivers увімкнені за замовчуванням. INDI — лише явний opt-in compatibility layer.

## Найближча Beta послідовність

1. HIL autofocus.
2. Auto-exposure HIL.
3. Scheduler HIL.
4. Mosaic HIL.
5. Polar Alignment HIL.

Smart Telescope UX лишається scope OAL 1.0.
