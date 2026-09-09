# Native ZWO ASI camera and EAF focuser drivers

> Current synchronized snapshot: **v0.2.10.58-buildfix9 (2026-09-09)**. See `STATUS.md`, `CURRENT_CHECKLIST.md` and `RELEASE_0.2.10.58.md` for current qualification boundaries.



> **Current development checkpoint:** v0.2.10.58-buildfix9. Native ZWO ASI/EAF remain default OAL drivers; ARM64 vendor libraries have passed the full Raspberry Pi cross-build.

Version: 0.2.10.5

## Purpose

ZWO is handled as a first-class native OpenAstroLink hardware family. The reference path is not `ZWO → INDI → OAL`; it is `ZWO SDK → native OAL ABI v2 → OpenAstroLink core`.

## oal.zwo.asi

The ASI driver enumerates every connected camera and creates a distinct OAL device. This makes two-camera installations possible: for example a cooled ASI main imager plus a small ASI guide camera, or a QHY/Canon main camera plus ASI guide camera.

Implemented capabilities include camera identity, sensor geometry, pixel size/bit depth/color information, supported binning/output formats, gain and offset ranges, ROI, single-frame exposure, exposure cancellation and native frame publication. The SDK also exposes video capture; the driver advertises the transport capability, while the full OAL streaming operation/ring-buffer profile remains a separate production-hardening task.

CMake options:

```text
OAS_ENABLE_NATIVE_ZWO_ASI=ON
ZWO_ASI_ROOT=/path/to/sdk
# or exact paths:
ZWO_ASI_INCLUDE_DIR=/path/containing/ASICamera2.h
ZWO_ASI_LIBRARY=/path/to/library
```

## oal.zwo.eaf

The EAF driver enumerates focusers and supports connect/disconnect, current position, absolute/relative move, halt/cancellation, moving state, temperature when available, maximum step, reverse and backlash capability reporting.

CMake options:

```text
OAS_ENABLE_NATIVE_ZWO_EAF=ON
ZWO_EAF_ROOT=/path/to/sdk
# or exact paths:
ZWO_EAF_INCLUDE_DIR=/path/containing/EAF_focuser.h
ZWO_EAF_LIBRARY=/path/to/library
```

## Hardware validation required

The source has compile/API-shape validation against SDK-compatible headers, but production status requires hardware-in-the-loop tests on representative ASI cameras and EAF generations on the target Raspberry Pi. Required tests include repeated exposure, abort, ROI/binning, two simultaneous ASI cameras, disconnect/reconnect, EAF motion/halt/limits/temperature and autofocus repeatability.

## Windows EAF build/runtime qualification — 2026-09-08

CMake now refuses the staged `EAF_focuser-static.lib` for the plugin driver and recovers to the DLL import library `EAF_focuser.lib`, pairing a same-architecture runtime DLL. Fresh buildfix9 configure/build passes. The remaining Windows acceptance test is node-registry loading of `oal.zwo.eaf`, followed by real EAF HIL.

