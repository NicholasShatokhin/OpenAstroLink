#pragma once
#include <cstdint>

struct qhyccd_handle {};
enum CONTROL_ID {
    CONTROL_EXPOSURE = 1,
    CONTROL_GAIN = 2,
    CONTROL_OFFSET = 3,
    CAM_COLOR = 4
};
static constexpr std::uint32_t QHYCCD_SUCCESS = 0;
static constexpr std::uint32_t QHYCCD_ERROR = 0xffffffffu;

extern "C" {
std::uint32_t InitQHYCCDResource();
std::uint32_t ReleaseQHYCCDResource();
std::uint32_t ScanQHYCCD();
std::uint32_t GetQHYCCDId(std::uint32_t, char *);
qhyccd_handle *OpenQHYCCD(char *);
std::uint32_t CloseQHYCCD(qhyccd_handle *);
std::uint32_t SetQHYCCDStreamMode(qhyccd_handle *, std::uint8_t);
std::uint32_t InitQHYCCD(qhyccd_handle *);
std::uint32_t SetQHYCCDDebayerOnOff(qhyccd_handle *, bool);
std::uint32_t GetQHYCCDChipInfo(qhyccd_handle *, double *, double *, std::uint32_t *, std::uint32_t *, double *, double *, std::uint32_t *);
std::uint32_t IsQHYCCDControlAvailable(qhyccd_handle *, CONTROL_ID);
std::uint32_t GetQHYCCDParamMinMaxStep(qhyccd_handle *, CONTROL_ID, double *, double *, double *);
double GetQHYCCDParam(qhyccd_handle *, CONTROL_ID);
std::uint32_t SetQHYCCDParam(qhyccd_handle *, CONTROL_ID, double);
std::uint32_t SetQHYCCDBinMode(qhyccd_handle *, std::uint32_t, std::uint32_t);
std::uint32_t SetQHYCCDResolution(qhyccd_handle *, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
std::uint32_t ExpQHYCCDSingleFrame(qhyccd_handle *);
std::uint32_t GetQHYCCDMemLength(qhyccd_handle *);
std::uint32_t GetQHYCCDSingleFrame(qhyccd_handle *, std::uint32_t *, std::uint32_t *, std::uint32_t *, std::uint32_t *, unsigned char *);
std::uint32_t CancelQHYCCDExposingAndReadout(qhyccd_handle *);
std::uint32_t BeginQHYCCDLive(qhyccd_handle *);
std::uint32_t GetQHYCCDLiveFrame(qhyccd_handle *, std::uint32_t *, std::uint32_t *, std::uint32_t *, std::uint32_t *, unsigned char *);
std::uint32_t StopQHYCCDLive(qhyccd_handle *);
}
