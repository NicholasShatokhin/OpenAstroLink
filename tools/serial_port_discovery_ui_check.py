#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=[]
def need(path,*tokens):
    text=(root/path).read_text(encoding="utf-8",errors="replace")
    for token in tokens: checks.append((token in text,f"{path}: {token}"))
need("drivers/gemini/oal_driver_gemini.cpp","QSerialPortInfo::availablePorts()","OAL_GEMINI_PORT","candidatePorts()")
need("src/node/main.cpp","gemini-port","OAL_GEMINI_PORT","skywatcher-port")
need("src/core/settings.cpp","nativeSerialPort","saveNativeSerialPort")
need("src/core/application_controller.cpp","availableSerialPorts()","setNativeSerialPortOverride","OAL_GEMINI_PORT","OAL_SKYWATCHER_PORT")
need("src/core/remote_observatory_controller.cpp","system/serial-ports","drivers/serial-port")
need("src/oal/oal_server.cpp","/api/v1/system/serial-ports","/api/v1/drivers/serial-port")
need("src/gui/main_window.cpp","Native serial discovery","Auto — scan all serial ports","Apply port & rediscover selected serial driver")
failed=[m for ok,m in checks if not ok]
if failed:
    print("Serial-port discovery/UI check: FAIL")
    for m in failed: print(" -",m)
    raise SystemExit(1)
print(f"Serial-port discovery/UI check: PASS ({len(checks)} assertions)")
