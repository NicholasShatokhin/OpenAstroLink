from pathlib import Path
import json

root = Path(__file__).resolve().parents[1]
checks=[]
def need(path,*tokens):
    text=(root/path).read_text(encoding='utf-8',errors='ignore')
    for token in tokens:
        assert token in text,f'{path}: missing {token!r}'
        checks.append((path,token))
    return text

cmake=need('CMakeLists.txt','project(OpenAstroSuite VERSION 0.2.10.35','OAS_ENABLE_NATIVE_EQDRIVE','OAS_ENABLE_ASCOM_CLASSIC','oal_driver_eqdrive','oas-ascom-host','src/backends/synscan_app_mount.cpp','src/backends/synscan_network_mount.cpp')
assert 'RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"' in cmake
manifest=json.loads((root/'drivers/eqdrive/oal_driver_eqdrive.manifest.json').read_text())
assert manifest['driverId']=='oal.eqdrive' and manifest['version']=='0.2.10.32' and 'mount' in manifest['deviceClasses']
checks += [('eqdrive manifest','driver/version/mount')]

need('drivers/skywatcher/motor_controller_protocol.h','decodeU24','decodePosition','parseStatus','getVersion','getCountsPerRev','setMotionMode','setGotoIncrement','setStepPeriod')
need('drivers/eqdrive/oal_driver_eqdrive.cpp','official EQDrive ASTEP','read("St"','read("Pos"','read("Cg"','Goto %1 %2','Access is denied','if(m=="mount.status")','if(m=="mount.sync")','if(m=="mount.slew")','HIL_SAFETY_LIMIT','"rawAxes"','mount.gotoAxes')
need('tools/ascom_host/main.cpp','ASCOM.Utilities.Chooser','CoCreateInstance','SlewToCoordinatesAsync','AbortSlew','SyncToCoordinates','PulseGuide','COINIT_APARTMENTTHREADED')
need('src/backends/ascom_classic_mount.cpp','ASCOM helper','chooseTelescope','setupTelescope')
need('src/backends/synscan_app_mount.cpp','RightAscensionDeclinationGet','SlewToCoordinatesAsync','ServerVersion','11881','PulseGuide')
need('src/backends/synscan_network_mount.cpp','11880','Motor Controller','getVersion(1)',"axisQuery('a'",'geometry_.axesForSky','geometry_.sync','SynScanNetworkMount::manualSlew')
assert '11882' not in (root/'src/backends/synscan_network_mount.cpp').read_text(encoding='utf-8')
checks.append(('synscan-wifi','direct UDP 11880, no 11882'))

app=need('src/core/application_controller.cpp','OAL_EQDRIVE_PORT','"synscan-app"','"synscan-wifi"','"ascom-classic"','oal.skywatcher','oal.eqdrive','"backends"')
need('src/gui/main_window.cpp','Detected native devices','mount/EQDrive Wi-Fi adapter IP[:11880]','ASCOM Chooser...','EQDrive native','refreshBackendComboIfChanged')
need('src/core/remote_observatory_controller.cpp','updateBackendCatalogFromState')
need('tests/native_protocol_smoke.cpp','skywatcher_mc::decodeU24','skywatcher_mc::parseStatus')
for p in ['docs/EQDRIVE.md','docs/uk/EQDRIVE.md','docs/ASCOM_CLASSIC.md','docs/uk/ASCOM_CLASSIC.md','docs/SYNSCAN_NETWORK.md','docs/uk/SYNSCAN_NETWORK.md']:
    assert (root/p).is_file();checks.append((p,'exists'))
print(f'EQDrive / Classic ASCOM / SynScan network check: PASS ({len(checks)} assertions)')
