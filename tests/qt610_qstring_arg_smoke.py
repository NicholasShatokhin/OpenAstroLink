from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / 'src/backends/synscan_network_mount.cpp').read_text(encoding='utf-8')

bad = 'QString("Axis %1 stop could not be confirmed: %2").arg(axis,last)'
good = 'QString("Axis %1 stop could not be confirmed: %2").arg(axis).arg(last)'

assert bad not in source, 'Qt 6.10 rejects mixed QString::arg(int, QString) variadic call'
assert good in source, 'Expected Qt 6.10-compatible chained QString::arg call missing'
print('qt610_qstring_arg_smoke: PASS')
