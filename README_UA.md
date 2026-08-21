# OpenAstroSuite / OpenAstroLink

**v0.2.10 — кросплатформна desktop observatory збірка (Windows x64, Linux x86_64, Linux ARM64/RPi)**

Канонічна документація проєкту ведеться англійською. Цей файл і `docs/uk/` є українськими дзеркальними версіями.

OpenAstroLink (OAL) — сучасний local-first стек керування обсерваторією. Нативні OAL-драйвери є основним шляхом до обладнання; INDI, ASCOM Alpaca та LX200 залишаються опційними шарами сумісності для пристроїв, які ще не мають нативного OAL-драйвера.

## Архітектура

```text
OpenAstroSuite GUI
  ├─ локально на observatory host (Windows/Linux/RPi)
  └─ віддалено на іншому комп'ютері
              │
        OAL HTTP / WebSocket
              │
      openastrolink-node
              │
      AstroCore / operations
              │
      Native OAL ABI v2
      ├─ oal.qhy          → QHYCCD SDK → QHY
      ├─ oal.canon        → Canon EDSDK (Windows) / USB/PTP-libgphoto2 (Linux) → Canon EOS
      ├─ oal.zwo.asi      → ZWO ASI SDK → ZWO ASI
      ├─ oal.zwo.eaf      → ZWO EAF SDK → ZWO EAF
      ├─ oal.gemini       → USB serial → Gemini EAF
      └─ oal.skywatcher   → SynScan serial → Sky-Watcher

За потреби паралельно:
      INDI / ASCOM Alpaca / LX200
```

Node володіє обладнанням та тривалими операціями. Закриття GUI не зупиняє node і не від'єднує пристрої.

## Нове у v0.2.10

- Нативний `oal.zwo.asi`: discovery, capabilities, exposure, ROI, binning, gain/offset, cancel, native frame publication.
- Нативний `oal.zwo.eaf`: absolute/relative move, halt, position/moving state, temperature, backlash/reverse/max-step.
- Дві незалежні ролі камер: `main` та `guide`; одночасно можуть працювати камери різних виробників і backend-ів.
- Незалежні locks `camera` і `camera.guide`.
- Окремі профілі головного та гідуючого оптичних трактів: апертура, ефективна фокусна, розмір пікселя, сенсор, f-ratio та image scale.
- Тип головної оптики й центральне екранування.
- TCP-міст Stellarium Telescope Control, типовий порт `10000`, для положення монтування та GOTO.
- Міст Stellarium можна ввімкнути локально або віддалено через OAL.
- Англійська документація є головною, українська підтримується як дзеркало.

## Дві камери

Типова схема:

```text
Main:  telescope → QHY / Canon EOS / ZWO ASI
Guide: guide scope або OAG → ZWO ASI / QHY / інша OAL/INDI camera
```

Registry позначає роль камери `main` або `guide`. Основна зйомка використовує lock `camera`, гід-кадр — `camera.guide`. Повний production guider із calibration/dither/star-loss recovery ще треба завершити; v0.2.9 створює правильну основу для двох незалежних камер.

## Stellarium

Міст вмикається на вкладці **OAL Server → Stellarium Telescope Control bridge** або так:

```bash
openastrolink-node --http-port 8080 --ws-port 8090 --stellarium-port 10000
```

У Stellarium треба налаштувати external telescope server на IP node і TCP-порт `10000`.

Стандартний протокол Stellarium керує саме монтуванням: OAL передає координати та приймає GOTO. Камера, фокусер, autofocus, polar alignment, guiding і sessions продовжують керуватися через OAL. Для повного керування прямо всередині Stellarium надалі можна зробити спеціальний OAL plug-in.

## Параметри оптики

Для головної труби задаються апертура/діаметр головного дзеркала, ефективна фокусна відстань з урахуванням reducer/Barlow, тип оптики, за потреби центральне екранування, розмір пікселя та розмір сенсора основної камери. Для гід-труби — окремі апертура, фокусна та параметри гід-камери.

```text
f-ratio = focal_length / aperture
image_scale_arcsec_per_pixel = 206.265 × pixel_size_um / focal_length_mm
```

## INDI

INDI залишається дуже легко ввімкнути для широкої підтримки стороннього обладнання. Він не є залежністю нативних OAL-драйверів і не обмежує модель можливостей OAL.

Поточний стан та перевірки: `docs/uk/STATUS.md`, `docs/uk/VALIDATION.md`, `docs/uk/ZWO_NATIVE.md`, `docs/uk/STELLARIUM.md`, `docs/uk/OPTICAL_TRAINS_AND_DUAL_CAMERAS.md`.

## v0.2.10: Windows/Linux як observatory node

RPi більше не обов'язковий. Якщо обладнання під'єднане прямо до Windows x64 або Linux x86_64 комп'ютера, цей комп'ютер може запускати `openastrolink-node` і локальний GUI; інший GUI та Stellarium можуть підключатися віддалено.

Portable build presets охоплюють Windows, desktop Linux, headless Linux і RPi. Локальні Qt/OpenCV/vendor SDK paths зберігаються у `CMakeUserPresets.json`, який ігнорується Git. Native Canon використовує EDSDK на Windows та libgphoto2/PTP на Linux за `AUTO` policy. Деталі: `docs/uk/BUILD_PLATFORMS.md`.

### v0.2.10.2 — виправлення ізоляції QHY headers для MSVC

Windows-ціль QHY більше не додає каталог `include` QHY SDK у глобальний шлях пошуку заголовків MSVC. Деякі версії QHY All-In-One містять сумісні заголовки `stdint.h` / `stdint_windows.h`; через `/I` вони могли підміняти системний `<stdint.h>` і ламати `std::int64_t`, `std::chrono` та інші типи STL. OAL тепер генерує приватний wrapper з абсолютним шляхом до `qhyccd.h`, не дозволяючи vendor headers затіняти CRT/STL headers.
