#pragma once
#include "core/interfaces.h"
#include <opencv2/videoio.hpp>

namespace oas {
class OpenCvCamera final : public ICamera {
public:
    explicit OpenCvCamera(int index = 0) : index_(index) {}
    QString id() const override { return QString("opencv-%1").arg(index_); }
    QString displayName() const override { return QString("OpenCV/UVC camera %1").arg(index_); }
    QString backendName() const override { return "opencv"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool capture(const ExposureRequest &request, CameraFrame &frame, QString *error = nullptr) override;
    QSize sensorSize() const override;
private:
    int index_{0};
    ConnectionState state_{ConnectionState::Disconnected};
    cv::VideoCapture capture_;
};
}
