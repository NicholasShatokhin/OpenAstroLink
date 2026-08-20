#ifdef OAS_HAVE_QHY
#include "backends/qhy_camera.h"

#include <QDateTime>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <vector>
#include <iterator>
#include <mutex>

namespace oas {
namespace {

bool qok(uint32_t result, QString *error, const char *what) {
    if (result == QHYCCD_SUCCESS) return true;
    if (error) *error = QString("%1 failed: QHY result=%2").arg(what).arg(result);
    return false;
}

bool qExposureAccepted(uint32_t result, QString *error) {
    if (result != QHYCCD_ERROR) return true; // SUCCESS or READ_DIRECTLY are both valid starts
    if (error) *error = "ExpQHYCCDSingleFrame failed";
    return false;
}

// QHY documents InitQHYCCDResource() as a process-level initialization call.
// Keep it alive for the lifetime of the node instead of repeatedly
// init/release cycling it during probe/reconnect operations.
struct QhySdkState {
    std::mutex mutex;
    bool attempted{false};
    bool ready{false};
    QString error;
    ~QhySdkState() {
        if (ready) ReleaseQHYCCDResource();
    }
};

QhySdkState &sdkState() {
    static QhySdkState state;
    return state;
}

bool ensureSdkResource(QString *error) {
    auto &state = sdkState();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.attempted) {
        state.attempted = true;
        const uint32_t rc = InitQHYCCDResource();
        state.ready = rc == QHYCCD_SUCCESS;
        if (!state.ready)
            state.error = QString("InitQHYCCDResource failed: QHY result=%1").arg(rc);
    }
    if (!state.ready && error) *error = state.error;
    return state.ready;
}

} // namespace

QStringList QhyCamera::scanCameraIds(QString *error) {
    QStringList ids;
    if (!ensureSdkResource(error)) return ids;
    const int count = int(ScanQHYCCD());
    for (int i = 0; i < count; ++i) {
        char id[128]{};
        if (GetQHYCCDId(i, id) == QHYCCD_SUCCESS) ids << QString::fromLatin1(id);
    }
    return ids;
}

bool QhyCamera::connectDevice(QString *error) {
    state_ = ConnectionState::Connecting;
    if (!ensureSdkResource(error)) {
        state_ = ConnectionState::Error;
        return false;
    }

    const int count = int(ScanQHYCCD());
    if (count <= 0) {
        if (error) *error = "No QHY cameras found";
        disconnectDevice();
        state_ = ConnectionState::Error;
        return false;
    }

    bool numeric = false;
    const int wantedIndex = selector_.trimmed().toInt(&numeric);
    int selected = -1;
    char selectedId[128]{};
    for (int i = 0; i < count; ++i) {
        char id[128]{};
        if (GetQHYCCDId(i, id) != QHYCCD_SUCCESS) continue;
        if ((numeric && i == wantedIndex) ||
            (!numeric && selector_.trimmed() == QString::fromLatin1(id))) {
            selected = i;
            std::copy(std::begin(id), std::end(id), std::begin(selectedId));
            break;
        }
    }
    if (selected < 0) {
        QStringList found;
        for (int i = 0; i < count; ++i) {
            char id[128]{};
            if (GetQHYCCDId(i, id) == QHYCCD_SUCCESS) found << QString::fromLatin1(id);
        }
        if (error)
            *error = QString("QHY selector '%1' not found. Cameras: %2")
                         .arg(selector_, found.isEmpty() ? "<none>" : found.join(", "));
        disconnectDevice();
        state_ = ConnectionState::Error;
        return false;
    }

    cameraId_ = QString::fromLatin1(selectedId);
    handle_ = OpenQHYCCD(selectedId);
    if (!handle_) {
        if (error) *error = "OpenQHYCCD returned null for " + cameraId_;
        disconnectDevice();
        state_ = ConnectionState::Error;
        return false;
    }

    // Single-frame / science mode. Read mode is intentionally left at the
    // camera/driver default until read-mode capability negotiation is added.
    if (!qok(SetQHYCCDStreamMode(handle_, 0), error, "SetQHYCCDStreamMode(single)") ||
        !qok(InitQHYCCD(handle_), error, "InitQHYCCD")) {
        disconnectDevice();
        state_ = ConnectionState::Error;
        return false;
    }

    if (IsQHYCCDControlAvailable(handle_, CONTROL_TRANSFERBIT) == QHYCCD_SUCCESS) {
        if (!qok(SetQHYCCDParam(handle_, CONTROL_TRANSFERBIT, 16), error, "SetQHYCCDParam(CONTROL_TRANSFERBIT=16)")) {
            disconnectDevice();
            state_ = ConnectionState::Error;
            return false;
        }
    }
    // Keep raw Bayer/mono data for science and ASTAP rather than debayering in SDK.
    SetQHYCCDDebayerOnOff(handle_, false);

    double chipW = 0, chipH = 0, pixW = 0, pixH = 0;
    uint32_t w = 0, h = 0, bpp = 0;
    if (!qok(GetQHYCCDChipInfo(handle_, &chipW, &chipH, &w, &h, &pixW, &pixH, &bpp),
             error, "GetQHYCCDChipInfo")) {
        disconnectDevice();
        state_ = ConnectionState::Error;
        return false;
    }
    sensorSize_ = {int(w), int(h)};
    if (!qok(SetQHYCCDBinMode(handle_, 1, 1), error, "SetQHYCCDBinMode(1x1)") ||
        !qok(SetQHYCCDResolution(handle_, 0, 0, w, h), error, "SetQHYCCDResolution(full)")) {
        disconnectDevice();
        state_ = ConnectionState::Error;
        return false;
    }

    abortRequested_.store(false);
    state_ = ConnectionState::Connected;
    return true;
}

void QhyCamera::disconnectDevice() {
    abortRequested_.store(true);
    if (handle_) {
        CancelQHYCCDExposingAndReadout(handle_);
        CloseQHYCCD(handle_);
        handle_ = nullptr;
    }
    state_ = ConnectionState::Disconnected;
}

bool QhyCamera::setOptionalParam(CONTROL_ID control, double value, const char *name, QString *error) {
    if (IsQHYCCDControlAvailable(handle_, control) != QHYCCD_SUCCESS) return true;

    double minV = 0, maxV = 0, step = 0;
    if (GetQHYCCDParamMinMaxStep(handle_, control, &minV, &maxV, &step) == QHYCCD_SUCCESS) {
        if (value < minV || value > maxV) {
            if (error)
                *error = QString("QHY %1=%2 outside supported range [%3,%4]")
                             .arg(name).arg(value).arg(minV).arg(maxV);
            return false;
        }
    }
    return qok(SetQHYCCDParam(handle_, control, value), error, name);
}

bool QhyCamera::configureReadout(const ExposureRequest &request, QString *error) {
    const int binX = std::max(1, request.binX);
    const int binY = std::max(1, request.binY);
    if (!qok(SetQHYCCDBinMode(handle_, binX, binY), error, "SetQHYCCDBinMode")) return false;

    const int binnedW = std::max(1, sensorSize_.width() / binX);
    const int binnedH = std::max(1, sensorSize_.height() / binY);
    int x = 0, y = 0, w = binnedW, h = binnedH;
    if (request.roi.width > 0 && request.roi.height > 0) {
        x = qBound(0, request.roi.x, binnedW - 1);
        y = qBound(0, request.roi.y, binnedH - 1);
        w = qBound(1, request.roi.width, binnedW - x);
        h = qBound(1, request.roi.height, binnedH - y);
    }
    return qok(SetQHYCCDResolution(handle_, uint32_t(x), uint32_t(y), uint32_t(w), uint32_t(h)),
               error, "SetQHYCCDResolution");
}

bool QhyCamera::capture(const ExposureRequest &request, CameraFrame &frame, QString *error) {
    if (!handle_ || state_ != ConnectionState::Connected) {
        if (error) *error = "QHY camera disconnected";
        return false;
    }
    abortRequested_.store(false);

    if (!configureReadout(request, error)) return false;
    if (!setOptionalParam(CONTROL_EXPOSURE, request.exposureSec * 1e6, "CONTROL_EXPOSURE", error)) return false;
    if (!setOptionalParam(CONTROL_GAIN, request.gain, "CONTROL_GAIN", error)) return false;
    if (!setOptionalParam(CONTROL_OFFSET, request.offset, "CONTROL_OFFSET", error)) return false;

    const uint32_t expResult = ExpQHYCCDSingleFrame(handle_);
    if (!qExposureAccepted(expResult, error)) return false;
    if (abortRequested_.load()) {
        if (error) *error = "QHY exposure cancelled";
        return false;
    }

    const uint32_t length = GetQHYCCDMemLength(handle_);
    if (length == 0) {
        if (error) *error = "GetQHYCCDMemLength returned zero";
        return false;
    }
    std::vector<unsigned char> data(length);
    uint32_t w = 0, h = 0, bpp = 0, channels = 0;
    if (!qok(GetQHYCCDSingleFrame(handle_, &w, &h, &bpp, &channels, data.data()),
             error, "GetQHYCCDSingleFrame"))
        return false;
    if (abortRequested_.load()) {
        if (error) *error = "QHY exposure cancelled";
        return false;
    }

    if (channels == 1 && bpp <= 8)
        frame.image = cv::Mat(int(h), int(w), CV_8UC1, data.data()).clone();
    else if (channels == 1 && bpp <= 16)
        frame.image = cv::Mat(int(h), int(w), CV_16UC1, data.data()).clone();
    else if (channels == 3 && bpp <= 8)
        frame.image = cv::Mat(int(h), int(w), CV_8UC3, data.data()).clone();
    else if (channels == 3 && bpp <= 16)
        frame.image = cv::Mat(int(h), int(w), CV_16UC3, data.data()).clone();
    else {
        if (error) *error = QString("Unsupported QHY frame: bpp=%1 channels=%2").arg(bpp).arg(channels);
        return false;
    }

    frame.id = "qhy-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzz");
    frame.capturedUtc = QDateTime::currentDateTimeUtc();
    frame.exposureSec = request.exposureSec;
    frame.gain = request.gain;
    frame.source = id();
    return true;
}

bool QhyCamera::abortExposure(QString *error) {
    if (!handle_) {
        if (error) *error = "QHY camera disconnected";
        return false;
    }
    abortRequested_.store(true);
    return qok(CancelQHYCCDExposingAndReadout(handle_), error,
               "CancelQHYCCDExposingAndReadout");
}

} // namespace oas
#endif
