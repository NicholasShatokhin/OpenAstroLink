#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
canon = (root / 'drivers/canon/oal_driver_canon_edsdk.cpp').read_text(encoding='utf-8')
server = (root / 'src/oal/oal_server.cpp').read_text(encoding='utf-8')

assert '#define __int64 long long' in canon
assert 'using WCHAR = wchar_t;' in canon
assert '#include <EDSDK.h>' in canon
assert canon.index('#define __int64 long long') < canon.index('#include <EDSDK.h>')
assert 'QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)' in server
assert 'http_.bind(&tcp_);' in server

# Reproduce the two Canon Linux header spellings that failed on the physical
# AArch64 cross-build and verify our chosen shim is accepted by GCC.
with tempfile.TemporaryDirectory() as td:
    td = Path(td)
    source = td / 'probe.cpp'
    source.write_text(r'''
#include <cwchar>
#ifndef __int64
#define __int64 long long
#endif
#ifndef WCHAR
using WCHAR = wchar_t;
#endif

typedef __int64 EdsInt64;
typedef unsigned __int64 EdsUInt64;
struct EdsDirectoryItemInfo { EdsUInt64 size; };
void api(const WCHAR *, EdsUInt64);
int main() { EdsDirectoryItemInfo i{}; return i.size == 0 ? 0 : 1; }
''', encoding='utf-8')
    subprocess.run(['g++', '-std=c++20', '-fsyntax-only', str(source)], check=True)

print('Linux/ARM portability smoke: PASS')
