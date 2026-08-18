#include "core/application_controller.h"
#include "algorithms/pattern_plate_solver.h"
#include "algorithms/neural_solver.h"
#include "algorithms/star_catalog.h"
#include "backends/alpaca_devices.h"
#include "backends/gemini_eaf_focuser.h"
#include "backends/oal_client_devices.h"
#include "backends/opencv_camera.h"
#include "backends/serial_lx200_mount.h"
#include "backends/simulated_devices.h"
#include "oal/oal_server.h"
#include "oal/oal_ws_server.h"
#ifdef OAS_HAVE_QHY
#include "backends/qhy_camera.h"
#endif
#ifdef OAS_HAVE_GPHOTO2
#include "backends/canon_camera.h"
#endif
#ifdef OAS_HAVE_INDI
#include "backends/indi_devices.h"
#endif
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>
#include <QSysInfo>
#include <opencv2/imgproc.hpp>
#ifdef OAS_HAVE_POSITIONING
#include <QGeoPositionInfoSource>
#include <QGeoCoordinate>
#endif
#include <algorithm>
#include <cmath>

namespace oas {
static QJsonObject sessionJson(const SessionStatus&s){return{{"id",s.id},{"name",s.name},{"active",s.active},{"targetIndex",s.targetIndex},{"targetCount",s.targetCount},{"completedFrames",s.completedFrames},{"state",s.state}};}
ApplicationController::ApplicationController(QObject *parent):ObservatoryController(parent),scheduler_(this){
    profile_=settings_.loadProfile();catalog_=std::make_shared<StarCatalog>();QString err;QString catalogPath=QDir(QCoreApplication::applicationDirPath()).filePath("config/stars_example.csv");if(!catalog_->loadCsv(catalogPath,&err)){catalogPath=QDir::current().filePath("config/stars_example.csv");catalog_->loadCsv(catalogPath,&err);}catalogSolver_=std::make_shared<PatternPlateSolver>(catalog_);neuralSolver_=std::make_shared<NeuralSolver>();solver_=catalogSolver_;
    #ifdef OAS_HAVE_POSITIONING
    if(profile_.observer.latitudeDeg==0.0&&profile_.observer.longitudeDeg==0.0)QTimer::singleShot(0,this,&ApplicationController::requestSystemLocation);
#endif
    connect(&scheduler_,&Scheduler::statusChanged,this,[this](const SessionStatus&s){auto j=sessionJson(s);emit sessionChanged(j);if(oalWsServer_)oalWsServer_->broadcast("sessionUpdate",j);emitState();});
}
ApplicationController::~ApplicationController(){stopOalServer();disconnectAll();}
void ApplicationController::setProfile(const TelescopeProfile&p){profile_=p;settings_.saveProfile(p);emit profileChanged();emitState();}
QStringList ApplicationController::cameraBackends()const{QStringList x{"simulated","opencv","oal"};
#ifdef OAS_HAVE_QHY
x<<"qhy";
#endif
#ifdef OAS_HAVE_GPHOTO2
x<<"canon-gphoto2";
#endif
return x;}
QStringList ApplicationController::mountBackends()const{QStringList x{"simulated","serial-lx200","ascom-alpaca","oal"};
#ifdef OAS_HAVE_INDI
x<<"indi";
#endif
return x;}
QStringList ApplicationController::focuserBackends()const{QStringList x{"simulated","gemini-eaf","ascom-alpaca","oal"};
#ifdef OAS_HAVE_INDI
x<<"indi";
#endif
return x;}
bool ApplicationController::selectSolver(const QString&name,QString*e){if(name=="catalog-pattern")solver_=catalogSolver_;else if(name=="neural")solver_=neuralSolver_;else{if(e)*e="Unknown solver backend";return false;}emit logMessage("Selected solver: "+solver_->name());return true;}
bool ApplicationController::loadCatalog(const QString&p,QString*e){bool ok=catalog_->loadCsv(p,e);if(ok)emit logMessage(QString("Loaded %1 catalog stars").arg(catalog_->stars().size()));return ok;}
bool ApplicationController::loadNeuralModel(const QString&p,QString*e){bool ok=neuralSolver_->loadModel(p,e);if(ok)emit logMessage("Neural model loaded: "+p);return ok;}
bool ApplicationController::connectCamera(const QString&b,const QString&e,QString*err){std::shared_ptr<ICamera>d;if(b=="simulated")d=std::make_shared<SimulatedCamera>();else if(b=="opencv")d=std::make_shared<OpenCvCamera>(e.toInt());else if(b=="oal")d=std::make_shared<OalCameraClient>(QUrl(e));
#ifdef OAS_HAVE_QHY
else if(b=="qhy")d=std::make_shared<QhyCamera>(e.toInt());
#endif
#ifdef OAS_HAVE_GPHOTO2
else if(b=="canon-gphoto2")d=std::make_shared<CanonGPhotoCamera>();
#endif
else{if(err)*err="Unknown camera backend";return false;}if(!d->connectDevice(err))return false;camera_=std::move(d);settings_.saveCameraBinding({b,e,true});emit logMessage("Camera connected: "+camera_->displayName());emitState();return true;}
bool ApplicationController::connectMount(const QString&b,const QString&e,QString*err){std::shared_ptr<IMount>d;if(b=="simulated")d=std::make_shared<SimulatedMount>();else if(b=="serial-lx200")d=std::make_shared<SerialLx200Mount>(e);else if(b=="ascom-alpaca")d=std::make_shared<AlpacaMount>(QUrl(e));else if(b=="oal")d=std::make_shared<OalMountClient>(QUrl(e));
#ifdef OAS_HAVE_INDI
else if(b=="indi")d=std::make_shared<IndiMount>(e);
#endif
else{if(err)*err="Unknown mount backend";return false;}if(!d->connectDevice(err))return false;mount_=std::move(d);settings_.saveMountBinding({b,e,true});emit logMessage("Mount connected: "+mount_->displayName());emitState();return true;}
bool ApplicationController::connectFocuser(const QString&b,const QString&e,QString*err){std::shared_ptr<IFocuser>d;if(b=="simulated")d=std::make_shared<SimulatedFocuser>();else if(b=="gemini-eaf")d=std::make_shared<GeminiEafFocuser>(e);else if(b=="ascom-alpaca")d=std::make_shared<AlpacaFocuser>(QUrl(e));else if(b=="oal")d=std::make_shared<OalFocuserClient>(QUrl(e));
#ifdef OAS_HAVE_INDI
else if(b=="indi")d=std::make_shared<IndiFocuser>(e);
#endif
else{if(err)*err="Unknown focuser backend";return false;}if(!d->connectDevice(err))return false;focuser_=std::move(d);settings_.saveFocuserBinding({b,e,true});emit logMessage("Focuser connected: "+focuser_->displayName());emitState();return true;}
void ApplicationController::disconnectAll(){if(camera_)camera_->disconnectDevice();if(mount_)mount_->disconnectDevice();if(focuser_)focuser_->disconnectDevice();camera_.reset();mount_.reset();focuser_.reset();emitState();}
bool ApplicationController::restoreConfiguredDevices(QStringList *errors){
    bool allOk=true;
    auto restore=[&](const QString &kind,const DeviceBinding &b,auto fn){
        if(!b.autoConnect||b.backend.isEmpty())return;
        QString e;if(!(this->*fn)(b.backend,b.endpoint,&e)){allOk=false;if(errors)errors->append(kind+": "+e);emit logMessage("Auto-connect "+kind+" failed: "+e);}
    };
    if(!camera_)restore("camera",settings_.cameraBinding(),&ApplicationController::connectCamera);
    if(!mount_)restore("mount",settings_.mountBinding(),&ApplicationController::connectMount);
    if(!focuser_)restore("focuser",settings_.focuserBinding(),&ApplicationController::connectFocuser);
    return allOk;
}
bool ApplicationController::capture(const ExposureRequest&r,CameraFrame*out,QString*err){if(!camera_){if(err)*err="No camera connected";return false;}CameraFrame f;if(!camera_->capture(r,f,err))return false;previousFrame_=lastFrame_;lastFrame_=f;if(out)*out=f;emit frameCaptured(toQImage(f.image),f.id);emit logMessage(QString("Captured %1 × %2 frame %3").arg(f.image.cols).arg(f.image.rows).arg(f.id));emitState();return true;}
SolveResult ApplicationController::solveLast(const SolveHint&h){if(lastFrame_.image.empty()){lastSolve_.message="No captured frame";lastSolve_.success=false;}else lastSolve_=solver_->solve(lastFrame_,profile_,h);auto j=solveToJson(lastSolve_);QJsonArray stars;for(const auto&s:lastSolve_.imageStars)stars.append(QJsonObject{{"x",s.positionPx.x},{"y",s.positionPx.y},{"flux",s.flux},{"peak",s.peak},{"hfrPx",s.hfrPx}});j["imageStars"]=stars;emit solveCompleted(j);if(oalWsServer_)oalWsServer_->broadcast("solveResult",j);emit logMessage(lastSolve_.message);emitState();return lastSolve_;}
AutofocusResult ApplicationController::autofocus(const AutofocusRequest&r){AutofocusResult x;if(!camera_||!focuser_){x.message="Camera and focuser must be connected";}else x=autofocusEngine_.run(*camera_,*focuser_,r,[this](const FocusSample&s){QJsonObject j{{"position",s.position},{"score",s.score},{"spread",s.spread}};emit autofocusProgress(j);if(oalWsServer_)oalWsServer_->broadcast("autofocusProgress",j);});auto j=autofocusToJson(x);emit autofocusCompleted(j);if(oalWsServer_)oalWsServer_->broadcast("autofocusResult",j);emit logMessage(x.message);emitState();return x;}
FrameMotion ApplicationController::estimateLastMotion(){FrameMotion m;if(previousFrame_.image.empty()||lastFrame_.image.empty()){emit logMessage("Two captured frames are required");return m;}auto a=starDetector_.detect(previousFrame_.image);auto b=starDetector_.detect(lastFrame_.image);m=motionEstimator_.estimate(a,b);QJsonObject j{{"valid",m.valid},{"dxPx",m.dxPx},{"dyPx",m.dyPx},{"rotationDeg",m.rotationDeg},{"scale",m.scale},{"inliers",m.inliers}};emit motionEstimated(j);if(oalWsServer_)oalWsServer_->broadcast("motion",j);return m;}
void ApplicationController::requestSystemLocation(){
#ifdef OAS_HAVE_POSITIONING
    if(!positionSource_){positionSource_=QGeoPositionInfoSource::createDefaultSource(this);if(!positionSource_){emit logMessage("No system positioning source is available");return;}connect(positionSource_,&QGeoPositionInfoSource::positionUpdated,this,[this](const QGeoPositionInfo&i){auto c=i.coordinate();profile_.observer.latitudeDeg=c.latitude();profile_.observer.longitudeDeg=c.longitude();if(c.type()==QGeoCoordinate::Coordinate3D)profile_.observer.elevationM=c.altitude();settings_.saveProfile(profile_);emit profileChanged();emit logMessage(QString("System location: %1, %2").arg(c.latitude(),0,'f',6).arg(c.longitude(),0,'f',6));emitState();});connect(positionSource_,&QGeoPositionInfoSource::errorOccurred,this,[this](QGeoPositionInfoSource::Error){emit logMessage("System location request failed");});}positionSource_->requestUpdate(10000);
#else
    emit logMessage("Qt Positioning was not available at build time; enter location manually");
#endif
}
bool ApplicationController::slewMount(const EquatorialCoord&t,QString*e){if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->slewTo(t,e);emitState();return ok;}
bool ApplicationController::syncMount(const EquatorialCoord&t,QString*e){if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->syncTo(t,e);emitState();return ok;}
bool ApplicationController::mountStatus(MountStatus&s,QString*e)const{if(!mount_){if(e)*e="No mount connected";return false;}return mount_->status(s,e);}
bool ApplicationController::focuserStatus(FocuserStatus&s,QString*e)const{if(!focuser_){if(e)*e="No focuser connected";return false;}return focuser_->status(s,e);}
bool ApplicationController::moveFocuser(int p,QString*e){if(!focuser_){if(e)*e="No focuser connected";return false;}bool ok=focuser_->moveAbsolute(p,e);emitState();return ok;}
bool ApplicationController::haltFocuser(QString*e){if(!focuser_){if(e)*e="No focuser connected";return false;}return focuser_->halt(e);}
bool ApplicationController::setMountTracking(bool v,QString*e){if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->setTracking(v,e);emitState();return ok;}
bool ApplicationController::parkMount(bool v,QString*e){if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->park(v,e);emitState();return ok;}
bool ApplicationController::pulseGuide(GuideDirection d,int ms,QString*e){if(!mount_){if(e)*e="No mount connected";return false;}return mount_->pulseGuide(d,ms,e);}
GuidingStatus ApplicationController::startGuiding(){if(lastSolve_.success)guiding_.setTarget({lastSolve_.raDeg,lastSolve_.decDeg});auto s=guiding_.status();auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
GuidingStatus ApplicationController::stopGuiding(){guiding_.stop();auto s=guiding_.status();auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
GuidingStatus ApplicationController::guideUsingLastSolve(){auto s=guiding_.update({lastSolve_.raDeg,lastSolve_.decDeg},mount_.get());auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
void ApplicationController::clearPolarSamples(){polarSamples_.clear();emit polarSampleCountChanged(0);if(oalWsServer_)oalWsServer_->broadcast("polarSampleCount",QJsonObject{{"count",0}});}
bool ApplicationController::addPolarSample(QString*e){if(!lastSolve_.success){if(e)*e="Last plate solve is invalid";return false;}MountStatus m;if(!mountStatus(m,e))return false;polarSamples_.push_back({lastSolve_,m.coordinate.raDeg});int n=int(polarSamples_.size());emit polarSampleCountChanged(n);if(oalWsServer_)oalWsServer_->broadcast("polarSampleCount",QJsonObject{{"count",n}});return true;}
bool ApplicationController::slewPolarRaOffset(double d,QString*e){MountStatus m;if(!mountStatus(m,e))return false;EquatorialCoord t=m.coordinate;t.raDeg=std::fmod(t.raDeg+d+360.0,360.0);return slewMount(t,e);}
PolarAlignmentResult ApplicationController::estimatePolarAlignment(){return polarEstimator_.estimate(polarSamples_,profile_.observer,QDateTime::currentDateTimeUtc());}
bool ApplicationController::setSessionPlan(const QString&name,const std::vector<SessionTarget>&targets,QString*e){
    if(targets.empty()){if(e)*e="No targets supplied";return false;}scheduler_.setPlan(name,targets);return true;
}
bool ApplicationController::startSession(QString*e){if(!scheduler_.start()){if(e)*e="No targets supplied";return false;}return true;}
void ApplicationController::stopSession(){scheduler_.stop();}
bool ApplicationController::startOalServer(quint16 hp,bool we,quint16 wp,QString*e){if(!oalServer_)oalServer_=std::make_unique<OalServer>(this);if(!oalServer_->start(hp,e))return false;if(we){if(!oalWsServer_)oalWsServer_=std::make_unique<OalWsServer>(this);if(!oalWsServer_->start(wp,e)){oalServer_->stop();return false;}}settings_.saveServer(true,hp,we,wp);emit logMessage(QString("OAL server listening on %1; WebSocket %2").arg(hp).arg(we?QString::number(wp):"disabled"));return true;}
void ApplicationController::stopOalServer(){if(oalWsServer_)oalWsServer_->stop();if(oalServer_)oalServer_->stop();settings_.saveServer(false,settings_.oalPort(),false,settings_.wsPort());}
bool ApplicationController::oalRunning()const{return oalServer_&&oalServer_->isRunning();}
QJsonArray ApplicationController::devicesJson()const{QJsonArray a;auto add=[&](const std::shared_ptr<IDevice>&d,const QString&type){if(d)a.append(QJsonObject{{"id",d->id()},{"type",type},{"name",d->displayName()},{"backend",d->backendName()},{"connected",d->connectionState()==ConnectionState::Connected}});};add(camera_,"camera");add(mount_,"mount");add(focuser_,"focuser");return a;}
QJsonObject ApplicationController::cameraStatusJson()const{QJsonObject j{{"connected",bool(camera_)},{"backend",camera_?camera_->backendName():QString()},{"name",camera_?camera_->displayName():QString()}};if(camera_){auto s=camera_->sensorSize();j["width"]=s.width();j["height"]=s.height();}return j;}
QJsonObject ApplicationController::stateJson()const{QJsonObject j{{"timestampUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"devices",devicesJson()},{"solve",solveToJson(lastSolve_)},{"session",sessionJson(scheduler_.status())}};MountStatus m;if(mountStatus(m,nullptr))j["mount"]=QJsonObject{{"raDeg",m.coordinate.raDeg},{"decDeg",m.coordinate.decDeg},{"tracking",m.tracking},{"slewing",m.slewing},{"parked",m.parked}};FocuserStatus f;if(focuserStatus(f,nullptr))j["focuser"]=QJsonObject{{"position",f.position},{"moving",f.moving}};auto g=guiding_.status();j["guiding"]=QJsonObject{{"active",g.active},{"raErrorArcsec",g.raErrorArcsec},{"decErrorArcsec",g.decErrorArcsec},{"rmsArcsec",g.rmsArcsec}};return j;}
QJsonObject ApplicationController::nodeInfoJson()const{
    return {{"nodeId",QCoreApplication::applicationName()+"@"+QSysInfo::machineHostName()},
            {"version",QString::fromLatin1(OAS_VERSION)},
            {"httpRunning",bool(oalServer_&&oalServer_->isRunning())},
            {"httpPort",oalServer_?int(oalServer_->port()):0},
            {"wsRunning",bool(oalWsServer_&&oalWsServer_->isRunning())},
            {"wsPort",settings_.wsPort()},
            {"controlExecution","node-local"}};
}
void ApplicationController::emitState(){auto j=stateJson();emit stateChanged(j);if(oalWsServer_)oalWsServer_->broadcast("state",j);}
QImage ApplicationController::toQImage(const cv::Mat&i){if(i.empty())return{};cv::Mat rgb;if(i.channels()==1){cv::Mat u8;if(i.depth()==CV_16U){double mn,mx;cv::minMaxLoc(i,&mn,&mx);i.convertTo(u8,CV_8U,255.0/std::max(1.0,mx-mn),-mn*255.0/std::max(1.0,mx-mn));}else i.convertTo(u8,CV_8U);cv::cvtColor(u8,rgb,cv::COLOR_GRAY2RGB);}else cv::cvtColor(i,rgb,cv::COLOR_BGR2RGB);return QImage(rgb.data,rgb.cols,rgb.rows,int(rgb.step),QImage::Format_RGB888).copy();}
} // namespace oas
