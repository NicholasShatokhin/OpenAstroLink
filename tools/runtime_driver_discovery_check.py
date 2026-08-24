from pathlib import Path

def need(path,*tokens):
    s=Path(path).read_text(encoding='utf-8')
    for t in tokens:
        assert t in s, f'{path}: missing {t}'

need('src/oal/driver_plugin_loader.cpp','base.startsWith(QStringLiteral("oal_driver_")','Vendor runtime DLLs')
need('src/tools/hardware_probe.cpp','--gemini-port' if False else 'gemini-port','Serial ports visible to Qt','OAL_GEMINI_PORT')
need('drivers/gemini/oal_driver_gemini.cpp','probing %1 at 9600 baud','handshake failed (:02# -> EOK# expected)')
need('src/backends/gemini_eaf_focuser.cpp','compatibility adapter','native:oal.gemini/<device-id>')
print('runtime_driver_discovery_check: PASS')
