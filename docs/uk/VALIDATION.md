# Валідація — v0.2.10 cross-platform desktop observatory

Канонічна версія: `docs/VALIDATION.md`.

Валідація розділяє static/source checks, SDK API-shape checks, simulator tests, реальні platform builds та hardware-in-the-loop. Сам факт компіляції не означає production-ready driver.

## Static regression

Потрібно запускати всі `tools/*check.py`, включно з `cross_platform_build_check.py` та `canon_edsdk_compile_check.py`, перевіряти JSON presets, `cmake --list-presets`, shell syntax і OpenAPI.

## Windows x64 / MSVC 2022

1. `windows-core-release` — Qt/OpenCV без vendor SDK;
2. `windows-native-release` — QHY + ZWO ASI/EAF + Canon EDSDK;
3. `windows-observatory-release` — те саме + INDI compatibility;
4. `package_windows.ps1` — self-contained Qt/OpenCV package з явно вказаними vendor runtime DLL;
5. package має запускатися поза Qt Creator без ручного редагування `PATH`.

## Linux x86_64

1. `linux-native-release` — native hardware, INDI off;
2. `linux-observatory-release` — native + INDI;
3. `linux-node-release` — headless;
4. `cmake --install` і `package_linux.sh`;
5. після встановлення vendor runtimes `ldd` не має показувати unresolved dependencies.

## Linux ARM64 / RPi

Ті самі gates із ARM64/AArch64 SDK. Перед linking обов'язково перевіряти vendor libraries командою `file`.

## Canon

На Linux native Canon за замовчуванням використовує libgphoto2/PTP. На Windows — Canon EDSDK.

`oal_driver_canon_edsdk.cpp` проходить dependency-light API-shape compile через `tests/stubs/edsdk/` з warnings-as-errors. Але потрібен реальний Windows HIL із фактичним EDSDK та конкретною EOS: enumeration, session, Bulb, cancel, CR2/CR3 transfer, thumbnail preview, reconnect.

## HIL для observatory hardware

На кожній ОС, яка напряму володітиме обладнанням, перевірити QHY, Canon, ZWO ASI (у тому числі main+guide одночасно), ZWO EAF/Gemini, Sky-Watcher, ASTAP, local+remote GUI та Stellarium. Окремо зупинити `indiserver` і довести, що native telescope продовжує працювати; потім увімкнути INDI і підключити compatibility-only device паралельно.

## Межі v0.2.10

EDSDK transport та частина native hardware drivers ще HIL-pending. Planetary SER/data plane, production guiding, durable sessions, security/safety, RFC 9457/idempotency/replay та out-of-process sandbox ще не завершені.
