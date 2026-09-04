# OpenAstroLink v0.2.10.51 — release checkpoint

## Summary

v0.2.10.51 додає легку offline Sky Map в OpenAstroSuite для навігації телескопа без зовнішнього Stellarium. Це supervised Beta navigation surface, а не Smart Telescope UX.

## Sky Map

- Нові tabs `Imaging / Sky Map` у лівій робочій області.
- Offline horizon projection з яскравими зорями, вибраними Messier/DSO targets і кількома constellation guide lines.
- Search, pan, zoom, selection та double-click GOTO.
- Telescope marker, plate-solved center і приблизний camera FOV.
- Slew, Sync, Abort, Park/Unpark та передача target у Scheduler проходять через існуючий `ObservatoryController`.

## Frozen mount contract

Direct-MC coordinate model v9 лишається HIL-qualified і незмінним (`Axis1Sign=+1`, `Axis2Sign=-1`). Тимчасовий прихований EQDrive 15° qualification gate вже видалено у v0.2.10.50. v0.2.10.51 не змінює mount geometry, Home/Park, transport polarity або direct-MC planning.

## Build baseline

Збережено підтверджені Windows x64, native Linux x86_64 та Raspberry Pi/Linux ARM64 builds. macOS physical qualification і Pi 5 physical runtime/HIL ще pending.

## Найближча Beta

1. supervised mount sanity після зняття qualification cap;
2. HIL autofocus;
3. HIL auto-exposure;
4. scheduler HIL;
5. mosaic HIL;
6. Polar Alignment HIL;
7. Sky Map real-mount navigation smoke як частина звичайної UI qualification.
