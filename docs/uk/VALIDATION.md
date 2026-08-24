# План валідації — v0.2.10.5

Цей реліз — build/HIL qualification checkpoint.

## Static gates

Запустити всі `tools/*check.py`, включно з `cmake_presets_compat_check.py`, перевірити JSON і `cmake --list-presets`.

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

## Supervised first-light PASS

Чиста збірка/package, hardware probe, HIL mount/focuser/camera, real-sky ASTAP, повторюваний autofocus, перевірені abort paths.

Unattended production PASS не ставити до реалізації reliable event replay, idempotency, durable science storage, production guiding/session recovery, security/auth/audit, safety/weather/roof/power, driver isolation і public conformance.
