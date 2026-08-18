#pragma once
#include "algorithms/autofocus_engine.h"
#include "algorithms/guiding_engine.h"
#include "algorithms/motion_estimator.h"
#include "algorithms/star_detector.h"
#include "algorithms/polar_alignment.h"
#include "algorithms/scheduler.h"
#include "core/interfaces.h"
#include "core/settings.h"
#include <QImage>
#include <QObject>
#include <memory>

#ifdef OAS_HAVE_POSITIONING
class QGeoPositionInfoSource;
#endif

namespace oas {
class OalServer;
class OalWsServer;
class StarCatalog;

class ApplicationController final : public QObject {
    Q_OBJECT
public:
    explicit ApplicationController(QObject *parent=nullptr);
    ~ApplicationController() override;

    TelescopeProfile profile() const{return profile_;}
    void setProfile(const TelescopeProfile &profile);
    QStringList cameraBackends() const;
    QStringList mountBackends() const;
    QStringList focuserBackends() const;
    QStringList solverBackends() const{return {"catalog-pattern","neural"};}
    bool selectSolver(const QString &name,QString *error=nullptr);
    bool loadCatalog(const QString &path,QString *error=nullptr);
    bool loadNeuralModel(const QString &path,QString *error=nullptr);

    bool connectCamera(const QString &backend,const QString &endpoint,QString *error=nullptr);
    bool connectMount(const QString &backend,const QString &endpoint,QString *error=nullptr);
    bool connectFocuser(const QString &backend,const QString &endpoint,QString *error=nullptr);
    void disconnectAll();

    bool capture(const ExposureRequest &request,CameraFrame *out=nullptr,QString *error=nullptr);
    SolveResult solveLast(const SolveHint &hint={});
    AutofocusResult autofocus(const AutofocusRequest &request);
    FrameMotion estimateLastMotion();
    void requestSystemLocation();

    bool slewMount(const EquatorialCoord &target,QString *error=nullptr);
    bool syncMount(const EquatorialCoord &target,QString *error=nullptr);
    bool mountStatus(MountStatus &status,QString *error=nullptr) const;
    bool focuserStatus(FocuserStatus &status,QString *error=nullptr) const;
    bool moveFocuser(int position,QString *error=nullptr);
    bool haltFocuser(QString *error=nullptr);
    bool setMountTracking(bool enabled,QString *error=nullptr);
    bool parkMount(bool parked,QString *error=nullptr);
    bool pulseGuide(GuideDirection direction,int durationMs,QString *error=nullptr);

    GuidingStatus startGuiding();
    GuidingStatus stopGuiding();
    GuidingStatus guideUsingLastSolve();
    GuidingStatus guidingStatus() const{return guiding_.status();}

    void clearPolarSamples();
    bool addPolarSample(QString *error=nullptr);
    bool slewPolarRaOffset(double deltaDeg,QString *error=nullptr);
    PolarAlignmentResult estimatePolarAlignment() const;

    Scheduler &scheduler(){return scheduler_;}
    const CameraFrame &lastFrame() const{return lastFrame_;}
    const SolveResult &lastSolve() const{return lastSolve_;}

    bool startOalServer(quint16 httpPort,bool websocketEnabled,quint16 wsPort,QString *error=nullptr);
    void stopOalServer();
    bool oalRunning() const;

    QJsonArray devicesJson() const;
    QJsonObject cameraStatusJson() const;
    QJsonObject stateJson() const;

signals:
    void logMessage(const QString &message);
    void frameCaptured(const QImage &image,const QString &frameId);
    void solveCompleted(const QJsonObject &result);
    void autofocusProgress(const QJsonObject &sample);
    void autofocusCompleted(const QJsonObject &result);
    void guidingChanged(const QJsonObject &status);
    void polarSampleCountChanged(int count);
    void sessionChanged(const QJsonObject &status);
    void stateChanged(const QJsonObject &state);
    void motionEstimated(const QJsonObject &motion);
    void profileChanged();

private:
    static QImage toQImage(const cv::Mat &image);
    void emitState();
    std::shared_ptr<ICamera> camera_;
    std::shared_ptr<IMount> mount_;
    std::shared_ptr<IFocuser> focuser_;
    std::shared_ptr<StarCatalog> catalog_;
    std::shared_ptr<IPlateSolver> solver_;
    std::shared_ptr<IPlateSolver> catalogSolver_;
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
    AppSettings settings_;
    std::unique_ptr<OalServer> oalServer_;
    std::unique_ptr<OalWsServer> oalWsServer_;
#ifdef OAS_HAVE_POSITIONING
    ::QGeoPositionInfoSource *positionSource_{};
#endif
};
}
