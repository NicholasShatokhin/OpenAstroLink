# Оновлення стану — OpenAstroLink / OpenAstroSuite v0.2.10.58

**Дата snapshot:** 2026-09-07  
**Мета цієї ревізії:** синхронізований repository/documentation/site/handoff пакет. Функціональний camera-streaming код — це реалізація v0.2.10.55 разом з уже інтегрованим HIL-hardening v0.2.10.54; v0.2.10.57 переважно фіксує поточну межу стану та handoff.

## Що фізично підтверджено

- ✅ Windows x64 build foundation дав успішний full observatory build.
- ✅ Native Linux x86_64 build foundation дав успішний full observatory build.
- ✅ Linux/WSL → Raspberry Pi ARM64 cross-build дійшов до 100% для node, hardware probe і native drivers.
- ✅ Direct-MC mount coordinate model **v9** HIL-підтверджений на реальному монтуванні з `Axis1Sign=+1`, `Axis2Sign=-1`.
- ✅ Free-point Sky Map GOTO фізично рухає реальне монтування.
- ✅ Sky Map target → Scheduler transfer працює у running GUI.
- ✅ QHY5III462C still FITS capture і native Live View працюють на реальному hardware.
- ✅ Gemini focuser: базовий рух, status/temperature, AF cancel і restore starting position працюють на реальному hardware.
- ✅ SER, створений OAL, відкривається в AutoStakkert.
- ✅ Mount GOTO abort HIL-positive.

## Реалізовано, але потрібен repeat/current-revision HIL

- 🟡 v0.2.10.54 hardening direct SynScan/EQDrive Wi-Fi manual slew: retry, instant stop, start/stop post-condition verification і fail-safe stop.
- 🟡 Інверсія осей у Mount tab тепер є явними stateful checkbox-ами; repeat HIL має підтвердити видимий стан і фізичний напрямок.
- 🟡 Selection на Sky Map заповнює той самий J2000 target у Mount tab; GUI треба повторно перевірити після current build.
- 🟡 Scene autofocus тепер використовує bright-tail metering, informative tiled contrast, compact near-focus search і repeatability-gated acceptance.
- 🟡 Still auto-exposure тепер використовує sparse-scene P99.5 bright-tail control, hysteresis/crossing logic і reset controller state при зміні gain.
- 🟡 Sky Map solved/main/guide/mosaic frame overlays та Stellarium Remote Control FOV export реалізовані; end-to-end HIL ще pending.
- 🟡 High-rate OALV v1 Live View і Dual Live реалізовані, але **ще не мають physical qualification** на v0.2.10.57.

## Стан high-rate streaming

Стару стелю Live View 30 FPS прибрано. Acquisition і GUI preview мають незалежні rate limits; `captureFpsLimit=0` означає camera maximum, preview default — 60 FPS. Raw SER отримує acquired frames до preview processing. Remote preview pixels ідуть binary OALV v1 через `/video`; JSON state/events лишаються на `/events`. Main і guide cameras мають окремі locks і можуть одночасно виконувати Live View.

Найближчі HIL criteria:

1. Main-camera measured capture rate >60 FPS, якщо exposure/USB hardware це дозволяють.
2. Preview 15/30/60/MAX не повинен істотно зменшувати capture/record FPS.
3. SER 5–10 хв із `recordDropped=0`, потім перевірка файла в AutoStakkert і звірка frame count з telemetry/sidecar.
4. Main + guide Dual Live одночасно з незалежними exposure/gain/preview limits.
5. Slow/stalled preview має збільшувати preview drops, а не recording drops.
6. Mount/focuser controls і `/events` повинні лишатися responsive при завантаженому `/video`.

## Виміряні HIL-проблеми 2026-09-06, які привели до current fixes

- Direct Wi-Fi manual slew commands доходили до node, але фізичні press/release іноді були intermittent: монтування могло проігнорувати press або продовжити рух після release.
- Scene autofocus control/cancel/restore працювали, але optical convergence була надто повільною/шумною. У кількох runs contrast surface була shallow, тому невеликі score differences могли рухати focuser без надійного покращення.
- Старий AF meter міг пройти 0.05 → 0.2 → 0.8 → 3.2 → 10 s на sparse daylight scene, бо оцінював темне global field замість корисної bright structure.
- Still auto-exposure показав виміряний limit cycle приблизно 0.64 s ↔ 1.43 s після зміни gain на 200. Frame 0.6381 s мав корисний bright-tail level, а 1.4307 s уже кліпав bright tail; старий global-median controller рухав exposure у неправильний бік для sparse scenes.
- Live View іноді давав `Frame is no longer available in the in-memory preview cache`; high-rate `/video` streaming прибирає цей HTTP-cache design із live data path.

## Frozen facts монтування

- Direct-MC coordinate model: **v9**.
- Qualified signs: `Axis1Sign=+1`, `Axis2Sign=-1`.
- Native serial EQDrive і direct UDP/11880 SynScan/EQDrive Wi-Fi використовують спільну Core geometry/GOTO planning.
- **Не змінювати** v9 geometry, axis meaning, Home/Park convention, serial/Wi-Fi parity або polarity без нових HIL-доказів.
- Старий прихований driver `maxNativeGotoDeg` / 15° qualification gate видалений у v0.2.10.50. Operator-controlled sky-safety та explicit raw-axis mechanical guards лишаються окремими safety mechanisms.

## Build/platform policy

- Native OAL drivers — default; INDI — лише opt-in compatibility.
- Confirmed ARM64 vendor path: QHYCCD 26.06.04, Canon EDSDK ARM64, ZWO ASI/EAF ARM64 плюс source-native Gemini/Sky-Watcher/EQDrive.
- Raspberry Pi 5 використовує той самий generic Linux `aarch64` ABI target; physical Pi 5 runtime/GUI/HIL ще pending.
- macOS Intel/Apple Silicon presets/bootstrap існують; physical build/sign/runtime qualification ще pending.
- Canon EDSDK лишається local-discovery/manual-download. Qt/OpenCV/QHY/ZWO dependencies, які можна детерміновано отримувати, шукаються/bootstrapped platform wrappers там, де це підтримується.

## Найближча Beta sequence

1. Зібрати v0.2.10.58 на Windows (і бажано Linux) з увімкненим WebRTC.
2. High-rate main-camera + SER HIL, потім Dual Live main+guide HIL.
3. Repeat direct-Wi-Fi manual-slew HIL із серією rapid press/release changes.
4. Repeat scene autofocus і still auto-exposure HIL на тому самому real setup, де проявилися failures.
5. Night star/HFR autofocus HIL.
6. Scheduler end-to-end HIL.
7. Mosaic HIL.
8. Polar Alignment HIL.
9. Full supervised night/session qualification.

Smart Telescope UX лишається scope OAL 1.0, а не найближчої Beta.

## 2026-09-07 Windows runtime qualification update — buildfix8
Fresh Windows/MSVC build з WebRTC успішно завершений, node стартує після staging WebRTC runtime, а OpenCV/UVC Live View має реальний runtime smoke pass. Remaining Windows runtime blocker — `ERROR_BAD_EXE_FORMAT` для Canon EDSDK і ZWO EAF plugins. buildfix8 додає PE-machine validation, stale-DLL cleanup та in-place repair command; повторна runtime-перевірка Canon/EAF тепер є immediate gate перед QHY/WebRTC throughput HIL.

- Windows buildfix9: Canon EDSDK import/runtime pair locking added after detecting an I386 runtime against the AMD64 import library. Runtime re-test pending.
