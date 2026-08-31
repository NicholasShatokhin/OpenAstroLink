from pathlib import Path
root=Path(__file__).resolve().parents[1]
def text(p):return (root/p).read_text(encoding='utf-8',errors='ignore')
assert '"backends",QJsonObject' in text('src/core/application_controller.cpp')
assert text('src/core/remote_observatory_controller.cpp').count('updateBackendCatalogFromState') >= 3
assert 'refreshBackendComboIfChanged' in text('src/gui/main_window.cpp')
assert 'Detected native devices' in text('src/gui/main_window.cpp')
assert '"gemini-eaf"<<' not in text('src/core/application_controller.cpp')
wifi=text('src/backends/synscan_network_mount.cpp')
assert '11880' in wifi and '11882' not in wifi and 'Motor Controller' in wifi
app=text('src/backends/synscan_app_mount.cpp');assert '11881' in app and 'ServerVersion' in app
eq=text('drivers/eqdrive/oal_driver_eqdrive.cpp')
for token in ['official EQDrive ASTEP','read("St"','read("Pos"','read("Cg"','Goto %1 %2','HIL_SAFETY_LIMIT','Access is denied','gem-polar-telescope-frame-v6']:
    assert token in eq, token
assert 'EQMOD-compatible Motor Controller protocol' in eq
assert 'oal.skywatcher' in text('src/core/application_controller.cpp')
print('Native catalogue / direct SynScan Wi-Fi / EQDrive protocol check: PASS')
