from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=[]
def has(path,text,label):
    ok=text in (root/path).read_text(encoding='utf-8',errors='ignore');checks.append((label,ok))

has('src/oal/driver_plugin_loader.cpp','refreshDevices(const QStringList &driverIds','filtered native driver refresh API')
has('src/core/application_controller.cpp','driversToRefresh','reconnect refresh scoped to missing native drivers')
has('src/core/application_controller.cpp','refreshNativeDiscoveryAsync(QStringList{driverId})','serial selector refreshes only selected native driver asynchronously')
has('src/core/application_controller.cpp','Persisted %1 binding auto-migrated','stale COM binding unique-auto migration')
has('src/core/application_controller.cpp','"backends",QJsonObject','state carries live backend catalogue')
has('src/core/remote_observatory_controller.cpp','updateBackendCatalogFromState','remote GUI updates backend catalogue from state')
has('src/gui/main_window.cpp','refreshBackendComboIfChanged','GUI repopulates backend combos after hot-plug')
has('drivers/qhy/oal_driver_qhy.cpp','if(anyCameraConnected())','QHY enumeration cache-only while connected')
has('drivers/qhy/oal_driver_qhy.cpp','CancelQHYCCDExposingAndReadout(post-frame)','QHY single-frame lifecycle termination')
has('drivers/qhy/oal_driver_qhy.cpp','CAPTURE_TIMEOUT','QHY readout watchdog')
has('src/backends/synscan_app_mount.cpp','Ok,ServerVersion','SynScan App 11881 discovery')
has('src/backends/synscan_network_mount.cpp','11880','direct mount Wi-Fi UDP 11880')
has('src/backends/synscan_network_mount.cpp','192.168.0.1','EQDrive AP gateway fallback')
has('src/gui/main_window.cpp','mountEndpointsByBackend_','mount endpoint remembered per backend')
has('src/gui/main_window.cpp','mount/EQDrive Wi-Fi adapter IP[:11880]','direct Wi-Fi endpoint UX')
has('src/core/application_controller.cpp','Mount GOTO preflight','GOTO direction preflight diagnostics')
has('src/integrations/stellarium_telescope_server.cpp','decoded/forwarded unchanged','Stellarium coordinates logged unchanged')
has('drivers/eqdrive/oal_driver_eqdrive.cpp','official EQDrive ASTEP','native EQDrive uses official ASTEP mount protocol')
has('drivers/eqdrive/oal_driver_eqdrive.cpp','native EQDrive discovery will not fight for the port','busy COM short-circuits discovery spam')
has('drivers/eqdrive/oal_driver_eqdrive.cpp','no hidden driver-level sky GOTO qualification cap','native EQDrive temporary qualification cap removed')
failed=[n for n,ok in checks if not ok]
for n,ok in checks:print(('PASS' if ok else 'FAIL')+': '+n)
print(f'{len(checks)-len(failed)}/{len(checks)} checks passed')
raise SystemExit(1 if failed else 0)
