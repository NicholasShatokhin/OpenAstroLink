# OpenAstroLink / OpenAstroSuite v0.2.10.55

## High-rate camera streaming та Dual Live

Цей реліз продовжує HIL-hardening від v0.2.10.54 і прибирає стару preview-архітектуру, де Live View залежав від окремого HTTP PNG/base64 fetch для кожного кадру. Геометрія монтування v9 не змінювалася.

### High-rate acquisition

- Прибрано старий ліміт Live View у 30 FPS.
- `captureFpsLimit = 0` означає отримувати native frames з максимальною швидкістю, яку реально видає camera SDK; явний ліміт підтримується до 1000 FPS.
- `previewFpsLimit` незалежний від acquisition і за замовчуванням дорівнює 60 FPS. Preview latest-only/droppable і не повинен гальмувати acquisition.
- Native Live View за замовчуванням просить 8-bit transfer для максимальної швидкості; 16-bit залишається опцією.
- QHY та ZWO ASI повторно використовують live buffers замість виділення нового великого vector на кожний SDK frame.
- Після ABI-v2 host copy прибрано зайвий повнокадровий OpenCV clone.

### Recording-first pipeline

Тепер acquisition path:

`camera SDK -> acquired raw frame -> SER append -> optional tracking analysis -> droppable preview processing -> UI/network preview`

SER отримує кожен успішно захоплений OAL кадр до preview conversion, scaling, debayer або JPEG. Preview frames під навантаженням можна пропускати; `recordDropped` рахується окремо і в qualification run має залишатися нулем. Фізична USB/disk bandwidth все одно може обмежити реальну швидкість acquisition/recording.

Planetary SER tracking analysis виконується приблизно на 20 Hz, а не на кожному високочастотному кадрі.

### OALV v1 binary preview transport

Remote Live View більше не робить `HTTP -> PNG -> base64 -> JSON` для кожного live frame. На WebSocket port є два логічні paths:

- `/events`: JSON state/operations/events;
- `/video`: binary OALV v1 preview packets.

OALV v1 packet:

1. ASCII magic `OALV`;
2. one-byte version (`1`);
3. one-byte camera role (`0=main`, `1=guide`);
4. 32-bit big-endian довжина JSON header;
5. compact JSON header (frame id, timestamp, dimensions, exposure, gain, measured FPS/drop counters, transport metadata);
6. JPEG preview payload.

JPEG encoding винесено з acquisition та WebSocket event threads. Повільний preview client втрачає старі preview frames, а не сповільнює science acquisition. Still/AF frames лишаються на існуючому exact-ID HTTP preview path.

### Одночасний Main + Guide Live View

- REST Live View приймає camera role `main` або `guide`.
- Main і guide мають незалежні resource locks (`camera`, `camera.guide`) та можуть стрімити одночасно, якщо це різні фізичні камери.
- В OpenAstroSuite додані Guide Live controls і Dual Live alignment view з незалежними exposure, gain, binning, capture limit, preview limit, transfer depth і preview width.
- UI показує measured capture FPS, record FPS, preview FPS, preview drops та record drops для обох camera roles.

### HIL gates цього релізу

- >=60 FPS main-camera Live View при достатньо короткій експозиції, якщо це дозволяють camera/USB.
- Підтвердити, що зміна preview FPS не змінює SER recording FPS.
- 5-10 хв high-rate SER із `recordDropped = 0`.
- Одночасний main + guide Live View з незалежними controls.
- Перевірити responsiveness preview паралельно з mount/focuser operations та JSON event traffic.
- Повторити v0.2.10.54 HIL для Wi-Fi manual slew, scene autofocus і sparse auto-exposure.

## Safety / frozen geometry

Direct-MC mount geometry v9 залишається frozen і незмінною. High-rate camera work не змінює mount geometry, Home/Park, pier-side mapping або GOTO coordinate equations.
