#pragma once

#include "backends/http_json_client.h"
#include "core/observatory_controller.h"
#include <QUrl>
#include <QWebSocket>
#include <QTimer>

namespace oas {

class RemoteObservatoryController final : public ObservatoryController {
    Q_OBJECT
public:
    explicit RemoteObservatoryController(QUrl nodeBase, QObject *parent = nullptr);
    ~RemoteObservatoryController() override = default;

    bool probe(QString *error = nullptr);

    QString controlMode() const override { return "remote-node"; }
    QString endpointDescription() const override { return base_.toString(); }
    bool isRemote() const override { return true; }

    TelescopeProfile profile() const override;
    void setProfile(const TelescopeProfile &profile) override;
    QStringList cameraBackends() const override;
    QStringList mountBackends() const override;
    QStringList focuserBackends() const override;
    QStringList solverBackends() const override;
    bool selectSolver(const QString &name, QString *error = nullptr) override;
    bool loadCatalog(const QString &path, QString *error = nullptr) override;
    bool loadNeuralModel(const QString &path, QString *error = nullptr) override;

    bool connectCamera(const QString &backend,const QString &endpoint,QString *error=nullptr) override;
    bool connectGuideCamera(const QString &backend,const QString &endpoint,QString *error=nullptr) override;
    bool connectMount(const QString &backend,const QString &endpoint,QString *error=nullptr) override;
    bool connectFocuser(const QString &backend,const QString &endpoint,QString *error=nullptr) override;
    bool disconnectCamera(QString *error=nullptr) override;
    bool disconnectGuideCamera(QString *error=nullptr) override;
    bool disconnectMount(QString *error=nullptr) override;
    bool disconnectFocuser(QString *error=nullptr) override;
    bool disconnectAll(QString *error=nullptr) override;

    bool capture(const ExposureRequest &request,CameraFrame *out=nullptr,QString *error=nullptr) override;
    QString startCapture(const ExposureRequest &request,QString *error=nullptr) override;
    QString startGuideCapture(const ExposureRequest &request,QString *error=nullptr) override;
    SolveResult solveLast(const SolveHint &hint={}) override;
    QString startAdaptiveSolve(const AdaptiveSolveRequest &request,QString *error=nullptr) override;
    AutofocusResult autofocus(const AutofocusRequest &request) override;
    QString startAutofocus(const AutofocusRequest &request,QString *error=nullptr) override;
    bool cancelOperation(const QString &operationId,QString *error=nullptr) override;
    QJsonObject operation(const QString &operationId,QString *error=nullptr) const override;
    QJsonArray operations(bool activeOnly=false) const override;
    FrameMotion estimateLastMotion() override;
    void requestSystemLocation() override;

    bool slewMount(const EquatorialCoord &target,QString *error=nullptr) override;
    bool abortMountMotion(QString *error=nullptr) override;
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
    GuidingStatus guidingStatus() const override;

    void clearPolarSamples() override;
    bool addPolarSample(QString *error=nullptr) override;
    bool slewPolarRaOffset(double deltaDeg,QString *error=nullptr) override;
    PolarAlignmentResult estimatePolarAlignment() override;

    bool setSessionPlan(const QString &name,const std::vector<SessionTarget> &targets,
                        QString *error=nullptr) override;
    bool startSession(QString *error=nullptr) override;
    void stopSession() override;
    SessionStatus sessionStatus() const override;

    const CameraFrame &lastFrame() const override{return lastFrame_;}
    const CameraFrame &lastGuideFrame() const override{return lastGuideFrame_;}
    const SolveResult &lastSolve() const override{return lastSolve_;}

    bool startOalServer(quint16,bool,quint16,QString *error=nullptr) override;
    void stopOalServer() override;
    bool oalRunning() const override{return true;}
    bool startStellariumServer(quint16 port,QString *error=nullptr) override;
    void stopStellariumServer() override;
    bool stellariumRunning() const override{return stellariumRunning_;}
    quint16 stellariumPort() const override{return stellariumPort_;}
    bool refreshNativeDiscovery(QString *error=nullptr) override;
    QJsonArray availableSerialPorts() const override;
    QString nativeSerialPortOverride(const QString &driverId) const override;
    bool setNativeSerialPortOverride(const QString &driverId,const QString &port,QString *error=nullptr) override;
    void refreshState() override;

private slots:
    void onWsText(const QString &message);

private:
    QUrl api(const QString &path) const;
    bool accepted(const HttpJsonClient::Reply &reply,QJsonValue *data=nullptr,QString *error=nullptr) const;
    bool refreshMetadata(QString *error=nullptr) const;
    void openEventStream(const QJsonObject &nodeInfo);
    static SolveResult parseSolve(const QJsonObject &o);
    static AutofocusResult parseAutofocus(const QJsonObject &o);
    static GuidingStatus parseGuiding(const QJsonObject &o);
    static SessionStatus parseSession(const QJsonObject &o);
    static PolarAlignmentResult parsePolar(const QJsonObject &o);
    static QImage toQImage(const cv::Mat &image);
    bool fetchFramePreview(const QString &frameId,CameraFrame *out=nullptr,QString *error=nullptr);

    QUrl base_;
    mutable HttpJsonClient http_;
    mutable TelescopeProfile profile_;
    mutable QStringList cameraBackends_;
    mutable QStringList mountBackends_;
    mutable QStringList focuserBackends_;
    mutable QStringList solverBackends_;
    mutable bool metadataLoaded_{false};
    QWebSocket ws_;
    QTimer wsReconnect_;
    QUrl wsUrl_;
    CameraFrame previousFrame_;
    CameraFrame lastFrame_;
    CameraFrame lastGuideFrame_;
    SolveResult lastSolve_;
    GuidingStatus guiding_;
    SessionStatus session_;
    QString pendingSessionName_;
    std::vector<SessionTarget> pendingTargets_;
    bool stellariumRunning_{false};
    quint16 stellariumPort_{10000};
};

} // namespace oas
