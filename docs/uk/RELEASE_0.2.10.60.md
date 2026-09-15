# OpenAstroLink v0.2.10.60 — Assisted Polar / Observable Sky source checkpoint

Дата: **2026-09-14**

Qualification: **поки лише source/static-qualified**. Для нової `.60` функціональності ще потрібні fresh Windows build і real-mount/real-sky HIL. Frozen direct-MC geometry v9 не змінювалася.

## Додано

### Assisted Polar Alignment

- sample після ручного доцентрування вибраної цілі зі Sky Map;
- optional sample за свіжим plate-solved центром;
- UTC-aware 3-D SVD/Kabsch fit у локальній Alt/Az системі;
- оцінка напрямку полярної осі;
- знакові поправки по висоті й азимуту;
- total error, RMS, sky span і confidence;
- однакова підтримка local і remote controller;
- HTTP API та event-stream result.

### Observable Sky Region

- persisted двокутова локальна Alt/Az-зона;
- підтримка переходу через північ (`minAzDeg > maxAzDeg`);
- overlay на Sky Map;
- optional reject automated GOTO поза доступним небом;
- manual joystick явно не обмежується цією зоною;
- Scheduler eligibility та runtime deferral block-ів;
- перехід лише на безпечній межі: активний FITS exposure/SER не обривається тільки через перетин межі видимої зони;
- conservative crash-resume cursor, щоб deferred незавершений block не був тихо пропущений.

## Розділення safety

Observable Sky — це visibility/planning constraint. Вона окрема від:

1. frozen mount geometry v9;
2. raw-axis mechanical collision guard;
3. Polar Motion Limits автоматичного polar-alignment RA-slew workflow.

## Попередній функціонал збережено

v0.2.10.60 зберігає Night Vision/Strict Night Mode та optional black→red mono/raw preview із `.59`, WebRTC/OALV high-rate architecture, Dual Live, native drivers, Scheduler executors, mosaic і попередні polar-alignment workflows.

## Потрібний HIL

- fresh Windows `.60` configure/build;
- 3–5 Assisted Polar sample, механічна корекція та повтор для перевірки convergence;
- two-corner Observable Sky + overlay;
- reject automated GOTO поза region;
- manual joystick поза region;
- Scheduler switch після завершеного FITS/SER і подальше повернення до deferred work.

## Qualification на момент створення checkpoint

Static regression: **86/86 scripts PASS**. Dedicated restricted-sky/Assisted-Polar guard проходить **70 assertions + synthetic SVD rotation-fit recovery test**. Frozen direct-MC geometry v9 і EQDrive source byte-identical до попередньої HIL-бази. Fresh Windows compilation і real-sky/mount HIL навмисно лишаються pending.
