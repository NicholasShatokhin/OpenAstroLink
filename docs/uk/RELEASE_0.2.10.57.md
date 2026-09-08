# OpenAstroLink / OpenAstroSuite v0.2.10.57

**Дата:** 2026-09-07

## Призначення

Це **pre-HIL hardening revision high-rate streaming**, знайдена під час qualification handoff snapshot v0.2.10.56. Зміни стосуються лише correctness/performance camera stream/data plane. Mount coordinate model v9, Home/Park convention, axis meaning і direct serial/Wi-Fi geometry навмисно не змінювалися.

## Hardening high-rate camera streaming

- Прибрано застаріле QHY capability поле `maxFps: 30`; native QHY Live тепер рекламує streaming support без вигаданого device-independent fixed ceiling.
- В OALV v1 `width`/`height` тепер описують фактичний encoded preview JPEG після optional preview downscale. `sourceWidth`/`sourceHeight` зберігають dimensions до transport downscale.
- Remote OALV receiver декодує кожен JPEG лише один раз. Уже декодований Qt image перетворюється в cached OpenCV BGR frame замість другого JPEG decode.
- Один і той самий native physical camera тепер не можна одночасно призначити як Main і Guide. Dual Live лишається функцією для двох різних фізичних камер.
- У steady-state QHY high-rate acquisition повторно використовується buffer, sized у `camera.liveStart`; `GetQHYCCDMemLength` більше не викликається на кожному нормальному live frame, лишаючи fallback query лише якщо start-time sizing був недоступний.
- High-rate static regression розширено цими invariants на додачу до acquisition-first SER, droppable preview та split `/events` + `/video` data plane.

## Qualification boundary

Після source hardening усі repository Python/static regression checks проходять: **73/73 scripts PASS**, включно з **43/43** dedicated high-rate/Dual-Live checks. Спроба local Linux configure у handoff sandbox зупиняється до compilation, бо в цьому environment немає Qt >= 6.4; це host-dependency limitation, а не успішна чи невдала source build qualification. Windows compilation фізично неможливо виконати у цьому Linux sandbox.

Тому v0.2.10.57 **ще не є current-revision build/HIL qualified**. Наступний gate — fresh Windows build (і бажано native Linux із dependencies), після чого QHY high-rate/SER/Dual-Live HIL.

## Наступні gates

Fresh build → QHY camera-max + Preview 60 measurement → 5–10 хв zero-drop SER + AutoStakkert frame-count check → preview 15/30/60/MAX independence → Main+Guide Dual Live → Wi-Fi manual-slew repeat HIL → autofocus/auto-exposure repeat HIL → Scheduler → Mosaic → Polar Alignment → supervised night.
