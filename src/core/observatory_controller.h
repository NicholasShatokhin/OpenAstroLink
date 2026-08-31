#pragma once

#include "core/astro_types.h"
#include "algorithms/motion_estimator.h"
#include <QImage>
#include <QObject>
#include <QStringList>
#include <vector>

namespace oas {

// GUI-facing control contract. Implementations may execute the observatory core
// in-process (ApplicationController) or proxy every operation to an OAL node
// (RemoteObservatoryController). High-level algorithms MUST execute in the core,
// not in the remote GUI.
class ObservatoryController : public QObject {
    Q_OBJECT
public:
    explicit ObservatoryController(QObject *parent = nullptr) : QObject(parent) {}
    ~ObservatoryController() override = default;

    virtual QString controlMode() const = 0;
    virtual QString endpointDescription() const = 0;
    virtual bool isRemote() const = 0;

    virtual TelescopeProfile profile() const = 0;
    virtual void setProfile(const TelescopeProfile &profile) = 0;

    virtual QStringList cameraBackends() const = 0;
    virtual QStringList mountBackends() const = 0;
    virtual QStringList focuserBackends() const = 0;
    virtual QStringList solverBackends() const = 0;
    virtual bool selectSolver(const QString &name, QString *error = nullptr) = 0;
    virtual bool loadCatalog(const QString &path, QString *error = nullptr) = 0;
    virtual bool loadNeuralModel(const QString &path, QString *error = nullptr) = 0;

    virtual bool connectCamera(const QString &backend, const QString &endpoint, QString *error = nullptr) = 0;
    virtual bool connectGuideCamera(const QString &backend, const QString &endpoint, QString *error = nullptr) = 0;
    virtual bool connectMount(const QString &backend, const QString &endpoint, QString *error = nullptr) = 0;
    virtual bool connectFocuser(const QString &backend, const QString &endpoint, QString *error = nullptr) = 0;
    virtual bool disconnectCamera(QString *error = nullptr) = 0;
    virtual bool disconnectGuideCamera(QString *error = nullptr) = 0;
    virtual bool disconnectMount(QString *error = nullptr) = 0;
    virtual bool disconnectFocuser(QString *error = nullptr) = 0;
    virtual bool disconnectAll(QString *error = nullptr) = 0;

    virtual bool capture(const ExposureRequest &request, CameraFrame *out = nullptr, QString *error = nullptr) = 0;
    // Preferred UI/API path: exposure is an operation and returns immediately.
    virtual QString startCapture(const ExposureRequest &request, QString *error = nullptr) = 0;
    virtual QString startGuideCapture(const ExposureRequest &request, QString *error = nullptr) = 0;
    // Continuous operational preview. The core owns the camera resource until
    // the returned operation is cancelled. Live frames are never science-saved.
    virtual QString startLiveView(const LiveViewRequest &request, QString *error = nullptr) = 0;
    virtual SolveResult solveLast(const SolveHint &hint = {}) = 0;
    virtual QString startAdaptiveSolve(const AdaptiveSolveRequest &request, QString *error = nullptr) = 0;
    virtual AutofocusResult autofocus(const AutofocusRequest &request) = 0;
    virtual QString startAutofocus(const AutofocusRequest &request, QString *error = nullptr) = 0;
    virtual bool cancelOperation(const QString &operationId, QString *error = nullptr) = 0;
    virtual QJsonObject operation(const QString &operationId, QString *error = nullptr) const = 0;
    virtual QJsonArray operations(bool activeOnly = false) const = 0;
    virtual FrameMotion estimateLastMotion() = 0;
    virtual void requestSystemLocation() = 0;

    virtual bool slewMount(const EquatorialCoord &target, QString *error = nullptr) = 0;
    virtual bool abortMountMotion(QString *error = nullptr) = 0;
    virtual bool syncMount(const EquatorialCoord &target, QString *error = nullptr) = 0;
    virtual bool mountStatus(MountStatus &status, QString *error = nullptr) const = 0;
    virtual bool focuserStatus(FocuserStatus &status, QString *error = nullptr) const = 0;
    virtual bool moveFocuser(int position, QString *error = nullptr) = 0;
    virtual bool haltFocuser(QString *error = nullptr) = 0;
    virtual bool setMountTracking(bool enabled, TrackingRate rate = TrackingRate::Sidereal, QString *error = nullptr) = 0;
    virtual bool setMountSiteTime(const ObserverLocation &site, const QDateTime &utc, QString *error = nullptr) = 0;
    virtual bool parkMount(bool parked, QString *error = nullptr) = 0;
    // One-time persistent calibration of the active backend's physical park.
    // Native raw-axis mounts save current axes as both Home and Park; ASCOM
    // backends delegate to the standard SetPark method when supported.
    virtual bool setCurrentMountAsPark(QString *error = nullptr) = 0;
    virtual bool pulseGuide(GuideDirection direction, int durationMs, QString *error = nullptr) = 0;
    virtual bool manualMountSlew(int axis1Direction, int axis2Direction, int rateLevel, QString *error = nullptr) = 0;

    virtual GuidingStatus startGuiding() = 0;
    virtual GuidingStatus stopGuiding() = 0;
    virtual GuidingStatus guideUsingLastSolve() = 0;
    virtual GuidingStatus guidingStatus() const = 0;

    virtual void clearPolarSamples() = 0;
    virtual bool addPolarSample(QString *error = nullptr) = 0;
    virtual bool slewPolarRaOffset(double deltaDeg, QString *error = nullptr) = 0;
    virtual PolarAlignmentResult estimatePolarAlignment() = 0;

    virtual bool setSessionPlan(const QString &name, const std::vector<SessionTarget> &targets,
                                QString *error = nullptr) = 0;
    virtual bool startSession(QString *error = nullptr) = 0;
    virtual void stopSession() = 0;
    virtual SessionStatus sessionStatus() const = 0;

    virtual const CameraFrame &lastFrame() const = 0;
    virtual const CameraFrame &lastGuideFrame() const = 0;
    virtual const SolveResult &lastSolve() const = 0;

    // Available only for an embedded/local core. A remote GUI cannot rebind the
    // transport of the node it is currently using.
    virtual bool startOalServer(quint16 httpPort, bool websocketEnabled, quint16 wsPort,
                                QString *error = nullptr) = 0;
    virtual void stopOalServer() = 0;
    virtual bool oalRunning() const = 0;

    // Stellarium Telescope Control TCP bridge. This bridge intentionally maps
    // only mount position/GOTO; all other observatory functions remain OAL.
    virtual bool startStellariumServer(quint16 port, QString *error = nullptr) = 0;
    virtual void stopStellariumServer() = 0;
    virtual bool stellariumRunning() const = 0;
    virtual quint16 stellariumPort() const = 0;

    // Explicitly rescan native hardware. Ordinary metadata/state reads use a
    // cache and never touch USB/serial hardware.
    virtual bool refreshNativeDiscovery(QString *error = nullptr) = 0;
    virtual QJsonArray availableSerialPorts() const = 0;
    virtual QString nativeSerialPortOverride(const QString &driverId) const = 0;
    virtual bool setNativeSerialPortOverride(const QString &driverId, const QString &port,
                                             QString *error = nullptr) = 0;

    // Request a fresh complete state snapshot. Remote clients use this after
    // connecting/reconnecting so GUI state does not depend on having observed
    // earlier WebSocket events.
    virtual void refreshState() = 0;

signals:
    void logMessage(const QString &message);
    void frameCaptured(const QImage &image, const QString &frameId);
    void solveCompleted(const QJsonObject &result);
    void autofocusProgress(const QJsonObject &sample);
    void autofocusCompleted(const QJsonObject &result);
    void guidingChanged(const QJsonObject &status);
    void polarSampleCountChanged(int count);
    void sessionChanged(const QJsonObject &status);
    void stateChanged(const QJsonObject &state);
    void motionEstimated(const QJsonObject &motion);
    void profileChanged();
    void operationChanged(const QJsonObject &operation);
};

} // namespace oas
