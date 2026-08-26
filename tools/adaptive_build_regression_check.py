from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
checks = 0

def require(path, token):
    global checks
    text = (root / path).read_text(encoding="utf-8")
    checks += 1
    if token not in text:
        raise SystemExit(f"FAIL {path}: missing {token}")
    return text

opencv = require(Path("src/backends/opencv_camera.cpp"), "#include <opencv2/imgproc.hpp>")
require(Path("CMakeLists.txt"), "COMPONENTS core imgproc imgcodecs videoio calib3d")
app = require(Path("src/core/application_controller.cpp"), "QJsonObject resultJson=solveToJson(lastResult)")
checks += 1
if 'out.result["' in app:
    raise SystemExit('FAIL application_controller.cpp: OperationOutcome::result is QJsonValue; mutate a QJsonObject before assigning it')
checks += 1
if app.count('QJsonObject resultJson=solveToJson(lastResult)') < 2:
    raise SystemExit('FAIL application_controller.cpp: both adaptive-solve success and failure paths must use mutable resultJson')
print(f"PASS adaptive MSVC build regression guard: {checks} assertions")
