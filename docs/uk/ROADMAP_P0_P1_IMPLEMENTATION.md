## v0.2.10.50 порядок робіт

Cross-platform build qualification більше не головний blocker: Windows x64, Linux x86_64 та Raspberry Pi/Linux ARM64 node builds підтверджені. Найближча Beta тепер вузько сфокусована:

1. **HIL autofocus** — repeatability, backlash, cancel/failure rollback та verification frame.
2. **HIL auto-exposure** — convergence/lock/reacquire на типових сценах.
3. **Scheduler HIL** — mixed DSO/planetary execution, cancellation і restart boundaries.
4. **Mosaic HIL** — tile geometry, solve/recenter і traversal.
5. **Polar Alignment HIL** — guided sample motion і safe-region behavior.

Smart Telescope UX, ширша unattended-observatory automation та one-button workflows лишаються OAL 1.0 scope.

# Roadmap OpenAstroLink після v0.2.10.47

Позначки: ✅ зроблено; 🟡 частково/HIL pending; ⏳ не зроблено.

## P0

1. **Capabilities/identity/discovery — 🟡**: registry/identity/device capabilities є; треба заморозити normative schemas/versioning.  
2. **Async operations — 🟡**: slew/exposure/autofocus готові; треба перевести решту довгих workflows і додати durable recovery.  
3. **Idempotency/locks — 🟡**: locks/queueing є; `Idempotency-Key` і повний lock graph ще немає.  
4. **Security/safety — ⏳**: TLS/auth/roles/scopes/audit/emergency stop/weather/roof/power policy.  
5. **Science data plane — 🟡**: native frame/preview foundation є; FITS/RAW/SER/checksum/resume/zero-copy/object storage ще треба.  
6. **Reliable WS — ⏳**: stream ID/sequence/replay/lastSequence/snapshot contract.  
7. **Error model — 🟡**: structured operation problems є; повний RFC 9457 HTTP ще ні.  
8. **Conformance — 🟡**: simulator/checks є; public black-box suite ще ні.

## P1

- Filter wheel, rotator, dome/roof, weather/safety, power/switch, cover/calibrator, GPS/time — ⏳.
- ASTAP — 🟡; astrometry.net production adapter — ⏳.
- Durable mixed-mode scheduler — 🟡 supervised DSO executor / ⏳ durable + planetary: v0.2.10.46 вже виконує slew → solve/recenter → autofocus → FITS/RAW × N і periodic recenter/autofocus; v0.2.10.47 додає planetary SER, full-frame acquisition, ROI tracking/provenance та опційний calibrated mount recenter; далі temperature focus, constraints, checkpoints/restart, meridian flip, weather/recovery. Повна модель — `SCHEDULER.md`.
- Guiding — 🟡 foundation; calibration/RMS/dither/backlash/lost-star recovery — ⏳.
- Geometric Bahtinov — ⏳.
- Sandboxed driver host + public C++/Python/Rust SDKs — ⏳.

## Найближча послідовність

1. Чиста Windows MSVC+Ninja і native Linux збірка з реальними SDK.
2. HIL QHY/Canon/ZWO/Gemini/Sky-Watcher: спочатку native-only, потім native+INDI.
3. Real-sky ASTAP → autofocus → closed-loop GOTO → polar alignment.
4. FITS/RAW data plane + planetary SER.
5. Production guider.
6. Durable session engine.
7. Паралельно закривати P0 hardening.

**Supervised first-light:** build + HIL + ASTAP + autofocus.  
**Повний imaging workflow:** додати closed-loop GOTO, polar alignment, DSO storage, SER.  
**Autonomous observatory:** guiding + durable scheduler + safety/recovery.  
**Public OAL ecosystem:** усі P0 + conformance suite.

## Ціль OAL 1.0 — автономна обсерваторія

Заплановано, але ще не завершено: TLS/auth/roles/audit, idempotency, durable operations, replayable events, safety/weather/roof/power interlocks, emergency stop, production guiding, durable mixed DSO/planetary scheduler, automatic meridian flip recovery, durable science data/provenance, driver isolation та public conformance. Це явні roadmap-вимоги 1.0, а не твердження про поточну beta.
