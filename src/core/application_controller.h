#pragma once
#include "algorithms/autofocus_engine.h"
#include "algorithms/guiding_engine.h"
#include "algorithms/motion_estimator.h"
#include "algorithms/star_detector.h"
#include "algorithms/polar_alignment.h"
#include "algorithms/scheduler.h"
#include "core/interfaces.h"
#include "core/operation_manager.h"
#include "core/observatory_controller.h"
#include "core/settings.h"
#include <memory>

#ifdef OAS_HAVE_POSITIONING
class QGeoPositionInfoSource;
#endif

namespace oas {
class OalServer;
class OalWsServer;
class StarCatalog;
class AstapSolver;

class ApplicationController final : public ObservatoryController {
    Q_OBJECT
public:
    explicit ApplicationController(QObject *parent=nullptr);
    ~ApplicationController() override;

    QString controlMode() const override { return "embedded-core"; }
    QString endpointDescription() const override { return "in-process"; }
    bool isRemote() const override { return false; }

    TelescopeProfile profile() const override{return profile_;}
    void setProfile(const TelescopeProfile &profile) override;
    QStringList cameraBackends() const override;
    QStringList mountBackends() const override;
    QStringList focuserBackends() const override;
    QStringList solverBackends() const override;
    bool selectSolver(const QString &name,QString *error=nullptr) override;
    bool loadCatalog(const QString &path,QString *error=nullptr) override;
    bool loadNeuralModel(const QString &path,QString *error=nullptr) override;

    bool connectCamera(const QString &backend,const QString &endpoint,QString *error=nullptr) override;
    bool connectMount(const QString &backend,const QString &endpoint,QString *error=nullptr) override;
    bool connectFocuser(const QString &backend,const QString &endpoint,QString *error=nullptr) override;
    bool disconnectCamera(QString *error=nullptr) override;
    bool disconnectMount(QString *error=nullptr) override;
    bool disconnectFocuser(QString *error=nullptr) override;
    bool disconnectAll(QString *error=nullptr) override;
    bool restoreConfiguredDevices(QStringList *errors=nullptr);

    bool capture(const ExposureRequest &request,CameraFrame *out=nullptr,QString *error=nullptr) override;
    QString startCapture(const ExposureRequest &request,QString *error=nullptr) override;
    SolveResult solveLast(const SolveHint &hint={}) override;
    AutofocusResult autofocus(const AutofocusRequest &request) override;
    QString startAutofocus(const AutofocusRequest &request,QString *error=nullptr) override;
    bool cancelOperation(const QString &operationId,QString *error=nullptr) override;
    QJsonObject operation(const QString &operationId,QString *error=nullptr) const override;
    QJsonArray operations(bool activeOnly=false) const override;
    FrameMotion estimateLastMotion() override;
    void requestSystemLocation() override;

    bool slewMount(const EquatorialCoord &target,QString *error=nullptr) override;
    bool abortMountMotion(QString *error=nullptr) override;
    QString startMountSlew(const EquatorialCoord &target,QString *error=nullptr);
    bool syncMount(const EquatorialCoord &target,QString *error=nullptr) override;
    bool mountStatus(MountStatus &status,QString *error=nullptr) const override;
    bool focuserStatus(FocuserStatus &status,QString *error=nullptr) const override;
    bool moveFocuser(int position,QString *error=nullptr) override;
    bool haltFocuser(QString *error=nullptr) override;
    bool setMountTracking(bool enabled,QString *error=nullptr) override;
    bool parkMount(bool parked,QString *error=nullptr) override;
    bool pulseGuide(GuideDirection direction,int durationMs,QString *error=nullptr) override;

    GuidingStatus startGuiding() override;
    GuidingStatus stopGuiding() override;
    GuidingStatus guideUsingLastSolve() override;
    GuidingStatus guidingStatus() const override{return guiding_.status();}

    void clearPolarSamples() override;
    bool addPolarSample(QString *error=nullptr) override;
    bool slewPolarRaOffset(double deltaDeg,QString *error=nullptr) override;
    PolarAlignmentResult estimatePolarAlignment() override;
    int polarSampleCount() const{return int(polarSamples_.size());}

    bool setSessionPlan(const QString &name,const std::vector<SessionTarget> &targets,
                        QString *error=nullptr) override;
    bool startSession(QString *error=nullptr) override;
    void stopSession() override;
    SessionStatus sessionStatus() const override{return scheduler_.status();}

    Scheduler &scheduler(){return scheduler_;}
    const CameraFrame &lastFrame() const override{return lastFrame_;}
    const SolveResult &lastSolve() const override{return lastSolve_;}

    bool startOalServer(quint16 httpPort,bool websocketEnabled,quint16 wsPort,QString *error=nullptr) override;
    void stopOalServer() override;
    bool oalRunning() const override;
    void refreshState() override;

    QJsonArray devicesJson() const;
    QJsonObject cameraStatusJson() const;
    bool frameById(const QString &frameId,CameraFrame &frame,QString *error=nullptr) const;
    QJsonObject stateJson() const;
    QJsonObject nodeInfoJson() const;

private:
    void disconnectDevices(bool clearAutoConnect);
    bool ensureResourcesAvailable(const QStringList &resources,QString *error=nullptr) const;
    void commitCapturedFrame(const CameraFrame &frame);
    static QImage toQImage(const cv::Mat &image);
    void emitState();
    std::shared_ptr<ICamera> camera_;
    std::shared_ptr<IMount> mount_;
    std::shared_ptr<IFocuser> focuser_;
    std::shared_ptr<StarCatalog> catalog_;
    std::shared_ptr<IPlateSolver> solver_;
    std::shared_ptr<IPlateSolver> catalogSolver_;
    std::shared_ptr<AstapSolver> astapSolver_;
    std::shared_ptr<INeuralSolver> neuralSolver_;
    TelescopeProfile profile_;
    CameraFrame previousFrame_;
    CameraFrame lastFrame_;
    SolveResult lastSolve_;
    AutofocusEngine autofocusEngine_;
    MotionEstimator motionEstimator_;
    StarDetector starDetector_;
    GuidingEngine guiding_;
    PolarAlignmentEstimator polarEstimator_;
    std::vector<PolarSample> polarSamples_;
    Scheduler scheduler_;
    OperationManager operations_;
    AppSettings settings_;
    std::unique_ptr<OalServer> oalServer_;
    std::unique_ptr<OalWsServer> oalWsServer_;
#ifdef OAS_HAVE_POSITIONING
    ::QGeoPositionInfoSource *positionSource_{};
#endif
};
}
