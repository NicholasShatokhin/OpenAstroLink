#pragma once
#ifdef OAS_HAVE_QHY
#include "core/interfaces.h"
#include <qhyccd.h>
#include <atomic>
#include <utility>

namespace oas {

class QhyCamera final : public ICamera {
public:
    // selector may be a numeric ScanQHYCCD index ("0") or the exact QHY camera ID.
    explicit QhyCamera(QString selector = "0") : selector_(std::move(selector)) {}
    ~QhyCamera() override { disconnectDevice(); }

    QString id() const override { return cameraId_.isEmpty() ? "qhy" : "qhy-" + cameraId_; }
    QString displayName() const override { return cameraId_.isEmpty() ? "QHY camera" : cameraId_; }
    QString backendName() const override { return "qhy"; }
    ConnectionState connectionState() const override { return state_; }

    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool capture(const ExposureRequest &, CameraFrame &, QString *error = nullptr) override;
    bool canAbortExposure() const override { return true; }
    bool abortExposure(QString *error = nullptr) override;
    QSize sensorSize() const override { return sensorSize_; }

    static QStringList scanCameraIds(QString *error = nullptr);

private:
    bool setOptionalParam(CONTROL_ID control, double value, const char *name, QString *error);
    bool configureReadout(const ExposureRequest &request, QString *error);

    QString selector_{"0"};
    QString cameraId_;
    ConnectionState state_{ConnectionState::Disconnected};
    qhyccd_handle *handle_{nullptr};
    QSize sensorSize_;
    std::atomic_bool abortRequested_{false};
};

} // namespace oas
#endif
