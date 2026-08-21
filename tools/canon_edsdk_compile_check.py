from pathlib import Path
import shutil, subprocess, sys
root=Path(__file__).resolve().parents[1]
cxx=shutil.which('g++') or shutil.which('clang++')
if not cxx:
    print('SKIP Canon EDSDK API-shape compile: no g++/clang++')
    raise SystemExit(0)
cmd=[cxx,'-std=c++20','-Wall','-Wextra','-Wpedantic','-Werror','-fsyntax-only',
     f'-I{root / "tests/stubs/edsdk"}', f'-I{root / "include"}', str(root/'drivers/canon/oal_driver_canon_edsdk.cpp')]
subprocess.run(cmd,check=True)
print('Canon EDSDK API-shape compile: PASS')
