from pathlib import Path
root=Path(__file__).resolve().parents[1]
loader=(root/'src/oal/driver_plugin_loader.cpp').read_text()
serial=(root/'drivers/common_blocking_serial_session.h').read_text()
gemini=(root/'drivers/gemini/oal_driver_gemini.cpp').read_text()
for needle in ['QThread::currentThread() == thread()', 'emit driverLog(driver, level, message)']:
    assert needle in loader, needle
for needle in ['lastExchangeDiagnostic()', 'bytesToWrite() > 0', 'read timeout; received']:
    assert needle in serial, needle
for needle in ['first :02# exchange', 'recovery :02# exchange', "probeReply.toHex(' ')"]:
    assert needle in gemini, needle
print('gemini_serial_diagnostics_check: PASS')
