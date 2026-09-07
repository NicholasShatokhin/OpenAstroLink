# План валідації — v0.2.10.55

## Найближчі high-rate / Dual Live HIL gates

1. Main QHY/ASI Live View з короткою exposure: перевірити measured capture rate >60 FPS, якщо hardware дозволяє.
2. Preview 15/30/60/MAX під час SER: capture/record FPS не повинні суттєво змінюватися, `recordDropped` має бути 0.
3. 5-10 хв high-rate SER і відкриття в AutoStakkert; frame count має збігатися з OAL telemetry/sidecar.
4. Одночасний main + guide Live View; незалежно змінити exposure/gain/preview rate на кожній camera і перевірити, що друга не страждає.
5. Створити slow-preview load і переконатися, що росте preview drop, але не record drop.
6. Mount/focuser commands та `/events` мають лишатися responsive при навантаженому `/video`.
7. Stop/Cancel має коректно finalize SER і звільняти обидва camera resource locks.

## Repeat HIL з v0.2.10.54

- 30-50 швидких direct-Wi-Fi press/release зі зміною напрямку; без ignored press і без продовження руху після release.
- Scene autofocus біля true focus і з навмисним offset у межах range; потрібне repeatable verified improvement.
- Sparse daylight auto-exposure на сцені, де раніше був цикл ~0.64/1.43 с; потрібен stable lock.
- Night star-field/HFR autofocus ще pending.

## Regression invariants

- Mount geometry v9 не змінюється camera-stream роботою.
- Science FITS/RAW та SER залишаються raw/undebayered.
- Preview можна пропускати; recording — ні.
- INDI залишається opt-in.
