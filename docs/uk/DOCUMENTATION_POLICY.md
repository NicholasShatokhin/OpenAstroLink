# Політика документації

> Поточний synchronized snapshot: **v0.2.10.57 (2026-09-07)**. Див. `STATUS.md` та `RELEASE_0.2.10.57.md` щодо current qualification boundaries.



> **Поточний реліз:** v0.2.10.53. Англійська версія лишається канонічною, українські дзеркала — обов’язковими.

Англійська є канонічною та нормативною мовою документації OpenAstroLink/OpenAstroSuite. Кожен людиночитний документ має українське дзеркало в `docs/uk/` (або `README_UA.md` / `PROJECT_MANIFEST_UA.md` у корені).

Якщо переклади розходяться, пріоритет має англійська версія. Українські дзеркала повинні зберігати технічний зміст, приклади, застереження та чесні статуси зрілості. Назви полів API, driver IDs, C/C++ symbols, protocol constants та OpenAPI/JSON Schema identifiers лишаються англійськими.

`docs/openapi.yaml` і JSON Schemas є машинозчитуваними нормативними артефактами й не перекладаються по полях.


## Handoff synchronization rule — v0.2.10.57

Milestone/handoff package має синхронізувати current status, START_HERE, handoff, validation, roadmap, release note, site і обидва language trees. Historical release notes не переписуються, окрім factual errata.
