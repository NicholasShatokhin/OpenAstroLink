from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks={
 'version':('CMakeLists.txt','VERSION 0.2.10.51'),
 'home config':('src/core/astro_types.h','autoHomeSync'),
 'home tolerance':('src/core/astro_types.h','homeToleranceDeg'),
 'home model':('src/core/mount_geometry.cpp','MountGeometryModel::syncHome'),
 'preserve operational sync':('src/core/mount_geometry.cpp','transformUnchanged'),
 'native auto home':('src/backends/oal_native_devices.cpp','tryAutoHomeSync'),
 'native core site':('src/backends/oal_native_devices.cpp','NativeOalMount::setSiteTime'),
 'remote safety transport':('src/core/remote_observatory_controller.cpp','maxGotoAxisDeltaDeg'),
 'server safety transport':('src/oal/oal_server.cpp','maxGotoAxisDeltaDeg'),
 'settings home persistence':('src/core/settings.cpp','mountGeometry/autoHomeSync'),
 'mount scroll':('src/gui/main_window.cpp','QScrollArea'),
 'home UI':('src/gui/main_window.cpp','Set current mechanical axes as Home'),
 'auto unpark direct':('src/core/application_controller.cpp','Mount auto-unparked for GOTO'),
 'auto unpark operation':('src/core/application_controller.cpp','Mount auto-unparked for GOTO operation'),
 'profile selective invalidation':('src/core/application_controller.cpp','existing Sync preserved'),
 'smoke home':('tests/mount_geometry_smoke.cpp','automatic Home sync failed'),
}
for name,(fn,needle) in checks.items():
    text=(root/fn).read_text(encoding='utf-8')
    if needle not in text: raise SystemExit(f'FAIL {name}: {needle!r} missing from {fn}')
    print('PASS',name)
print(f'mount home/remote profile v0.2.10.45: PASS ({len(checks)} assertions)')
