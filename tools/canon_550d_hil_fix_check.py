from pathlib import Path
root=Path(__file__).resolve().parents[1]
canon=(root/"drivers/canon/oal_driver_canon_edsdk.cpp").read_text()
gui=(root/"src/gui/main_window.cpp").read_text()
controller=(root/"src/core/application_controller.cpp").read_text()
checks={
 "version":"0.2.10.32" in canon,
 "hotplug edge preserved": "if(count>0)state.cameraAddedEvent=false" in canon,
 "hotplug bounded settle": "attempts=state.cameraAddedEvent.load()?12:1" in canon,
 "hotplug emits rediscovery hint": 'device.discoveryHint' in canon and 'requesting automatic native rediscovery' in canon,
 "controller consumes rediscovery hint": 'scheduleCanonHotplugRediscovery' in controller,
 "hotplug debounce generation": 'canonHotplugGeneration_' in controller and 'generation!=canonHotplugGeneration_' in controller,
 "hotplug staged retry delays": 'delaysMs[]={700,2200,4500,8000}' in controller and 'const bool hardRecovery=(i==3)' in controller,
 "focused Canon hard fallback": 'Canon hot-plug fallback: hard-reloaded native Canon EDSDK driver' in controller and 'requested("oal.canon")' in controller,
 "Canon hard reload stays on app event loop": 'Qt::BlockingQueuedConnection' in controller and 'hard-reloaded native Canon EDSDK driver on application event-loop thread' in controller,
 "Canon session transfer path validated": 'EdsSetObjectEventHandler' in canon and 'EdsSetPropertyData(SaveTo=Host)' in canon and 'Canon EDSDK session ready: object/state handlers registered' in canon,
 "queued hard recovery keeps scope": 'if(driverIds.isEmpty())pendingNativeDiscoveryAll_=true;' in controller and 'else for(const auto&id:driverIds)' in controller and 'if(hardVendorRecovery)pendingNativeDiscoveryHardRecovery_=true;' in controller,
 "EDSDK gain maps to ISO": 'kEdsPropID_ISOSpeed' in canon and 'setIsoForGain' in canon and 'actualIso' in canon,
 "bulb held shutter": "held-non-af-shutter" in canon and "ShutterButton_Completely_NonAF" in canon,
 "bulbstart only fallback": "trying BulbStart compatibility fallback" in canon,
 "invalid parameter named": "INVALID_PARAMETER" in canon,
 "raw preview fallback": "previewFromStoredOriginal" in canon and "embedded-jpeg" in canon,
 "preview source logged": "Canon preview source=" in canon,
 "serial rediscover label scoped": "Apply port & rediscover selected serial driver" in gui,
 "global refresh label explicit": "Refresh all native devices (USB / serial)" in gui,
}
for k,v in checks.items(): print(("PASS" if v else "FAIL"),k)
if not all(checks.values()): raise SystemExit(1)
print(f"{len(checks)}/{len(checks)} PASS")
