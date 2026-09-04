# Оновлення стану — v0.2.10.51

## v0.2.10.51 — Sky Map MVP

OpenAstroSuite тепер має lightweight offline Sky Map у лівій робочій області. Вона рендерить horizon/all-sky view через existing observer/time coordinate path, має bright-star/DSO search, pan/zoom, live telescope і plate-solve markers, approximate main-camera FOV та controller-backed Slew/Sync/Abort/Park і transfer target у Scheduler. Mount geometry не змінена; direct-MC v9 лишається frozen.


**Build foundation:** Windows x64 ✅, Linux x86_64 ✅, Raspberry Pi/Linux ARM64 cross node+probe+native drivers ✅, macOS presets/bootstrap 🟡 physical build pending. Native OAL drivers — default; INDI — opt-in.

**Mount:** direct-MC coordinate model v9 HIL-підтверджений і frozen. Тимчасовий driver-level 15°/`maxNativeGotoDeg` qualification gate видалений. Core/profile sky-safety лишається user-controlled; raw-axis motion має явний mechanical guard.

**Наступна Beta-кваліфікація:** HIL autofocus → auto-exposure → scheduler → mosaic → Polar Alignment. Smart Telescope UX — OAL 1.0.

> Канонічне українське дзеркало повного status-документа: [`docs/uk/STATUS.md`](uk/STATUS.md).

Цей файл лишається короткою compatibility-точкою для старих посилань.
