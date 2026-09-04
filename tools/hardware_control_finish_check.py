#!/usr/bin/env python3
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1]
checks=[]
def has(path,*tokens):
 s=(r/path).read_text(errors='replace')
 return all(t in s for t in tokens)
def check(name,cond):
 checks.append((name,bool(cond)));print(('PASS' if cond else 'FAIL'),name)
canon='drivers/canon/oal_driver_canon_edsdk.cpp'
app='src/core/application_controller.cpp'
check('Canon shutdown state handler',has(canon,'EdsSetCameraStateEventHandler','kEdsStateEvent_Shutdown','device.disconnected','transportLost'))
check('Camera disconnect wakes pending exposure',has(canon,'c->eventCv.notify_all()','Canon camera was switched off or USB transport was lost during exposure'))
check('Controller drops native device on physical disconnect',has(app,'Main camera physically disconnected','camera_.reset()','operations_.isResourceLocked'))
apptext=(r/app).read_text(); idx=apptext.find('Main camera physically disconnected'); check('Persisted binding not cleared by physical disconnect','saveCameraBinding' not in apptext[max(0,idx-500):idx+900])
check('Native geometry configurable GOTO safety',has('src/backends/oal_native_devices.cpp','maxGotoAxisDeltaDeg'))
check('EQDrive temporary qualification GOTO cap removed','gMaxNativeGotoDeg' not in (r/'drivers/eqdrive/oal_driver_eqdrive.cpp').read_text() and 'HIL_SAFETY_LIMIT' not in (r/'drivers/eqdrive/oal_driver_eqdrive.cpp').read_text())
check('EQDrive manifest has no hidden GOTO cap','maxNativeGotoDeg' not in (r/'drivers/eqdrive/oal_driver_eqdrive.manifest.json').read_text())
check('Direct WiFi old HIL gate removed','HIL-limited to' not in (r/'src/backends/synscan_network_mount.cpp').read_text())
check('Core manual mount API',has('src/core/observatory_controller.h','manualMountSlew') and has('src/core/interfaces.h','manualSlew'))
check('REST manual slew',has('src/oal/oal_server.cpp','manual-slew','manualMountSlew'))
check('Remote manual slew',has('src/core/remote_observatory_controller.cpp','manualMountSlew','manual-slew'))
check('GUI two-axis hold pad',has('src/gui/main_window.cpp','Manual two-axis slew','QPushButton::pressed','QPushButton::released','Axis 1 +','Axis 2 +'))
check('Native EQDrive manual slew',has('drivers/eqdrive/oal_driver_eqdrive.cpp','mount.manualSlew','mcSetManualRate'))
check('Native SynScan manual slew',has('drivers/skywatcher/oal_driver_skywatcher.cpp','mount.manualSlew','fixedRate'))
check('Direct WiFi manual slew',has('src/backends/synscan_network_mount.cpp','SynScanNetworkMount::manualSlew','setManualRate'))
check('Classic ASCOM MoveAxis',has('tools/ascom_host/main.cpp','manualSlew','MoveAxis'))
check('Stellarium live J2000 position',has('src/integrations/stellarium_telescope_server.cpp','live mount position stream active','convertEquatorialFrame(status.coordinate,EquatorialFrame::J2000)','positionTimer_.setInterval(250)'))
fail=[n for n,v in checks if not v]
print(f'{len(checks)-len(fail)}/{len(checks)} PASS')
sys.exit(1 if fail else 0)
