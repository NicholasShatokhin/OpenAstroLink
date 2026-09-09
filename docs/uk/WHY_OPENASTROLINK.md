# Навіщо існує OpenAstroLink

> Канонічне публічне пояснення призначення OpenAstroLink / OpenAstroSuite. Поточний development checkpoint: **v0.2.10.58-buildfix9**.

## Що таке OpenAstroLink

OpenAstroLink (OAL) — це відкритий control layer і протокол для астрономічного обладнання та observing workflows. `openastrolink-node` володіє hardware sessions і довготривалими операціями; OpenAstroSuite та сторонні клієнти використовують OAL HTTP/event/streaming interfaces замість прямого керування vendor SDK з GUI.

Поточний проєкт охоплює native cameras, mounts і focusers; plate solving; autofocus; high-rate planetary capture; scheduler; mosaic execution; foundation для Polar Alignment; Stellarium integration та compatibility paths ASCOM/Alpaca/LX200/INDI там, де вони корисні.

## Навіщо його було створено

Вихідна практична вимога була простою: одна observatory stack повинна працювати локально на Windows або Raspberry Pi, керуватися віддалено без перенесення ownership обладнання в GUI та продовжувати виконання операції без необхідності, щоб інтерфейс користувача безперервно робив кожен device call.

Звідси з'явилися архітектурні вимоги:

1. **Обладнанням володіє node.** GUI — клієнт, а не lifetime owner камери, монтування чи фокусера.
2. **Довгі астрономічні дії є operations.** Slew, exposure, autofocus, solve, scheduler blocks і майбутні safety workflows мають state, progress, cancellation та resource ownership.
3. **Science acquisition відділений від preview.** Втрачений кадр GUI не повинен означати втрачений SER/science frame.
4. **Capabilities треба discover-ити, а не припускати.** Різні пристрої мають різні controls та constraints.
5. **Native hardware access — first-class.** Vendor SDK і direct hardware protocols можна використовувати без обов'язкового окремого compatibility server.
6. **Compatibility залишається цінною.** INDI, ASCOM/Alpaca та інші established interfaces — варіанти інтеграції, а не «конкуренти, яких треба знищити».
7. **Remote control є частиною архітектури.** HTTP, event streaming і preview transport — protocol surfaces, а не випадкові деталі GUI.
8. **HIL-докази важливі.** Geometry, timing і failure behaviour перевіряються на реальному обладнанні та заморожуються після підтвердження.

## Чого нам не вистачало в наявних рішеннях

OpenAstroLink не виходить із твердження, що INDI, ASCOM, Alpaca, N.I.N.A., KStars/Ekos чи інші астрономічні проєкти погані. Вони вирішують реальні задачі й залишаються корисними interoperability targets. Проблема була в тому, що для OpenAstroLink потрібна була певна **комбінація** властивостей в одному coherent, open, native-first stack.

### GUI ownership проти node ownership

Багато astronomy applications насамперед є desktop applications. Це зручно для інтерактивної роботи, але remote/headless execution і recovery тоді сильно залежать від конкретного application process. OAL натомість робить node власником обладнання, а клієнти — змінними views/controllers.

### Device API проти workflow semantics

Класичні device interfaces добре описують «перемісти фокусер», «наведи монтування» або «запусти exposure». Проєкту додатково потрібна єдина модель операції, яка reserve-ить resources, показує progress, підтримує cancel і композицію в scheduler, autofocus, solve/recenter, mosaic та Polar Alignment.

### Preview traffic проти science data

Planetary imaging може працювати з десятками або сотнями FPS. Не можна робити recording залежним від UI-oriented JSON чи rendering loop. Тому OAL розділяє camera acquisition/recording path і droppable preview transports OALV/WebRTC.

### Native-first hardware без відмови від compatibility

Потрібні були direct QHY, ZWO, Canon, Gemini та Sky-Watcher/EQDrive paths для low-latency control і hardware-specific features, але без втрати compatibility adapters. У OAL native drivers є default architecture, а compatibility — optional.

### Протокол для стороннього ПЗ

OpenAstroLink має бути корисним і без OpenAstroSuite. Planetarium, automation service, mobile client, observatory dashboard чи інша програма повинні мати можливість discover devices, читати capabilities, запускати operations та отримувати state/events через той самий control layer.

## Чим OpenAstroLink не є

- Це **не** твердження, що всі інші astronomy ecosystems треба замінити.
- Це **ще не** unattended-observatory safety platform; найближча Beta залишається supervised.
- Це **ще не** frozen protocol 1.0; частина payload/reliability semantics проходить qualification.
- Це **не** лише GUI. OpenAstroSuite — reference client, а OAL — protocol/control layer.

## Бажаний результат

Довгострокова мета — відкритий observatory substrate, де hardware vendors можуть робити drivers, software developers — незалежні clients, а astronomers — комбінувати обладнання без прив'язки workflow до одного desktop process чи одного vendor SDK.

Див. також: `MANIFESTO.md`, `ARCHITECTURE.md`, `OAL_SPECIFICATION.md`, `OAL_API.md`, `NATIVE_DRIVER_SDK.md` та `CURRENT_CHECKLIST.md`.
