from pathlib import Path
root=Path(__file__).resolve().parents[1]
canon=(root/'drivers/canon/oal_driver_canon_edsdk.cpp').read_text()
stub=(root/'tests/stubs/edsdk/EDSDK.h').read_text()
checks={
 'AF_NG named diagnostic':'EDS_ERR_TAKE_PICTURE_AF_NG' in canon,
 'non-AF shutter capture':'kEdsCameraCommand_ShutterButton_Completely_NonAF' in canon,
 'no TakePicture fallback':'kEdsCameraCommand_TakePicture,0' not in canon,
 'Tv property exposure':'kEdsPropID_Tv' in canon and 'setTvForExposure' in canon,
 'Bulb exposure path':'setBulbTv' in canon and 'held-non-af-shutter' in canon and 'kEdsCameraCommand_BulbStart' in canon,
 'Bulb UI lock':'kEdsCameraStatusCommand_UILock' in canon and 'kEdsCameraStatusCommand_UIUnLock' in canon,
 'EDSDK event pump':'state.eventPumpThread' in canon and 'EdsGetEvent()' in canon,
 'camera-added handler':'EdsSetCameraAddedHandler' in canon,
 'camera-added bounded enumeration retry':'attempts=state.cameraAddedEvent.load()?12:1' in canon and 'if(count>0)state.cameraAddedEvent=false' in canon,
 'CR2 embedded JPEG preview':'previewFromStoredOriginal' in canon and 'embedded-jpeg' in canon,
 'automatic rediscovery wording':'requesting automatic native rediscovery' in canon,
 'requested/actual exposure metadata':'requestedExposureSec' in canon and 'actualExposureSec' in canon,
 'stub models new API':'EdsGetPropertyDesc' in stub and 'EdsSetCameraAddedHandler' in stub,
}
for k,v in checks.items(): print(('PASS' if v else 'FAIL'),k)
failed=[k for k,v in checks.items() if not v]
if failed: raise SystemExit(1)
print(f'{len(checks)}/{len(checks)} PASS')
