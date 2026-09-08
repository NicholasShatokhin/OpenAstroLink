# OpenAstroLink / OpenAstroSuite v0.2.10.56

**Date:** 2026-09-07

## Purpose

This is the synchronized **handoff/full-package checkpoint** after the v0.2.10.55 high-rate/Dual-Live source pass and the 2026-09-06 hardware findings. It does not intentionally change mount geometry or add a new observing algorithm. It consolidates current source, version metadata, documentation, site state, validation boundaries and the next-chat continuation instructions.

## High-rate camera streaming carried forward

The high-rate camera streaming implementation is carried from v0.2.10.55 unchanged in intent: OALV v1 `/video`, independent capture/preview rates, acquisition-first SER and Dual Live. This release does not claim new HIL for that path.

## Carried functional state

- v0.2.10.55 high-rate OALV v1 `/video` streaming, independent capture/preview FPS, acquisition-first raw SER and simultaneous Main+Guide Dual Live.
- v0.2.10.54 direct-Wi-Fi manual-slew reliability hardening, visible axis-inversion checkboxes, Sky Map → Mount target synchronization, scene-AF repeatability hardening, sparse-scene auto-exposure hardening and live-preview race fixes.
- v0.2.10.53 solved/planned camera footprints, mosaic overlay and Stellarium FOV export.
- v0.2.10.52 free-point Sky Map navigation and Scheduler transfer.
- HIL-qualified mount coordinate model v9 remains unchanged.

## Qualification boundary

Previous Windows/Linux/RPi build foundations are physically confirmed, but the current high-rate source branch has not yet received a fresh physical build/HIL cycle. Therefore v0.2.10.56 is a **source/documentation handoff checkpoint**, not a claim that the new high-rate code is already hardware-qualified.

## Next gates

Fresh build → high-rate SER/Dual Live HIL → Wi-Fi manual slew repeat HIL → autofocus/auto-exposure repeat HIL → Scheduler → Mosaic → Polar Alignment → supervised night.
