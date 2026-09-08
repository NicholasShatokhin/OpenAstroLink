# OpenAstroLink / OpenAstroSuite v0.2.10.56

**Дата:** 2026-09-07

## Призначення

Це синхронізований **handoff/full-package checkpoint** після source pass v0.2.10.55 high-rate/Dual Live і hardware findings 2026-09-06. Ревізія навмисно не змінює mount geometry і не додає новий observing algorithm. Вона консолідує current source, version metadata, documentation, site state, validation boundaries і instructions для продовження в новому чаті.

## High-rate camera streaming перенесений без зміни semantics

High-rate camera streaming implementation перенесено з v0.2.10.55 без зміни intent: OALV v1 `/video`, independent capture/preview rates, acquisition-first SER і Dual Live. Цей release не заявляє новий HIL для цього path.

## Перенесений функціональний стан

- v0.2.10.55: high-rate OALV v1 `/video`, independent capture/preview FPS, acquisition-first raw SER і simultaneous Main+Guide Dual Live.
- v0.2.10.54: direct-Wi-Fi manual-slew reliability hardening, visible axis-inversion checkboxes, Sky Map → Mount target synchronization, scene-AF repeatability hardening, sparse-scene auto-exposure hardening і live-preview race fixes.
- v0.2.10.53: solved/planned camera footprints, mosaic overlay і Stellarium FOV export.
- v0.2.10.52: free-point Sky Map navigation і Scheduler transfer.
- HIL-qualified mount coordinate model v9 лишається незмінною.

## Qualification boundary

Попередні Windows/Linux/RPi build foundations фізично підтверджені, але current high-rate source branch ще не пройшов fresh physical build/HIL cycle. Тому v0.2.10.56 — це **source/documentation handoff checkpoint**, а не твердження, що новий high-rate code уже hardware-qualified.

## Наступні gates

Fresh build → high-rate SER/Dual Live HIL → Wi-Fi manual slew repeat HIL → autofocus/auto-exposure repeat HIL → Scheduler → Mosaic → Polar Alignment → supervised night.
