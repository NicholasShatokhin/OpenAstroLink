from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
main = (root / "src/node/main.cpp").read_text(encoding="utf-8")
ctl_h = (root / "src/core/application_controller.h").read_text(encoding="utf-8")
ctl = (root / "src/core/application_controller.cpp").read_text(encoding="utf-8")
op = (root / "src/core/operation_manager.cpp").read_text(encoding="utf-8")
ws = (root / "src/oal/oal_ws_server.cpp").read_text(encoding="utf-8")
stell = (root / "src/integrations/stellarium_telescope_server.cpp").read_text(encoding="utf-8")

checks = {
    "Windows console handler installed": "SetConsoleCtrlHandler(&oalConsoleCtrlHandler, TRUE)" in main,
    "handler does not call Qt": "InterlockedIncrement(&gConsoleInterruptCount)" in main and "QCoreApplication::quit()" not in main.split("BOOL WINAPI oalConsoleCtrlHandler",1)[1].split("}",1)[0],
    "main-thread poll requests quit": "consoleShutdownRequested()" in main and "QCoreApplication::quit();" in main,
    "second Ctrl+C escape hatch": "return count == 1 ? TRUE : FALSE;" in main,
    "cleanup hooked before dispatcher teardown": "QCoreApplication::aboutToQuit" in main and "controller.shutdown();" in main,
    "controller has idempotent shutdown": "void shutdown();" in ctl_h and "if(shuttingDown_)return;" in ctl,
    "listeners stop before operations": ctl.find("stellariumServer_->stop()") < ctl.find("operations_.shutdown()") and ctl.find("oalWsServer_->stop()") < ctl.find("operations_.shutdown()"),
    "operations finish before devices disconnect": ctl.find("operations_.shutdown()") < ctl.find("disconnectDevices(false)", ctl.find("void ApplicationController::shutdown")),
    "drivers cleared during orderly shutdown": "if(driverLoader_)driverLoader_->clear();" in ctl,
    "worker tail events drained": "sendPostedEvents(nullptr,QEvent::DeferredDelete)" in op,
    "websocket clients synchronously destroyed": "delete c;" in ws,
    "stellarium clients synchronously destroyed": "delete socket;" in stell,
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    print("Graceful node shutdown check: FAIL")
    for name in failed:
        print(" -", name)
    sys.exit(1)
print(f"Graceful node shutdown check: PASS ({len(checks)}/{len(checks)})")
