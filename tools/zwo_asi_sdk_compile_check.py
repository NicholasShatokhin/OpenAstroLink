#!/usr/bin/env python3
from pathlib import Path
import shutil, subprocess, tempfile, textwrap, sys

root = Path(__file__).resolve().parents[1]
driver = root / 'drivers/zwo_asi/oal_driver_zwo_asi.cpp'
compiler = shutil.which('g++') or shutil.which('clang++')
if not compiler:
    print('ZWO ASI SDK compile-shape check: SKIP (no C++ compiler)')
    raise SystemExit(0)

stub = r'''
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
typedef int ASI_ERROR_CODE;
enum { ASI_SUCCESS=0, ASI_ERROR_INVALID_SEQUENCE=1 };
typedef int ASI_BOOL; enum { ASI_FALSE=0, ASI_TRUE=1 };
typedef enum { ASI_BAYER_RG=0, ASI_BAYER_BG, ASI_BAYER_GR, ASI_BAYER_GB } ASI_BAYER_PATTERN;
typedef enum { ASI_IMG_RAW8=0, ASI_IMG_RGB24, ASI_IMG_RAW16, ASI_IMG_Y8, ASI_IMG_END=-1 } ASI_IMG_TYPE;
typedef enum { ASI_GAIN=0, ASI_EXPOSURE=1, ASI_OFFSET=2 } ASI_CONTROL_TYPE;
typedef enum { ASI_EXP_IDLE=0, ASI_EXP_WORKING=1, ASI_EXP_SUCCESS=2, ASI_EXP_FAILED=3 } ASI_EXPOSURE_STATUS;
typedef struct {
  char Name[64]; int CameraID; long MaxHeight; long MaxWidth; ASI_BOOL IsColorCam; ASI_BAYER_PATTERN BayerPattern;
  int SupportedBins[16]; ASI_IMG_TYPE SupportedVideoFormat[8]; double PixelSize; ASI_BOOL MechanicalShutter;
  ASI_BOOL ST4Port; ASI_BOOL IsCoolerCam; ASI_BOOL IsUSB3Host; ASI_BOOL IsUSB3Camera; float ElecPerADU; int BitDepth;
  ASI_BOOL IsTriggerCam; char Unused[16];
} ASI_CAMERA_INFO;
typedef struct {
  char Name[64]; char Description[128]; long MaxValue; long MinValue; long DefaultValue;
  ASI_BOOL IsAutoSupported; ASI_BOOL IsWritable; ASI_CONTROL_TYPE ControlType; char Unused[32];
} ASI_CONTROL_CAPS;
ASI_ERROR_CODE ASIGetCameraProperty(ASI_CAMERA_INFO*, int);
ASI_ERROR_CODE ASIGetCameraPropertyByID(int, ASI_CAMERA_INFO*);
int ASIGetNumOfConnectedCameras(void);
ASI_ERROR_CODE ASIOpenCamera(int); ASI_ERROR_CODE ASIInitCamera(int); ASI_ERROR_CODE ASICloseCamera(int);
ASI_ERROR_CODE ASIGetNumOfControls(int,int*); ASI_ERROR_CODE ASIGetControlCaps(int,int,ASI_CONTROL_CAPS*);
ASI_ERROR_CODE ASISetControlValue(int,ASI_CONTROL_TYPE,long,ASI_BOOL);
ASI_ERROR_CODE ASISetROIFormat(int,int,int,int,ASI_IMG_TYPE); ASI_ERROR_CODE ASISetStartPos(int,int,int);
ASI_ERROR_CODE ASIStartVideoCapture(int); ASI_ERROR_CODE ASIStopVideoCapture(int);
ASI_ERROR_CODE ASIGetVideoData(int,unsigned char*,long,int);
ASI_ERROR_CODE ASIStartExposure(int,ASI_BOOL); ASI_ERROR_CODE ASIStopExposure(int);
ASI_ERROR_CODE ASIGetExpStatus(int,ASI_EXPOSURE_STATUS*); ASI_ERROR_CODE ASIGetDataAfterExp(int,unsigned char*,long);
#ifdef __cplusplus
}
#endif
'''
with tempfile.TemporaryDirectory(prefix='oal-zwo-sdk-') as td:
    td = Path(td)
    (td / 'ASICamera2.h').write_text(stub, encoding='utf-8')
    cmd = [compiler, '-std=c++20', '-fsyntax-only', f'-I{root / "include"}', f'-I{td}', str(driver)]
    proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if proc.returncode:
        print('ZWO ASI SDK compile-shape check: FAIL')
        print(proc.stdout)
        raise SystemExit(proc.returncode)
print('ZWO ASI SDK compile-shape check: PASS')
