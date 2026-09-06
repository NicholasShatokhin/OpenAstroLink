# Оновлення стану — v0.2.10.53


## v0.2.10.53 — Sky Map camera footprints і Stellarium framing

- Plate solve тепер публікує stateful measured `lastSolvedFrame` geometry (J2000 center, angular size, PA, solved image dimensions).
- Sky Map окремо малює measured solved, predicted main і predicted guide camera rectangles.
- Mosaic planner overlay використовує Scheduler rows/columns/overlap/rotation; main PA синхронізований із Scheduler.
- Один вибраний OAL footprint можна передати в optional Stellarium Remote Control rectangular FOV marker; Telescope Control TCP лишається position/GOTO only.
- Mount direct-MC v9 geometry не змінювалася.

## v0.2.10.52 — Sky Map free-point targeting

Offline Sky Map тепер дозволяє вибрати довільну видиму sky position, а не лише catalogue entries. Click усередині horizon circle інвертується у Az/Alt і один раз переводиться у J2000 через existing coordinate path; selection показується жовтим marker `Target`. Slew, Sync, double-click GOTO і transfer у Scheduler використовують той самий selected-coordinate contract. Catalogue targets мають click priority. Користувач уже підтвердив перенесення catalogue-object coordinate із Sky Map у Scheduler у працюючому GUI. Direct-MC mount geometry v9 лишається frozen і не змінена.


**Build foundation:** Windows x64 ✅, Linux x86_64 ✅, Raspberry Pi/Linux ARM64 cross node+probe+native drivers ✅, macOS presets/bootstrap 🟡 physical build pending. Native OAL drivers — default; INDI — opt-in.

**Mount:** direct-MC coordinate model v9 HIL-підтверджений і frozen. Тимчасовий driver-level 15°/`maxNativeGotoDeg` qualification gate видалений. Core/profile sky-safety лишається user-controlled; raw-axis motion має явний mechanical guard.

**Наступна Beta-кваліфікація:** HIL autofocus → auto-exposure → scheduler → mosaic → Polar Alignment. Smart Telescope UX — OAL 1.0.

> Канонічне українське дзеркало повного status-документа: [`docs/uk/STATUS.md`](uk/STATUS.md).

Цей файл лишається короткою compatibility-точкою для старих посилань.
