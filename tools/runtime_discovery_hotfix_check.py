from pathlib import Path
import json

root = Path(__file__).resolve().parents[1]
loader_h = (root/'src/oal/driver_plugin_loader.h').read_text(encoding='utf-8')
loader_cpp = (root/'src/oal/driver_plugin_loader.cpp').read_text(encoding='utf-8')
app_cpp = (root/'src/core/application_controller.cpp').read_text(encoding='utf-8')
server_cpp = (root/'src/oal/oal_server.cpp').read_text(encoding='utf-8')
cmake = (root/'CMakeLists.txt').read_text(encoding='utf-8')

assert 'QJsonArray refreshDevices(QStringList *errors=nullptr);' in loader_h
assert 'return deviceCache_;' in loader_cpp
assert 'refreshDevices(errors);' in loader_cpp
assert 'Native OAL async discovery refreshed' in app_cpp
assert '/api/v1/drivers/refresh' in server_cpp
assert 'oal_stage_windows_runtime' in cmake
for var in ['QHYCCD_RUNTIME_DIR','ZWO_ASI_RUNTIME_DIR','ZWO_EAF_RUNTIME_DIR','CANON_EDSDK_RUNTIME_DIR']:
    assert var in cmake
for rel in ['drivers/zwo_asi/oal_driver_zwo_asi.manifest.json','drivers/zwo_eaf/oal_driver_zwo_eaf.manifest.json']:
    obj=json.loads((root/rel).read_text(encoding='utf-8'))
    assert obj['schema']=='org.openastrolink.driver-manifest/v2'
    assert isinstance(obj['permissions'],list)
    assert obj['abiVersion']==2
print('Runtime/discovery hotfix check: PASS')
