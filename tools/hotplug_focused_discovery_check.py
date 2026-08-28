from pathlib import Path

root = Path(__file__).resolve().parents[1]
checks = []

def need(path, *tokens):
    text = (root / path).read_text(encoding='utf-8')
    for token in tokens:
        assert token in text, f'{path}: missing {token!r}'
        checks.append((path, token))

need('drivers/qhy/oal_driver_qhy.cpp',
     'QHY hot-plug recovery: recycling SDK resource',
     'ReleaseQHYCCDResource()',
     'InitQHYCCDResource()',
     'previousCount==0',
     '!anyCameraConnected()',
     'sdkLifecycleMutex')
need('src/core/application_controller.cpp',
     'inferPersistedSerialPort',
     'Focused reconnect discovery: oal.gemini',
     'ScopedEnvOverride',
     'qhy=%3 gemini=%4 skywatcher=%5 eqdrive=%6')
need('drivers/skywatcher/oal_driver_skywatcher.cpp',
     'QByteArrayLiteral(":e1\\r")',
     "true,'\\r'",
     'direct Sky-Watcher Motor Controller protocol detected')
need('drivers/qhy/oal_driver_qhy.manifest.json', '0.2.10.19')
need('drivers/gemini/oal_driver_gemini.manifest.json', '0.2.10.19')
need('drivers/skywatcher/oal_driver_skywatcher.manifest.json', '0.2.10.19')

print(f'hotplug/focused discovery check: PASS ({len(checks)} assertions)')
