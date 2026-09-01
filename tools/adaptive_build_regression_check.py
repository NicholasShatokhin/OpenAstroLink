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
checks += 1
if 'const auto cam=camera_,mount=mount_' in app or 'const auto cam = camera_, mount = mount_' in app:
    raise SystemExit('FAIL application_controller.cpp: MSVC requires heterogeneous auto declarations to be split')
checks += 1
if 'const auto cam=camera_;' not in app or 'const auto mount=mount_;' not in app:
    raise SystemExit('FAIL application_controller.cpp: planetary calibration camera/mount captures must use separate auto declarations')
print(f"PASS adaptive MSVC build regression guard: {checks} assertions")
