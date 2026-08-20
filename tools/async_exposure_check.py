from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks={
 'controller startCapture':'QString ApplicationController::startCapture' in (root/'src/core/application_controller.cpp').read_text(),
 'camera.exposure operation':'"camera.exposure"' in (root/'src/core/application_controller.cpp').read_text(),
 'camera resource lock':'{"camera"}' in (root/'src/core/application_controller.cpp').read_text(),
 'capture 202':'acceptedResponse(controller_->operation(id,nullptr))' in (root/'src/oal/oal_server.cpp').read_text(),
 'preview endpoint':'/api/v1/frames/<arg>/preview' in (root/'src/oal/oal_server.cpp').read_text(),
 'remote startCapture':'RemoteObservatoryController::startCapture' in (root/'src/core/remote_observatory_controller.cpp').read_text(),
 'frameReady event':'frameReady' in (root/'src/core/remote_observatory_controller.cpp').read_text(),
 'gui tracks exposure':'kind=="camera.exposure"' in (root/'src/gui/main_window.cpp').read_text(),
 'embedded sim abort':'SimulatedCamera::abortExposure' in (root/'src/backends/simulated_devices.cpp').read_text(),
 'native QHY abort':'camera.abortExposure' in (root/'drivers/qhy/oal_driver_qhy.cpp').read_text(),
 'native frame plane':'publishFrame' in (root/'drivers/qhy/oal_driver_qhy.cpp').read_text(),
}
failed=[k for k,v in checks.items() if not v]
if failed:
 print('FAILED:', ', '.join(failed)); raise SystemExit(1)
print('Async exposure/capture operation checks passed.')
