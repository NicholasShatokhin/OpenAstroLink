#pragma once
#include "core/interfaces.h"
#include <QMutex>
#include <atomic>

namespace oas {

class SimulatedCamera final : public ICamera {
public:
    QString id() const override { return "sim-camera"; }
    QString displayName() const override { return "Simulated star camera"; }
    QString backendName() const override { return "simulated"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool capture(const ExposureRequest &request, CameraFrame &frame, QString *error = nullptr) override;
    bool canAbortExposure() const override { return true; }
    bool abortExposure(QString *error = nullptr) override;
    QSize sensorSize() const override { return {1280, 960}; }
private:
    ConnectionState state_{ConnectionState::Disconnected};
    std::atomic_bool abortRequested_{false};
    std::atomic_int frameNo_{0};
};

class SimulatedMount final : public IMount {
public:
    QString id() const override { return "sim-mount"; }
    QString displayName() const override { return "Simulated equatorial mount"; }
    QString backendName() const override { return "simulated"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool status(MountStatus &status, QString *error = nullptr) override;
    bool slewTo(const EquatorialCoord &target, QString *error = nullptr) override;
    bool abortMotion(QString *error = nullptr) override;
    bool syncTo(const EquatorialCoord &target, QString *error = nullptr) override;
    bool setTracking(bool enabled, TrackingRate rate = TrackingRate::Sidereal, QString *error = nullptr) override;
    bool park(bool enabled, QString *error = nullptr) override;
    bool pulseGuide(GuideDirection direction, int durationMs, QString *error = nullptr) override;
private:
    mutable QMutex mutex_;
    ConnectionState state_{ConnectionState::Disconnected};
    MountStatus status_{};
};

class SimulatedFocuser final : public IFocuser {
public:
    QString id() const override { return "sim-focuser"; }
    QString displayName() const override { return "Simulated absolute focuser"; }
    QString backendName() const override { return "simulated"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool status(FocuserStatus &status, QString *error = nullptr) override;
    bool moveAbsolute(int position, QString *error = nullptr) override;
    bool moveRelative(int delta, QString *error = nullptr) override;
    bool halt(QString *error = nullptr) override;
private:
    ConnectionState state_{ConnectionState::Disconnected};
};

} // namespace oas
