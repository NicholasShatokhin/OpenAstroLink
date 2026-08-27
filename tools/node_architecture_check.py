#!/usr/bin/env python3
from pathlib import Path
import re, sys
root=Path(__file__).resolve().parents[1]
checks={
 'core library': ('CMakeLists.txt','add_library(oas_core STATIC'),
 'node executable': ('CMakeLists.txt','add_executable(openastrolink-node'),
 'gui executable': ('CMakeLists.txt','add_executable(OpenAstroSuite'),
 'gui abstraction': ('src/gui/main_window.h','ObservatoryController *controller'),
 'remote proxy': ('src/core/remote_observatory_controller.cpp','api("autofocus/default/run")'),
 'remote polar': ('src/core/remote_observatory_controller.cpp','api("polar-align/estimate")'),
 'remote session': ('src/core/remote_observatory_controller.cpp','api("sessions")'),
 'node server': ('src/node/main.cpp','startOalServer'),
 'device config API': ('src/oal/oal_server.cpp','/api/v1/devices/connect'),
 'profile API': ('src/oal/oal_server.cpp','/api/v1/profile'),
 'backend discovery API': ('src/oal/oal_server.cpp','/api/v1/node/backends'),
 'systemd template': ('packaging/systemd/openastrolink-node.service.in','ExecStart=/usr/local/bin/openastrolink-node'),
}
failed=[]
for name,(file,needle) in checks.items():
    text=(root/file).read_text(encoding='utf-8')
    if needle not in text: failed.append(f'{name}: missing {needle!r} in {file}')
# All project-local quoted includes must resolve.
for f in list(root.glob('src/**/*.cpp'))+list(root.glob('src/**/*.h')):
    for inc in re.findall(r'#include\s+"([^"]+)"', f.read_text(encoding='utf-8')):
        if not (f.parent/inc).resolve().exists() and not (root/'src'/inc).exists() and not (root/'include'/inc).exists():
            failed.append(f'{f.relative_to(root)}: unresolved include {inc}')
if failed:
    print('\n'.join(failed));sys.exit(1)
print('Headless-node / local-or-remote GUI architecture checks passed.')
