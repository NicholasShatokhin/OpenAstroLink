from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
required = [
    'CMakeLists.txt', 'src/app/main.cpp', 'src/node/main.cpp',
    'src/core/observatory_controller.h', 'src/core/application_controller.cpp',
    'src/core/remote_observatory_controller.cpp', 'src/oal/oal_server.cpp',
    'src/oal/driver_plugin_loader.cpp', 'src/backends/oal_native_devices.cpp',
    'drivers/reference_simulated/oal_driver_simulated.cpp',
    'src/gui/main_window.cpp', 'packaging/systemd/openastrolink-node.service.in',
    'config/stars_example.csv'
]
missing = [p for p in required if not (root / p).exists()]
if missing:
    print('Missing:', *missing, sep='\n  ')
    sys.exit(1)

def strip_cpp_literals(text: str) -> str:
    out=[]; i=0; n=len(text)
    while i<n:
        if text.startswith('//',i):
            j=text.find('\n',i+2); i=n if j<0 else j; out.append('\n'); continue
        if text.startswith('/*',i):
            j=text.find('*/',i+2); i=n if j<0 else j+2; out.append(' '); continue
        if text.startswith('R"',i):
            p=text.find('(',i+2)
            if p>=0:
                delim=text[i+2:p]; close=')'+delim+'"'; j=text.find(close,p+1)
                if j>=0: i=j+len(close); out.append('""'); continue
        if text[i] in {'"', "'"}:
            q=text[i]; i+=1
            while i<n:
                if text[i]=='\\': i+=2; continue
                if text[i]==q: i+=1; break
                i+=1
            out.append(q+q); continue
        out.append(text[i]); i+=1
    return ''.join(out)

for p in root.rglob('*'):
    if p.suffix in {'.cpp', '.h'}:
        text = strip_cpp_literals(p.read_text(errors='replace'))
        if text.count('{') != text.count('}'):
            print('Brace mismatch:', p.relative_to(root), text.count('{'), text.count('}'))
            sys.exit(2)
print('Project structure smoke check passed.')
