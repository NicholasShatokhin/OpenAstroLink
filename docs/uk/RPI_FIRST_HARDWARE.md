# Перший hardware запуск на Raspberry Pi — v0.2.10.5

> Поточний synchronized snapshot: **v0.2.10.58-buildfix9 (2026-09-09)**. Див. `STATUS.md`, `CURRENT_CHECKLIST.md` та `RELEASE_0.2.10.58.md` щодо current qualification boundaries.



> **Поточний development checkpoint:** v0.2.10.58-buildfix9. Raspberry Pi 4 і Pi 5 трактуються як generic Linux `aarch64` target; ARM64 node/probe/native-driver build підтверджений, physical Pi 5 runtime/HIL ще pending.

Канонічний документ: `../RPI_FIRST_HARDWARE.md`.

Рекомендований порядок: зібрати node, запустити hardware probe, перевірити native drivers, підключити mount, focuser, main camera, guide camera, ASTAP; після цього виконати supervised slew/capture/autofocus/polar tests. Native-only режим дозволяє перевірити незалежність від INDI; потім INDI можна ввімкнути для додаткового обладнання.

ZWO ASI/EAF у v0.2.10 додаються через офіційні SDK. Для guide camera використовується окрема роль і lock.
