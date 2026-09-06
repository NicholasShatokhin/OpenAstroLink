# OpenAstroLink / OpenAstroSuite v0.2.10.52

## Наведення Sky Map у довільну точку

v0.2.10.52 розширює offline Sky Map, додану у v0.2.10.51: тепер навігація телескопа не обмежена лише catalogue objects.

- Left click у порожнє місце всередині visible horizon circle створює free-point target.
- Екранна точка інвертується у horizontal Az/Alt і один раз переводиться у J2000 coordinate через existing `horizontalToEquatorial`.
- Free point показується жовтим marker `Target` та має той самий RA/DEC і Alt/Az readout, що й catalogue targets.
- **Slew**, **Sync**, double-click GOTO та **Use in Scheduler** працюють із free-point target через той самий `ObservatoryController` / active OAL mount backend path.
- Якщо cursor близько до catalogue object marker, catalogue object має пріоритет.
- Click поза horizon circle ігнорується.
- Для Scheduler free-point target отримує coordinate-bearing name, наприклад `Sky point RA 18.247h Dec +32.14°`.

Clicked direction переводиться у J2000 у момент selection. Sky Map не зберігає назавжди фіксований Alt/Az direction, тому tracking/scheduling лишаються у звичайній equatorial/inertial semantics.

## Підтверджена інтеграція

Користувач підтвердив, що coordinates catalogue object із Sky Map успішно переносяться у Scheduler у працюючому OpenAstroSuite GUI. Free-point implementation використовує той самий selected-coordinate path і потребує лише короткого UI/HIL підтвердження.

## Frozen mount contract

Цей реліз не змінює direct-MC mount geometry, v9 signs, Home/Park convention, serial/Wi-Fi parity, EQDrive transport logic або mount safety policy.
