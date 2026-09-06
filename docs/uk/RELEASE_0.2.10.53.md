# OpenAstroLink / OpenAstroSuite v0.2.10.53

## Sky Map framing

v0.2.10.53 завершує всі три заплановані етапи camera-footprint поверх offline Sky Map v0.2.10.51–52.

- Кожен successful main-camera plate solve зберігає `lastSolvedFrame`: J2000 center, measured angular width/height, solver PA, solved pixel dimensions і provenance.
- Sky Map малює цю measured frame як повернутий celestial rectangle.
- До solve окремо доступні predicted main/guide-camera footprints із optical profile, independent visibility і position angles.
- Scheduler mosaic rows/columns/overlap/rotation відображаються на Sky Map як tile grid; main Sky Map PA та Scheduler mosaic rotation синхронізовані.
- `Use in Scheduler` переносить selected J2000 target і current main framing rotation.
- OpenAstroSuite може передавати Solved/Main/Guide/Mosaic-envelope footprint у optional Stellarium Remote Control HTTP plugin. Standard Telescope Control TCP лишається mount position/GOTO only.
- Stock Stellarium має одну rectangular FOV frame, тому export замінює попередню exported frame; кілька persistent rectangles не імітуються.

## Compatibility

- Windows x64, Linux x86_64 та Raspberry Pi ARM64 build foundations не змінені.
- Native OAL drivers лишаються default; INDI — opt-in.
- Direct-MC mount geometry v9 не змінена й лишається HIL-qualified/frozen.

## Що ще перевірити

Потрібен physical/UI HIL для знака solved-frame orientation, main/guide profile FOV, mosaic planner overlap, Scheduler framing transfer та Stellarium Remote Control проти реально запущеного Stellarium.
