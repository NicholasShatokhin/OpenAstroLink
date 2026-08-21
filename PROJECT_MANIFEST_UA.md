# Маніфест проєкту — OpenAstroSuite / OpenAstroLink v0.2.9

Канонічна документація — англійська. `PROJECT_MANIFEST_UA.md` є українським дзеркалом.

- Застосунок: `OpenAstroSuite`
- Headless service: `openastrolink-node`
- Діагностика обладнання: `oal-hardware-probe`
- Core library: `oas_core`
- Протокол і нативний driver framework: `OpenAstroLink (OAL)`
- Версія: `0.2.9-zwo-stellarium-dual-optics`
- Мова: C++20
- UI: Qt 6.4+
- Обробка зображень: OpenCV 4
- REST: Qt HTTP Server
- Події: Qt WebSockets
- Асинхронне виконання: Qt Concurrent + `OperationManager`
- Native driver ABI: C ABI v2

## Нативні OAL-драйвери

`oal.qhy`, `oal.canon`, `oal.zwo.asi`, `oal.zwo.eaf`, `oal.gemini`, `oal.skywatcher` та reference `oal.simulated`.

## Сумісність

INDI, ASCOM Alpaca, LX200, OpenCV/UVC та remote OAL adapters збережені. Native OAL є пріоритетним, але INDI навмисно легко вмикається для широкого стороннього обладнання.

## Дві камери

Core має незалежні ролі `main` і `guide` та ресурси `camera` і `camera.guide`. Обидві камери можуть бути підключені одночасно, у тому числі дві камери одного виробника.

## Оптичні профілі

`TelescopeProfile` містить окремі головний і гідуючий оптичні тракти: апертуру, ефективну фокусну, геометрію камери, f-ratio та image scale, а також тип головної оптики й центральне екранування.

## Stellarium

`StellariumTelescopeServer` реалізує зовнішній Stellarium Telescope Control TCP bridge для координат монтування та GOTO. Типовий порт — `10000`. Повне керування камерою/фокусером/workflows лишається в OAL.

## Документація

Головна: `README.md`, `PROJECT_MANIFEST.md`, `docs/*.md`.
Українські дзеркала: `README_UA.md`, `PROJECT_MANIFEST_UA.md`, `docs/uk/*.md`.
