from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
required = [
    'CMakeLists.txt', 'src/app/main.cpp', 'src/node/main.cpp',
    'src/core/observatory_controller.h', 'src/core/application_controller.cpp',
    'src/core/remote_observatory_controller.cpp', 'src/oal/oal_server.cpp',
    'src/gui/main_window.cpp', 'packaging/systemd/openastrolink-node.service.in',
    'config/stars_example.csv'
]
missing = [p for p in required if not (root / p).exists()]
if missing:
    print('Missing:', *missing, sep='\n  ')
    sys.exit(1)
for p in root.rglob('*'):
    if p.suffix in {'.cpp', '.h'}:
        text = p.read_text(errors='replace')
        if text.count('{') != text.count('}'):
            print('Brace mismatch:', p.relative_to(root))
            sys.exit(2)
print('Project structure smoke check passed.')
