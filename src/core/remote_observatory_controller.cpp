#include "core/remote_observatory_controller.h"
#include <QJsonDocument>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace oas {
namespace {
QJsonObject profileJson(const TelescopeProfile &p){
    return {{"name",p.name},{"focalLengthMm",p.focalLengthMm},{"pixelSizeUm",p.pixelSizeUm},
            {"sensorWidthPx",p.sensorWidthPx},{"sensorHeightPx",p.sensorHeightPx},
            {"observer",QJsonObject{{"latitudeDeg",p.observer.latitudeDeg},{"longitudeDeg",p.observer.longitudeDeg},{"elevationM",p.observer.elevationM}}}};
}
TelescopeProfile profileFromJson(const QJsonObject&o){
    TelescopeProfile p;p.name=o.value("name").toString("Default");p.focalLengthMm=o.value("focalLengthMm").toDouble(400);
    p.pixelSizeUm=o.value("pixelSizeUm").toDouble(4.8);p.sensorWidthPx=o.value("sensorWidthPx").toInt(1920);p.sensorHeightPx=o.value("sensorHeightPx").toInt(1080);
    auto l=o.value("observer").toObject();p.observer={l.value("latitudeDeg").toDouble(),l.value("longitudeDeg").toDouble(),l.value("elevationM").toDouble()};return p;
}
QString guideName(GuideDirection d){switch(d){case GuideDirection::South:return "south";case GuideDirection::East:return "east";case GuideDirection::West:return "west";default:return "north";}}
}

RemoteObservatoryController::RemoteObservatoryController(QUrl base,QObject *parent):ObservatoryController(parent),base_(std::move(base)){
    QString p=base_.path();while(p.endsWith('/'))p.chop(1);if(p.endsWith("/api/v1"))p.chop(7);base_.setPath(p);
    wsReconnect_.setInterval(3000);wsReconnect_.setSingleShot(false);
    connect(&ws_,&QWebSocket::textMessageReceived,this,&RemoteObservatoryController::onWsText);
    connect(&ws_,&QWebSocket::connected,this,[this](){wsReconnect_.stop();emit logMessage("Connected to OAL event stream");refreshState();});
    connect(&ws_,&QWebSocket::disconnected,this,[this](){emit logMessage("OAL event stream disconnected; commands still use HTTP");if(wsUrl_.isValid()&&!wsReconnect_.isActive())wsReconnect_.start();});
    connect(&wsReconnect_,&QTimer::timeout,this,[this](){if(ws_.state()==QAbstractSocket::UnconnectedState&&wsUrl_.isValid())ws_.open(wsUrl_);});
}
QUrl RemoteObservatoryController::api(const QString&p)const{return appendPath(base_,"api/v1/"+p);}
bool RemoteObservatoryController::accepted(const HttpJsonClient::Reply&r,QJsonValue*d,QString*e)const{
    if(!r.ok()||!r.json.value("ok").toBool()){if(e)*e=r.error.isEmpty()?r.json.value("error").toObject().value("message").toString("OAL request failed"):r.error;return false;}if(d)*d=r.json.value("data");return true;
}
bool RemoteObservatoryController::probe(QString*e){
    QJsonValue d;auto r=http_.get(api("node/info"),3000);if(!accepted(r,&d,e))return false;
    if(!refreshMetadata(e))return false;openEventStream(d.toObject());emit logMessage("Using remote OAL core at "+base_.toString());return true;
}
void RemoteObservatoryController::refreshState(){QJsonValue d;QString e;if(accepted(http_.get(api("state"),3000),&d,&e))emit stateChanged(d.toObject());else emit logMessage("State refresh failed: "+e);}
bool RemoteObservatoryController::refreshMetadata(QString*e)const{
    QJsonValue d;auto r=http_.get(api("node/backends"),3000);if(!accepted(r,&d,e))return false;auto o=d.toObject();
    auto arr=[](const QJsonValue&v){QStringList x;for(auto i:v.toArray())x<<i.toString();return x;};cameraBackends_=arr(o.value("camera"));mountBackends_=arr(o.value("mount"));focuserBackends_=arr(o.value("focuser"));solverBackends_=arr(o.value("solver"));
    auto pr=http_.get(api("profile"),3000);if(!accepted(pr,&d,e))return false;profile_=profileFromJson(d.toObject());metadataLoaded_=true;return true;
}
void RemoteObservatoryController::openEventStream(const QJsonObject&i){if(!i.value("wsRunning").toBool())return;wsUrl_=base_;wsUrl_.setScheme(wsUrl_.scheme()=="https"?"wss":"ws");wsUrl_.setPort(i.value("wsPort").toInt(8090));wsUrl_.setPath("/");ws_.open(wsUrl_);}
TelescopeProfile RemoteObservatoryController::profile()const{if(!metadataLoaded_)refreshMetadata(nullptr);return profile_;}
void RemoteObservatoryController::setProfile(const TelescopeProfile&p){QJsonValue d;QString e;if(accepted(http_.postJson(api("profile"),profileJson(p)),&d,&e)){profile_=p;emit profileChanged();emit logMessage("Profile saved on OAL node");}else emit logMessage("Profile update failed: "+e);}
QStringList RemoteObservatoryController::cameraBackends()const{if(!metadataLoaded_)refreshMetadata(nullptr);return cameraBackends_;}
QStringList RemoteObservatoryController::mountBackends()const{if(!metadataLoaded_)refreshMetadata(nullptr);return mountBackends_;}
QStringList RemoteObservatoryController::focuserBackends()const{if(!metadataLoaded_)refreshMetadata(nullptr);return focuserBackends_;}
QStringList RemoteObservatoryController::solverBackends()const{if(!metadataLoaded_)refreshMetadata(nullptr);return solverBackends_;}
bool RemoteObservatoryController::selectSolver(const QString&n,QString*e){return accepted(http_.postJson(api("solver/select"),{{"name",n}}),nullptr,e);}
bool RemoteObservatoryController::loadCatalog(const QString&p,QString*e){return accepted(http_.postJson(api("solver/catalog"),{{"path",p}}),nullptr,e);}
bool RemoteObservatoryController::loadNeuralModel(const QString&p,QString*e){return accepted(http_.postJson(api("solver/model"),{{"path",p}}),nullptr,e);}
static bool connectRemoteDevice(HttpJsonClient &h,const QUrl&u,const QString&t,const QString&b,const QString&ep,QString*e){auto r=h.postJson(u,{{"type",t},{"backend",b},{"endpoint",ep}},30000);if(!r.ok()||!r.json.value("ok").toBool()){if(e)*e=r.error.isEmpty()?r.json.value("error").toObject().value("message").toString("Device connect failed"):r.error;return false;}return true;}
bool RemoteObservatoryController::connectCamera(const QString&b,const QString&ep,QString*e){bool ok=connectRemoteDevice(http_,api("devices/connect"),"camera",b,ep,e);if(ok){emit logMessage("Camera configured on node: "+b);refreshState();}return ok;}
bool RemoteObservatoryController::connectMount(const QString&b,const QString&ep,QString*e){bool ok=connectRemoteDevice(http_,api("devices/connect"),"mount",b,ep,e);if(ok){emit logMessage("Mount configured on node: "+b);refreshState();}return ok;}
bool RemoteObservatoryController::connectFocuser(const QString&b,const QString&ep,QString*e){bool ok=connectRemoteDevice(http_,api("devices/connect"),"focuser",b,ep,e);if(ok){emit logMessage("Focuser configured on node: "+b);refreshState();}return ok;}
void RemoteObservatoryController::disconnectAll(){QString e;if(!accepted(http_.postJson(api("devices/disconnect-all"),{}),nullptr,&e))emit logMessage(e);else refreshState();}
bool RemoteObservatoryController::capture(const ExposureRequest&r,CameraFrame*out,QString*e){
    QJsonObject q{{"exposureSec",r.exposureSec},{"binX",r.binX},{"binY",r.binY},{"gain",r.gain},{"offset",r.offset},{"includeImage",true}};if(r.roi.width>0&&r.roi.height>0)q["roi"]=QJsonObject{{"x",r.roi.x},{"y",r.roi.y},{"width",r.roi.width},{"height",r.roi.height}};
    QJsonValue d;int timeout=std::max(120000,int(r.exposureSec*1000.0)+60000);if(!accepted(http_.postJson(api("cameras/main/capture"),q,timeout),&d,e))return false;auto o=d.toObject();CameraFrame f;f.id=o.value("frameId").toString();f.capturedUtc=QDateTime::fromString(o.value("capturedUtc").toString(),Qt::ISODateWithMs);f.exposureSec=r.exposureSec;f.gain=r.gain;f.source="remote-node";
    auto png=QByteArray::fromBase64(o.value("imagePngBase64").toString().toLatin1());std::vector<uchar> bytes(png.begin(),png.end());f.image=cv::imdecode(bytes,cv::IMREAD_UNCHANGED);if(f.image.empty()){if(e)*e="Node returned no preview pixels";return false;}previousFrame_=lastFrame_;lastFrame_=f;if(out)*out=f;emit frameCaptured(toQImage(f.image),f.id);emit logMessage("Remote capture completed: "+f.id);return true;
}
SolveResult RemoteObservatoryController::parseSolve(const QJsonObject&o){SolveResult s;s.success=o.value("success").toBool();s.raDeg=o.value("raDeg").toDouble();s.decDeg=o.value("decDeg").toDouble();s.rotationDeg=o.value("rotationDeg").toDouble();s.scaleArcsecPerPx=o.value("scaleArcsecPerPx").toDouble();s.matchedStars=o.value("matchedStars").toInt();s.rmsArcsec=o.value("rmsArcsec").toDouble();s.catalog=o.value("catalog").toString();s.message=o.value("message").toString();for(auto v:o.value("imageStars").toArray()){auto x=v.toObject();DetectedStar st;st.positionPx={x.value("x").toDouble(),x.value("y").toDouble()};st.flux=x.value("flux").toDouble();st.peak=x.value("peak").toDouble();st.hfrPx=x.value("hfrPx").toDouble();s.imageStars.push_back(st);}return s;}
SolveResult RemoteObservatoryController::solveLast(const SolveHint&h){QJsonObject q{{"searchRadiusDeg",h.searchRadiusDeg}};if(h.raDeg)q["hintRaDeg"]=*h.raDeg;if(h.decDeg)q["hintDecDeg"]=*h.decDeg;if(h.fovDeg)q["hintFovDeg"]=*h.fovDeg;QJsonValue d;QString e;if(accepted(http_.postJson(api("solve"),q,120000),&d,&e))lastSolve_=parseSolve(d.toObject());else{lastSolve_={};lastSolve_.message=e;}emit solveCompleted(solveToJson(lastSolve_));emit logMessage(lastSolve_.message);return lastSolve_;}
AutofocusResult RemoteObservatoryController::parseAutofocus(const QJsonObject&o){AutofocusResult x;x.success=o.value("success").toBool();x.bestPosition=o.value("bestPosition").toInt();x.bestScore=o.value("bestScore").toDouble();x.message=o.value("message").toString();for(auto v:o.value("samples").toArray()){auto s=v.toObject();x.samples.push_back({s.value("position").toInt(),s.value("score").toDouble(),s.value("spread").toDouble()});}return x;}
AutofocusResult RemoteObservatoryController::autofocus(const AutofocusRequest&r){QString m=r.mode==AutofocusMode::Planet?"planet":r.mode==AutofocusMode::Bahtinov?"bahtinov":"stars";QJsonObject q{{"mode",m},{"rangeSteps",r.rangeSteps},{"coarseStep",r.coarseStep},{"fineStep",r.fineStep},{"framesPerPosition",r.framesPerPosition},{"settleMs",r.settleMs},{"autoPlanetRoi",r.autoPlanetRoi}};QJsonValue d;QString e;AutofocusResult x;if(accepted(http_.postJson(api("autofocus/default/run"),q,300000),&d,&e))x=parseAutofocus(d.toObject());else x.message=e;emit autofocusCompleted(autofocusToJson(x));emit logMessage(x.message);return x;}
FrameMotion RemoteObservatoryController::estimateLastMotion(){QJsonValue d;QString e;FrameMotion m;if(accepted(http_.postJson(api("motion/estimate"),{}),&d,&e)){auto o=d.toObject();m={o.value("valid").toBool(),o.value("dxPx").toDouble(),o.value("dyPx").toDouble(),o.value("rotationDeg").toDouble(),o.value("scale").toDouble(1),o.value("inliers").toInt()};}else emit logMessage(e);emit motionEstimated(QJsonObject{{"valid",m.valid},{"dxPx",m.dxPx},{"dyPx",m.dyPx},{"rotationDeg",m.rotationDeg},{"scale",m.scale},{"inliers",m.inliers}});return m;}
void RemoteObservatoryController::requestSystemLocation(){QString e;if(!accepted(http_.postJson(api("observer/system-location"),{}),nullptr,&e))emit logMessage(e);}
bool RemoteObservatoryController::slewMount(const EquatorialCoord&t,QString*e){return accepted(http_.postJson(api("mounts/main/slew"),coordToJson(t),120000),nullptr,e);}
bool RemoteObservatoryController::syncMount(const EquatorialCoord&t,QString*e){return accepted(http_.postJson(api("mounts/main/sync"),coordToJson(t)),nullptr,e);}
bool RemoteObservatoryController::mountStatus(MountStatus&s,QString*e)const{QJsonValue d;if(!accepted(http_.get(api("mounts/main/status")),&d,e))return false;auto o=d.toObject();s.connection=ConnectionState::Connected;s.coordinate={o.value("raDeg").toDouble(),o.value("decDeg").toDouble()};s.tracking=o.value("tracking").toBool();s.slewing=o.value("slewing").toBool();s.parked=o.value("parked").toBool();s.pierSide=o.value("pierSide").toString("unknown");return true;}
bool RemoteObservatoryController::focuserStatus(FocuserStatus&s,QString*e)const{QJsonValue d;if(!accepted(http_.get(api("focusers/main/status")),&d,e))return false;auto o=d.toObject();s.connection=ConnectionState::Connected;s.position=o.value("position").toInt();s.moving=o.value("moving").toBool();if(o.contains("temperatureC"))s.temperatureC=o.value("temperatureC").toDouble();return true;}
bool RemoteObservatoryController::moveFocuser(int p,QString*e){bool ok=accepted(http_.postJson(api("focusers/main/move"),{{"position",p}},120000),nullptr,e);if(ok)refreshState();return ok;}
bool RemoteObservatoryController::haltFocuser(QString*e){bool ok=accepted(http_.postJson(api("focusers/main/halt"),{}),nullptr,e);if(ok)refreshState();return ok;}
bool RemoteObservatoryController::setMountTracking(bool v,QString*e){return accepted(http_.postJson(api("mounts/main/tracking"),{{"enabled",v}}),nullptr,e);}
bool RemoteObservatoryController::parkMount(bool v,QString*e){return accepted(http_.postJson(api("mounts/main/park"),{{"parked",v}},120000),nullptr,e);}
bool RemoteObservatoryController::pulseGuide(GuideDirection d,int ms,QString*e){return accepted(http_.postJson(api("mounts/main/pulse-guide"),{{"direction",guideName(d)},{"durationMs",ms}}),nullptr,e);}
GuidingStatus RemoteObservatoryController::parseGuiding(const QJsonObject&o){GuidingStatus g;g.active=o.value("active").toBool();g.raErrorArcsec=o.value("raErrorArcsec").toDouble();g.decErrorArcsec=o.value("decErrorArcsec").toDouble();g.rmsArcsec=o.value("rmsArcsec").toDouble();return g;}
GuidingStatus RemoteObservatoryController::startGuiding(){QJsonValue d;QString e;if(accepted(http_.postJson(api("guider/default/start"),{}),&d,&e))guiding_=parseGuiding(d.toObject());else emit logMessage(e);return guiding_;}
GuidingStatus RemoteObservatoryController::stopGuiding(){QJsonValue d;QString e;if(accepted(http_.postJson(api("guider/default/stop"),{}),&d,&e))guiding_=parseGuiding(d.toObject());else emit logMessage(e);return guiding_;}
GuidingStatus RemoteObservatoryController::guideUsingLastSolve(){QJsonValue d;QString e;if(accepted(http_.postJson(api("guider/default/update"),{}),&d,&e))guiding_=parseGuiding(d.toObject());else emit logMessage(e);return guiding_;}
GuidingStatus RemoteObservatoryController::guidingStatus()const{QJsonValue d;QString e;if(accepted(http_.get(api("guider/default/status")),&d,&e))return parseGuiding(d.toObject());return guiding_;}
void RemoteObservatoryController::clearPolarSamples(){QJsonValue d;QString e;if(accepted(http_.postJson(api("polar-align/clear"),{}),&d,&e))emit polarSampleCountChanged(d.toObject().value("count").toInt());else emit logMessage(e);}
bool RemoteObservatoryController::addPolarSample(QString*e){QJsonValue d;if(!accepted(http_.postJson(api("polar-align/sample"),{}),&d,e))return false;emit polarSampleCountChanged(d.toObject().value("count").toInt());return true;}
bool RemoteObservatoryController::slewPolarRaOffset(double d,QString*e){return accepted(http_.postJson(api("polar-align/ra-offset"),{{"deltaDeg",d}},120000),nullptr,e);}
PolarAlignmentResult RemoteObservatoryController::parsePolar(const QJsonObject&o){PolarAlignmentResult p;p.success=o.value("success").toBool();p.axisRaDeg=o.value("axisRaDeg").toDouble();p.axisDecDeg=o.value("axisDecDeg").toDouble(90);p.totalErrorArcmin=o.value("totalErrorArcmin").toDouble();p.altitudeAdjustmentArcmin=o.value("altitudeAdjustmentArcmin").toDouble();p.azimuthAdjustmentArcmin=o.value("azimuthAdjustmentArcmin").toDouble();p.message=o.value("message").toString();return p;}
PolarAlignmentResult RemoteObservatoryController::estimatePolarAlignment(){QJsonValue d;QString e;if(accepted(http_.get(api("polar-align/estimate")),&d,&e))return parsePolar(d.toObject());PolarAlignmentResult p;p.message=e;return p;}
bool RemoteObservatoryController::setSessionPlan(const QString&n,const std::vector<SessionTarget>&t,QString*e){if(t.empty()){if(e)*e="No targets supplied";return false;}pendingSessionName_=n;pendingTargets_=t;return true;}
SessionStatus RemoteObservatoryController::parseSession(const QJsonObject&o){SessionStatus s;s.id=o.value("id").toString();s.name=o.value("name").toString();s.active=o.value("active").toBool();s.targetIndex=o.value("targetIndex").toInt();s.targetCount=o.value("targetCount").toInt();s.completedFrames=o.value("completedFrames").toInt();s.state=o.value("state").toString("idle");return s;}
bool RemoteObservatoryController::startSession(QString*e){if(pendingTargets_.empty()){if(e)*e="No targets supplied";return false;}QJsonArray a;for(auto&t:pendingTargets_)a.append(QJsonObject{{"name",t.name},{"raDeg",t.coordinate.raDeg},{"decDeg",t.coordinate.decDeg},{"exposureSec",t.exposureSec},{"repeats",t.repeats},{"filter",t.filter}});QJsonValue d;if(!accepted(http_.postJson(api("sessions"),{{"name",pendingSessionName_},{"targets",a}}),&d,e))return false;session_=parseSession(d.toObject());emit sessionChanged(d.toObject());return true;}
void RemoteObservatoryController::stopSession(){QJsonValue d;QString e;if(accepted(http_.postJson(api("sessions/current/stop"),{}),&d,&e)){session_=parseSession(d.toObject());emit sessionChanged(d.toObject());}else emit logMessage(e);}
SessionStatus RemoteObservatoryController::sessionStatus()const{QJsonValue d;QString e;if(accepted(http_.get(api("sessions/current")),&d,&e))return parseSession(d.toObject());return session_;}
bool RemoteObservatoryController::startOalServer(quint16,bool,quint16,QString*e){if(e)*e="Remote mode: server lifecycle is owned by openastrolink-node/systemd";return false;}
void RemoteObservatoryController::stopOalServer(){emit logMessage("Remote mode: use systemd on the node to stop openastrolink-node");}
void RemoteObservatoryController::onWsText(const QString&m){auto doc=QJsonDocument::fromJson(m.toUtf8());if(!doc.isObject())return;auto root=doc.object();auto type=root.value("type").toString();auto p=root.value("payload").toObject();if(type=="state")emit stateChanged(p);else if(type=="solveResult"){lastSolve_=parseSolve(p);emit solveCompleted(p);}else if(type=="autofocusProgress")emit autofocusProgress(p);else if(type=="autofocusResult")emit autofocusCompleted(p);else if(type=="guidingUpdate"){guiding_=parseGuiding(p);emit guidingChanged(p);}else if(type=="sessionUpdate"){session_=parseSession(p);emit sessionChanged(p);}else if(type=="polarSampleCount")emit polarSampleCountChanged(p.value("count").toInt());else if(type=="motion")emit motionEstimated(p);}
QImage RemoteObservatoryController::toQImage(const cv::Mat&i){if(i.empty())return{};cv::Mat rgb;if(i.channels()==1){cv::Mat u8;if(i.depth()==CV_16U){double mn,mx;cv::minMaxLoc(i,&mn,&mx);i.convertTo(u8,CV_8U,255.0/std::max(1.0,mx-mn),-mn*255.0/std::max(1.0,mx-mn));}else i.convertTo(u8,CV_8U);cv::cvtColor(u8,rgb,cv::COLOR_GRAY2RGB);}else cv::cvtColor(i,rgb,cv::COLOR_BGR2RGB);return QImage(rgb.data,rgb.cols,rgb.rows,int(rgb.step),QImage::Format_RGB888).copy();}
} // namespace oas
