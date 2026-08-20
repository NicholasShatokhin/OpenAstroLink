#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
checks={
    'operation manager source': ('src/core/operation_manager.cpp','OperationManager::submit'),
    'operation states': ('src/core/operation_manager.cpp','"queued"'),
    'resource locks': ('src/core/operation_manager.cpp','lockOwners_'),
    'cancel flag': ('src/core/operation_manager.cpp','cancelFlag->store'),
    'autofocus camera+focuser locks': ('src/core/application_controller.cpp','"autofocus.run",{"camera","focuser"}'),
    'mount lock': ('src/core/application_controller.cpp','"mount.slew",{"mount"}'),
    'capture lock guard': ('src/core/application_controller.cpp','ensureResourcesAvailable({"camera"}'),
    'operation REST list': ('src/oal/oal_server.cpp','/api/v1/operations"'),
    'operation REST cancel': ('src/oal/oal_server.cpp','/api/v1/operations/<arg>/cancel'),
    'autofocus 202': ('src/oal/oal_server.cpp','acceptedResponse(controller_->operation(id,nullptr))'),
    'mount abort': ('src/oal/oal_server.cpp','/api/v1/mounts/<arg>/abort'),
    'individual disconnect': ('src/oal/oal_server.cpp','/api/v1/devices/<arg>/disconnect'),
    'websocket operation event': ('src/core/application_controller.cpp','broadcast("operation",o)'),
}
failed=[]
for name,(file,needle) in checks.items():
    text=(root/file).read_text(encoding='utf-8')
    if needle not in text: failed.append(f'{name}: missing {needle!r} in {file}')
if failed:
    print('\n'.join(failed));sys.exit(1)
print('P0 async operation/resource-lock static checks passed.')
