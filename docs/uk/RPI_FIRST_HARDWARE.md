# Перший hardware запуск на Raspberry Pi — v0.2.10.5

Канонічний документ: `../RPI_FIRST_HARDWARE.md`.

Рекомендований порядок: зібрати node, запустити hardware probe, перевірити native drivers, підключити mount, focuser, main camera, guide camera, ASTAP; після цього виконати supervised slew/capture/autofocus/polar tests. Native-only режим дозволяє перевірити незалежність від INDI; потім INDI можна ввімкнути для додаткового обладнання.

ZWO ASI/EAF у v0.2.10 додаються через офіційні SDK. Для guide camera використовується окрема роль і lock.
