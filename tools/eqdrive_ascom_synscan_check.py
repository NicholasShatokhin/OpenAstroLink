from pathlib import Path
import json

root = Path(__file__).resolve().parents[1]
checks = []

def need(path, *tokens):
    text = (root / path).read_text(encoding='utf-8')
    for token in tokens:
        assert token in text, f'{path}: missing {token!r}'
        checks.append((path, token))
    return text

cmake = need('CMakeLists.txt',
    'project(OpenAstroSuite VERSION 0.2.10.16',
    'OAS_ENABLE_NATIVE_EQDRIVE', 'OAS_ENABLE_ASCOM_CLASSIC',
    'oal_driver_eqdrive', 'oas-ascom-host', 'ole32', 'oleaut32',
    'src/backends/ascom_classic_mount.cpp', 'src/backends/synscan_app_mount.cpp',
    'src/backends/synscan_network_mount.cpp')
assert 'RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"' in cmake
checks.append(('CMakeLists.txt','ASCOM helper beside node/GUI'))

manifest = json.loads((root/'drivers/eqdrive/oal_driver_eqdrive.manifest.json').read_text())
assert manifest['driverId'] == 'oal.eqdrive'
assert manifest['version'] == '0.2.10.16'
assert 'mount' in manifest['deviceClasses']
checks += [('eqdrive manifest','driverId'),('eqdrive manifest','version'),('eqdrive manifest','mount')]

need('drivers/eqdrive/astep_protocol.h',
    'parseStatus', 'parsePosition', 'parseSpeed',
    '0x0100u', '0x0001u', '0x1000u', '0x0010u')
need('drivers/eqdrive/oal_driver_eqdrive.cpp',
    '"oal.eqdrive"', 'probing %1 for EQDrive ASTEP',
    'exchange(d,"St"', 'exchange(d,"FWs"', 'exchange(d,"Cg"',
    'if(m=="mount.status")', 'if(m=="mount.sync")', 'if(m=="mount.slew")',
    'if(m=="mount.abort")', 'if(m=="mount.setTracking")', 'if(m=="mount.pulseGuide")',
    '"coordinateValid",d.coordinateSynced', 'sync-anchor-v1',
    'Native EQDrive park/meridian model is not exposed')

need('tools/ascom_host/main.cpp',
    'ASCOM.Utilities.Chooser', 'CoCreateInstance', 'IID_IDispatch',
    'SlewToCoordinatesAsync', 'AbortSlew', 'SyncToCoordinates',
    'PulseGuide', 'SetupDialog', 'COINIT_APARTMENTTHREADED')
need('src/backends/ascom_classic_mount.cpp',
    'ASCOM helper', 'chooseTelescope', 'setupTelescope')

need('src/backends/synscan_app_mount.cpp',
    'RightAscensionDeclinationGet', 'SlewToCoordinatesAsync',
    'boolGet("Tracking"', 'ServerVersion', '11881', 'PulseGuide')
need('src/backends/synscan_network_mount.cpp',
    '11882', 'echoProbe', 'gotoRaDec', 'syncRaDec')

app = need('src/core/application_controller.cpp',
    'OAL_EQDRIVE_PORT', 'inferPersistedSerialPort',
    'Persisted %1 binding migrated', 'Gemini persisted binding updated',
    '"synscan-app"', '"synscan-wifi"', '"ascom-classic"',
    'coordinateValid')
assert 'oal.skywatcher' in app and 'oal.eqdrive' in app
checks.append(('application_controller.cpp','SkyWatcher retained + EQDrive added'))

need('src/gui/main_window.cpp',
    'ASCOM Chooser...', 'ASCOM Properties...', 'EQDrive (ASTEP)',
    'Apply & rediscover native devices', 'nativeSerialPortOverride',
    'IP of phone/PC running SynScan Pro', 'SynScan App host:11882')
need('src/node/main.cpp', 'eqdrive-port', 'gemini-port', 'skywatcher-port')
need('src/tools/hardware_probe.cpp', 'Native EQDrive:', 'eqdrive-port')
need('src/integrations/stellarium_telescope_server.cpp', '!status.coordinateValid')
need('src/core/astro_types.h', 'bool coordinateValid{true};')
need('tests/native_protocol_smoke.cpp', 'eqdrive::parseStatus', 'eqdrive::parsePosition', 'eqdrive::parseSpeed')

for p in ['docs/EQDRIVE.md','docs/uk/EQDRIVE.md','docs/ASCOM_CLASSIC.md','docs/uk/ASCOM_CLASSIC.md','docs/SYNSCAN_NETWORK.md','docs/uk/SYNSCAN_NETWORK.md']:
    assert (root/p).is_file(), f'missing {p}'
    checks.append((p,'exists'))

print(f'EQDrive / Classic ASCOM / SynScan network check: PASS ({len(checks)} assertions)')
