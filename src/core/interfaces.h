#pragma once

#include "core/astro_types.h"
#include <QStringList>
#include <QSize>
#include <memory>

namespace oas {

class IDevice {
public:
    virtual ~IDevice() = default;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual QString backendName() const = 0;
    virtual ConnectionState connectionState() const = 0;
    virtual bool connectDevice(QString *error = nullptr) = 0;
    virtual void disconnectDevice() = 0;
};

class ICamera : public IDevice {
public:
    virtual bool capture(const ExposureRequest &request, CameraFrame &frame, QString *error = nullptr) = 0;
    virtual QSize sensorSize() const = 0;
};

class IMount : public IDevice {
public:
    virtual bool status(MountStatus &status, QString *error = nullptr) = 0;
    virtual bool slewTo(const EquatorialCoord &target, QString *error = nullptr) = 0;
    virtual bool syncTo(const EquatorialCoord &target, QString *error = nullptr) = 0;
    virtual bool setTracking(bool enabled, QString *error = nullptr) = 0;
    virtual bool park(bool enabled, QString *error = nullptr) = 0;
    virtual bool pulseGuide(GuideDirection direction, int durationMs, QString *error = nullptr) = 0;
};

class IFocuser : public IDevice {
public:
    virtual bool status(FocuserStatus &status, QString *error = nullptr) = 0;
    virtual bool moveAbsolute(int position, QString *error = nullptr) = 0;
    virtual bool moveRelative(int delta, QString *error = nullptr) = 0;
    virtual bool halt(QString *error = nullptr) = 0;
};

class IPlateSolver {
public:
    virtual ~IPlateSolver() = default;
    virtual QString name() const = 0;
    virtual SolveResult solve(const CameraFrame &frame, const TelescopeProfile &profile,
                              const SolveHint &hint = {}) = 0;
};

class INeuralSolver : public IPlateSolver {
public:
    virtual bool loadModel(const QString &path, QString *error = nullptr) = 0;
};

} // namespace oas
