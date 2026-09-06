## v0.2.10.53 validation focus

Build qualification підтверджена для Windows x64, native Linux x86_64 та Linux/WSL→ARM64 Raspberry Pi node/probe/native-driver target. Ці builds лишаються regression gates. Наступний release gate — HIL workflow behavior, починаючи з autofocus. Для HIL-qualified direct-MC mount після змін заліза лишається small-motion sanity check, після чого треба тестувати звичайний supervised full-range GOTO; прихованого 15° driver qualification cap більше немає. Profile-level sky-safety оператор може залишити ввімкненим.

# План валідації — v0.2.10.53

Цей реліз — build/HIL qualification checkpoint.

## Static gates

Запустити всі `tools/*check.py`, включно з `cmake_presets_compat_check.py` та `mount_v9_unrestricted_goto_check.py`, перевірити JSON і `cmake --list-presets`.

## Windows

```powershell
.\scripts\check_build_environment.ps1
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

PASS: CMake >=3.20, Ninja, MSVC `cl.exe`, без MinGW/Strawberry, сумісні x64 MSVC Qt/OpenCV/vendor libs. QHY SDK include не повинен потрапляти в normal `/I`; використовується generated wrapper.

## Linux/WSL

```bash
./scripts/check_build_environment.sh
rm -f CMakeUserPresets.json
cp CMakeUserPresets.example.json CMakeUserPresets.json
# відредагувати SDK paths
./scripts/build_linux.sh my-linux-observatory
```

При preset error зберегти `cmake --version` і перші рядки обох preset JSON.

## HIL

Mount: status → safe small GOTO → abort → tracking → sync → pier side → pulse guide → reconnect.  
Focuser: position → small moves → absolute move → halt if advertised → temp/limits → reconnect → autofocus repeats.  
QHY/ZWO: exact ID → short/long exposure → gain/offset → ROI/binning → bit depth → cancel → 20–100 frames → unplug/replug → main+guide simultaneous test.  
Canon: discovery/session → short capture → original file → Bulb → cancel → preview → repeated capture → reconnect.  
ASTAP: archived frame → real frame → scale check → repeated solve → closed-loop correction.

Scheduler/DSO v0.2.10.46: один block на 2–3 короткі science frames → recenter before first (1–2 arcmin) → autofocus before first → перевірити `slew → solve → correction slew/re-solve за потреби → autofocus → capture`; далі окремо `recenter every 1 frame` і `autofocus every 1 frame`; Stop перевірити під час кожного child operation; solve/recenter failure має завершити session у `failed` з `lastError`. Legacy `targets` має лишитися сумісним без неявної вимоги solver/focuser. Planetary HIL: почати з mount corrections OFF: GOTO → full-frame detection → autofocus → 10–20 s ROI SER; перевірити SER, `.txt`, `.roi.jsonl` і примусовий ROI shift без зміни геометрії SER. Calibrated mount corrections тестувати окремо лише після безпечного HIL backend; calibration micro-slews мають повернути ціль, а всі corrections — бути записані.

## Supervised first-light PASS

Чиста збірка/package, hardware probe, HIL mount/focuser/camera, real-sky ASTAP, повторюваний autofocus, перевірені abort paths.

Unattended production PASS не ставити до реалізації reliable event replay, idempotency, durable science storage, production guiding/session recovery, security/auth/audit, safety/weather/roof/power, driver isolation і public conformance.

## Mount v9 full-range gate

Після будь-якої зміни hardware/config зробити один small supervised sanity GOTO, а потім normal supervised full-range GOTO на representative east/west targets. Hidden 15° `maxNativeGotoDeg` driver gate у v0.2.10.50 відсутній. Якщо `maxGotoSkyDeltaDeg` у profile увімкнений/малий, це окрема user-controlled Core safety policy, а не driver limitation. Abort має реально зупинити обидві осі. Automatic meridian flip не включати в unattended PASS без окремого HIL.

## Sky Map MVP — v0.2.10.52

- Перевірити `Imaging / Sky Map` у лівій workspace на Windows, Linux та Raspberry Pi GUI builds.
- З configured site вибрати яскраву зорю й порівняти Alt/Az з незалежним reference.
- Sky Map GOTO та Mount-tab GOTO для тієї самої J2000 coordinate мають давати той самий target active backend.
- Double-click GOTO має abort-итися; Park/Unpark проходять через active controller.
- Після plate solve зелений solved marker має відповідати solved J2000 center.
- Червоний telescope marker оновлюється зі mount state, approximate FOV — зі зміною optical profile.
- ✅ Підтверджено у running GUI: catalogue-object `Use in Scheduler` успішно копіює selected J2000 coordinate.
- Click у порожню точку всередині horizon circle має створити жовтий `Target`; RA/DEC + Alt/Az повинні бути правдоподібні.
- **Slew** і double-click GOTO мають використовувати free-point coordinate, Abort — працювати як раніше.
- Free-point `Use in Scheduler` має копіювати ту саму J2000 coordinate.
- Click поза horizon circle не створює target, а nearby catalogue object має click priority.

## Sky Map framing / Stellarium Remote Control — v0.2.10.53

- [ ] Solve real main-camera frame і перевірити, що `lastSolvedFrame` center збігається з solve center.
- [ ] Measured width/height мають дорівнювати solver scale × actual solved pixel dimensions.
- [ ] Фізично повернути камеру й перевірити PA sign/orientation на Sky Map проти solved image.
- [ ] Predicted main/guide footprint sizes мають відповідати optical profile.
- [ ] Solved/main/guide layers і labels мають незалежно вмикатися.
- [ ] Scheduler mosaic columns/rows/overlap/rotation мають правильно рендеритися навколо selected target.
- [ ] Перевірити двосторонню синхронізацію Sky Map main PA ↔ Scheduler mosaic rotation.
- [ ] Передати selected target/framing у Scheduler без зміни J2000 coordinates.
- [ ] Увімкнути Stellarium Remote Control і передати Solved/Main/Guide/Mosaic envelope; звірити center/size/PA.
- [ ] Hide Stellarium footprint не має переривати TCP Telescope Control bridge.

