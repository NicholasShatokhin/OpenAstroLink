

> Поточний synchronized snapshot: **v0.2.10.57 (2026-09-07)**. Див. `STATUS.md` та `RELEASE_0.2.10.57.md` щодо current qualification boundaries.

## High-rate camera data plane — v0.2.10.57

Live camera pixels використовують recording-first pipeline та окремий binary OALV `/video` WebSocket path; звичайний JSON control/state лишається на `/events`. Main і guide camera operations мають різні locks і можуть працювати одночасно. Preview latest-only/droppable; SER отримує frame до preview transforms. Див. `HIGH_RATE_STREAMING.md`.

## Поточна deployment policy — v0.2.10.57

Native OAL drivers — основний execution path на Windows, Linux, Raspberry Pi та macOS. INDI не входить у default runtime і вмикається лише як compatibility adapter. Windows x64, Linux x86_64 та Raspberry Pi/Linux ARM64 node builds уже підтверджені; macOS build-configured, але physical qualification ще pending.

# Архітектура OpenAstroSuite / OpenAstroLink — v0.2.10.5

Канонічний документ: `../ARCHITECTURE.md`.

## Native OAL — основний hardware path

GUI може працювати локально на RPi або віддалено. Усі алгоритми та hardware state живуть у `openastrolink-node`. Core бачить пристрої через Native OAL ABI v2; INDI, ASCOM Alpaca та LX200 є паралельними compatibility adapters і не визначають семантику OAL.

Нативний комплект включає QHY, Canon EOS, ZWO ASI, ZWO EAF, Gemini EAF і Sky-Watcher. Device registry та capabilities відокремлюють vendor transport від high-level OAL services.

## Дві камери

Core має ролі `main` та `guide`. Вони використовують різні resource locks (`camera`, `camera.guide`) та можуть бути активними одночасно.

## Інтеграції

Stellarium TCP bridge працює на node і відображає mount position/GOTO у стандартний Stellarium telescope protocol. Решта функцій — camera/focus/AF/polar/session — лишаються OAL-native.

## Process boundary

Закриття GUI не зупиняє node. Driver isolation і durable operations розвиваються окремо; safety/emergency commands не повинні залежати від GUI.
