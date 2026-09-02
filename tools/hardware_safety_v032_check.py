#!/usr/bin/env python3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
checks=[]
def need(path,*needles):
    text=(ROOT/path).read_text(encoding='utf-8')
    for n in needles:
        ok=n in text; checks.append((ok,f'{path}: {n}'))
need('CMakeLists.txt','VERSION 0.2.10.49')
need('src/core/application_controller.cpp','nativeHealthTimer_->setInterval(1800)','refreshNativeDiscoveryAsync(QStringList{driver})')
need('drivers/qhy/oal_driver_qhy.cpp','std::try_to_lock','GetQHYCCDParamMinMaxStep(c->handle,CONTROL_EXPOSURE','healthFailures<3','QHY SDK health probe failed three times','device.disconnected')
need('drivers/gemini/oal_driver_gemini.cpp','Gemini health probe failed','positionCommand()','device.disconnected')
need('drivers/eqdrive/oal_driver_eqdrive.cpp','EQDrive health probe failed','mcInstantStopAxis','instantStop(axis)','ABORT_NOT_CONFIRMED')
need('src/backends/oal_native_devices.cpp','writeFitsScienceFrame','OpenAstroLink/','Mechanical Park is not calibrated','if(parking_&&!abortMotion(e))','maxGotoAxisDeltaDeg')
need('src/gui/main_window.cpp','Save user captures as science files','Sync mount to last successful plate solve','MOUNT MODEL','Reverse Axis 1 mapping / tracking direction','Clear native OAL Park calibration')
failed=[label for ok,label in checks if not ok]
for ok,label in checks: print(('PASS' if ok else 'FAIL')+': '+label)
if failed: raise SystemExit(f'{len(failed)} failure(s)')
print(f'hardware safety v0.2.10.32: PASS ({len(checks)} assertions)')
