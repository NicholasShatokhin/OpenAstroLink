#include "core/application_controller.h"
#include "algorithms/pattern_plate_solver.h"
#include "algorithms/astap_solver.h"
#include "algorithms/neural_solver.h"
#include "algorithms/star_catalog.h"
#include "backends/alpaca_devices.h"
#include "backends/gemini_eaf_focuser.h"
#include "backends/oal_client_devices.h"
#include "backends/oal_native_devices.h"
#include "backends/opencv_camera.h"
#include "backends/serial_lx200_mount.h"
#include "backends/simulated_devices.h"
#include "oal/oal_server.h"
#include "oal/driver_plugin_loader.h"
#include "oal/oal_ws_server.h"
#include "integrations/stellarium_telescope_server.h"
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
#include <QThread>
#include <QSerialPortInfo>
#include <QMetaObject>
#include <QSysInfo>
#include <opencv2/imgproc.hpp>
#ifdef OAS_HAVE_POSITIONING
#include <QGeoPositionInfoSource>
#include <QGeoCoordinate>
#endif
#include <algorithm>
#include <cmath>

namespace oas {
namespace {
QJsonObject nativeDescriptor(const std::shared_ptr<OalDriverPluginLoader> &loader,const QString &backend,const QString &type,QString *error){
    QString driverId,deviceId;if(!parseNativeBackendKey(backend,driverId,deviceId)){if(error)*error="Invalid native OAL backend key";return{};}
    if(!loader){if(error)*error="Native OAL driver registry unavailable";return{};}
    for(const auto &v:loader->devices()){const auto o=v.toObject();if(o.value("driverId").toString()==driverId&&o.value("id").toString()==deviceId&&o.value("type").toString()==type)return o;}
    if(error)*error=QString("Native OAL %1 device not found: %2/%3").arg(type,driverId,deviceId);return{};
}
QStringList nativeBackendsFor(const std::shared_ptr<OalDriverPluginLoader> &loader,const QString &type){
    QStringList out;if(!loader)return out;for(const auto &v:loader->devices()){const auto o=v.toObject();if(o.value("type").toString()!=type||!o.value("native").toBool())continue;out<<nativeBackendKey(o.value("driverId").toString(),o.value("id").toString());}out.removeDuplicates();return out;
}
QJsonObject migrateQhyDescriptor(const std::shared_ptr<OalDriverPluginLoader> &loader,const QString &selector){
    if(!loader)return{};QJsonArray matches;for(const auto&v:loader->devices()){auto o=v.toObject();if(o.value("driverId").toString()=="oal.qhy"&&o.value("type").toString()=="camera")matches.append(o);}
    bool numeric=false;int index=selector.trimmed().toInt(&numeric);if(selector.trimmed().isEmpty()){numeric=true;index=0;}
    if(numeric&&index>=0&&index<matches.size())return matches.at(index).toObject();for(const auto&v:matches){auto o=v.toObject();const QString id=o.value("id").toString();if(id==selector||id=="qhy:"+selector||o.value("name").toString()==selector)return o;}return{};
}
QJsonObject migrateCanonDescriptor(const std::shared_ptr<OalDriverPluginLoader> &loader,const QString &selector){
    if(!loader)return{};QJsonArray matches;for(const auto&v:loader->devices()){auto o=v.toObject();if(o.value("driverId").toString()=="oal.canon"&&o.value("type").toString()=="camera")matches.append(o);}
    bool numeric=false;int index=selector.trimmed().toInt(&numeric);if(selector.trimmed().isEmpty()){numeric=true;index=0;}
    if(numeric&&index>=0&&index<matches.size())return matches.at(index).toObject();for(const auto&v:matches){auto o=v.toObject();const QString id=o.value("id").toString();if(id==selector||id=="canon:"+selector||o.value("name").toString()==selector)return o;}return{};
}
std::shared_ptr<ICamera> makeCameraDevice(const std::shared_ptr<OalDriverPluginLoader> &loader,
                                          const QString &backend,const QString &endpoint,
                                          QString *error,QString *note=nullptr){
    std::shared_ptr<ICamera> d;QString driverId,deviceId;
    if(parseNativeBackendKey(backend,driverId,deviceId)){
        auto desc=nativeDescriptor(loader,backend,"camera",error);if(desc.isEmpty())return{};d=std::make_shared<NativeOalCamera>(loader,desc);
    } else if(backend=="qhy"){
        auto desc=migrateQhyDescriptor(loader,endpoint);if(desc.isEmpty()){if(error)*error="Legacy qhy binding could not be migrated: native oal.qhy driver/camera not found";return{};}
        d=std::make_shared<NativeOalCamera>(loader,desc);if(note)*note="Migrated legacy direct QHY binding to native OpenAstroLink driver";
    } else if(backend=="canon-gphoto2"){
        auto desc=migrateCanonDescriptor(loader,endpoint);
        if(!desc.isEmpty()){d=std::make_shared<NativeOalCamera>(loader,desc);if(note)*note="Migrated legacy Canon/libgphoto2 binding to native OpenAstroLink oal.canon driver";}
#ifdef OAS_HAVE_GPHOTO2
        else d=std::make_shared<CanonGPhotoCamera>();
#else
        else {if(error)*error="Legacy Canon binding could not be migrated: native oal.canon driver not found";return{};}
#endif
    } else if(backend=="simulated") d=std::make_shared<SimulatedCamera>();
    else if(backend=="opencv") d=std::make_shared<OpenCvCamera>(endpoint.toInt());
    else if(backend=="oal") d=std::make_shared<OalCameraClient>(QUrl(endpoint));
    else {if(error)*error="Unknown camera backend";return{};}
    return d;
}
template <typename F>
auto invokeOnControllerThread(QObject *owner,F &&fn)->decltype(fn()){
    using R=decltype(fn());
    if(QThread::currentThread()==owner->thread())return fn();
    R result{};
    QMetaObject::invokeMethod(owner,[&](){result=fn();},Qt::BlockingQueuedConnection);
    return result;
}

class ThreadMarshalledCamera final : public ICamera {
public:
    ThreadMarshalledCamera(QObject *owner,std::shared_ptr<ICamera> inner):owner_(owner),inner_(std::move(inner)),direct_(inner_->backendName().startsWith("native:")){}
    QString id()const override{return inner_->id();} QString displayName()const override{return inner_->displayName();} QString backendName()const override{return inner_->backendName();}
    ConnectionState connectionState()const override{return direct_?inner_->connectionState():invokeOnControllerThread(owner_,[this](){return inner_->connectionState();});}
    bool connectDevice(QString *e=nullptr)override{return direct_?inner_->connectDevice(e):invokeOnControllerThread(owner_,[this,e](){return inner_->connectDevice(e);});}
    void disconnectDevice()override{if(direct_||QThread::currentThread()==owner_->thread())inner_->disconnectDevice();else QMetaObject::invokeMethod(owner_,[this](){inner_->disconnectDevice();},Qt::BlockingQueuedConnection);}
    bool capture(const ExposureRequest&r,CameraFrame&f,QString*e=nullptr)override{return direct_?inner_->capture(r,f,e):invokeOnControllerThread(owner_,[this,&r,&f,e](){return inner_->capture(r,f,e);});}
    bool canAbortExposure()const override{return inner_->canAbortExposure();}
    bool abortExposure(QString*e=nullptr)override{return direct_?inner_->abortExposure(e):invokeOnControllerThread(owner_,[this,e](){return inner_->abortExposure(e);});}
    QSize sensorSize()const override{return direct_?inner_->sensorSize():invokeOnControllerThread(owner_,[this](){return inner_->sensorSize();});}
private:QObject *owner_;std::shared_ptr<ICamera> inner_;bool direct_{false};
};

class ThreadMarshalledFocuser final : public IFocuser {
public:
    ThreadMarshalledFocuser(QObject *owner,std::shared_ptr<IFocuser> inner):owner_(owner),inner_(std::move(inner)),direct_(inner_->backendName().startsWith("native:")){}
    QString id()const override{return inner_->id();} QString displayName()const override{return inner_->displayName();} QString backendName()const override{return inner_->backendName();}
    ConnectionState connectionState()const override{return direct_?inner_->connectionState():invokeOnControllerThread(owner_,[this](){return inner_->connectionState();});}
    bool connectDevice(QString *e=nullptr)override{return direct_?inner_->connectDevice(e):invokeOnControllerThread(owner_,[this,e](){return inner_->connectDevice(e);});}
    void disconnectDevice()override{if(direct_||QThread::currentThread()==owner_->thread())inner_->disconnectDevice();else QMetaObject::invokeMethod(owner_,[this](){inner_->disconnectDevice();},Qt::BlockingQueuedConnection);}
    bool status(FocuserStatus&s,QString*e=nullptr)override{return direct_?inner_->status(s,e):invokeOnControllerThread(owner_,[this,&s,e](){return inner_->status(s,e);});}
    bool moveAbsolute(int p,QString*e=nullptr)override{return direct_?inner_->moveAbsolute(p,e):invokeOnControllerThread(owner_,[this,p,e](){return inner_->moveAbsolute(p,e);});}
    bool moveRelative(int d,QString*e=nullptr)override{return direct_?inner_->moveRelative(d,e):invokeOnControllerThread(owner_,[this,d,e](){return inner_->moveRelative(d,e);});}
    bool halt(QString*e=nullptr)override{return direct_?inner_->halt(e):invokeOnControllerThread(owner_,[this,e](){return inner_->halt(e);});}
private:QObject *owner_;std::shared_ptr<IFocuser> inner_;bool direct_{false};
};

class ThreadMarshalledMount final : public IMount {
public:
    ThreadMarshalledMount(QObject *owner,std::shared_ptr<IMount> inner):owner_(owner),inner_(std::move(inner)),direct_(inner_->backendName().startsWith("native:")){}
    QString id()const override{return inner_->id();} QString displayName()const override{return inner_->displayName();} QString backendName()const override{return inner_->backendName();}
    ConnectionState connectionState()const override{return direct_?inner_->connectionState():invokeOnControllerThread(owner_,[this](){return inner_->connectionState();});}
    bool connectDevice(QString *e=nullptr)override{return direct_?inner_->connectDevice(e):invokeOnControllerThread(owner_,[this,e](){return inner_->connectDevice(e);});}
    void disconnectDevice()override{if(direct_||QThread::currentThread()==owner_->thread())inner_->disconnectDevice();else QMetaObject::invokeMethod(owner_,[this](){inner_->disconnectDevice();},Qt::BlockingQueuedConnection);}
    bool status(MountStatus&s,QString*e=nullptr)override{return direct_?inner_->status(s,e):invokeOnControllerThread(owner_,[this,&s,e](){return inner_->status(s,e);});}
    bool slewTo(const EquatorialCoord&t,QString*e=nullptr)override{return direct_?inner_->slewTo(t,e):invokeOnControllerThread(owner_,[this,&t,e](){return inner_->slewTo(t,e);});}
    bool abortMotion(QString*e=nullptr)override{return direct_?inner_->abortMotion(e):invokeOnControllerThread(owner_,[this,e](){return inner_->abortMotion(e);});}
    bool syncTo(const EquatorialCoord&t,QString*e=nullptr)override{return direct_?inner_->syncTo(t,e):invokeOnControllerThread(owner_,[this,&t,e](){return inner_->syncTo(t,e);});}
    bool setTracking(bool v,QString*e=nullptr)override{return direct_?inner_->setTracking(v,e):invokeOnControllerThread(owner_,[this,v,e](){return inner_->setTracking(v,e);});}
    bool park(bool v,QString*e=nullptr)override{return direct_?inner_->park(v,e):invokeOnControllerThread(owner_,[this,v,e](){return inner_->park(v,e);});}
    bool pulseGuide(GuideDirection d,int ms,QString*e=nullptr)override{return direct_?inner_->pulseGuide(d,ms,e):invokeOnControllerThread(owner_,[this,d,ms,e](){return inner_->pulseGuide(d,ms,e);});}
private:QObject *owner_;std::shared_ptr<IMount> inner_;bool direct_{false};
};
}
static QJsonObject sessionJson(const SessionStatus&s){return{{"id",s.id},{"name",s.name},{"active",s.active},{"targetIndex",s.targetIndex},{"targetCount",s.targetCount},{"completedFrames",s.completedFrames},{"state",s.state}};}
ApplicationController::ApplicationController(QObject *parent):ObservatoryController(parent),scheduler_(this),operations_(this){
    profile_=settings_.loadProfile();
    // Persisted serial overrides are applied before native drivers perform their
    // initial discovery. Explicit process environment variables (for example
    // --gemini-port on the node) always win over saved GUI settings.
    if(qEnvironmentVariable("OAL_GEMINI_PORT").trimmed().isEmpty()){const QString p=settings_.nativeSerialPort("oal.gemini");if(!p.isEmpty())qputenv("OAL_GEMINI_PORT",p.toUtf8());}
    if(qEnvironmentVariable("OAL_SKYWATCHER_PORT").trimmed().isEmpty()){const QString p=settings_.nativeSerialPort("oal.skywatcher");if(!p.isEmpty())qputenv("OAL_SKYWATCHER_PORT",p.toUtf8());}
    catalog_=std::make_shared<StarCatalog>();QString err;QString catalogPath=QDir(QCoreApplication::applicationDirPath()).filePath("config/stars_example.csv");if(!catalog_->loadCsv(catalogPath,&err)){catalogPath=QDir::current().filePath("config/stars_example.csv");catalog_->loadCsv(catalogPath,&err);}catalogSolver_=std::make_shared<PatternPlateSolver>(catalog_);astapSolver_=std::make_shared<AstapSolver>();neuralSolver_=std::make_shared<NeuralSolver>();solver_=astapSolver_->available(nullptr)?astapSolver_:catalogSolver_;
    driverLoader_=std::make_shared<OalDriverPluginLoader>();QStringList driverErrors;const int nativeCount=driverLoader_->scanDefaultPaths(&driverErrors);
    connect(driverLoader_.get(),&OalDriverPluginLoader::driverLog,this,[this](const QString&driver,int,const QString&message){emit logMessage(QString("[%1] %2").arg(driver,message));});
    connect(driverLoader_.get(),&OalDriverPluginLoader::driverEvent,this,[this](const QString&driver,const QString&device,const QJsonObject&event){QJsonObject p=event;p["driverId"]=driver;p["deviceId"]=device;if(oalWsServer_)oalWsServer_->broadcast("driverEvent",p);});
    QTimer::singleShot(0,this,[this,nativeCount,driverErrors](){emit logMessage(QString("Native OAL driver registry: %1 driver(s)").arg(nativeCount));for(const auto&e:driverErrors)emit logMessage("Native driver load warning: "+e);});
    #ifdef OAS_HAVE_POSITIONING
    if(profile_.observer.latitudeDeg==0.0&&profile_.observer.longitudeDeg==0.0)QTimer::singleShot(0,this,&ApplicationController::requestSystemLocation);
#endif
    connect(&scheduler_,&Scheduler::statusChanged,this,[this](const SessionStatus&s){auto j=sessionJson(s);emit sessionChanged(j);if(oalWsServer_)oalWsServer_->broadcast("sessionUpdate",j);emitState();});
    connect(&operations_,&OperationManager::operationChanged,this,[this](const QJsonObject&o){
        emit operationChanged(o);
        if(oalWsServer_)oalWsServer_->broadcast("operation",o);
        const QString kind=o.value("kind").toString(),state=o.value("state").toString();
        if(kind=="autofocus.run"&&(state=="succeeded"||state=="failed"||state=="cancelled")){
            auto result=o.value("result").toObject();
            if(result.isEmpty())result={{"success",false},{"message",state=="cancelled"?"Autofocus cancelled":o.value("problem").toObject().value("message").toString("Autofocus failed")}};
            emit autofocusCompleted(result);
            if(oalWsServer_)oalWsServer_->broadcast("autofocusResult",result);
            emit logMessage(result.value("message").toString());
        }
        emitState();
    });
}
ApplicationController::~ApplicationController(){operations_.shutdown();stopOalServer();disconnectDevices(false);}
void ApplicationController::setProfile(const TelescopeProfile&p){profile_=p;settings_.saveProfile(p);emit profileChanged();emitState();}
QStringList ApplicationController::cameraBackends()const{QStringList x=nativeBackendsFor(driverLoader_,"camera");x<<"simulated"<<"opencv"<<"oal";
#ifdef OAS_HAVE_GPHOTO2
x<<"canon-gphoto2";
#endif
return x;}
QStringList ApplicationController::mountBackends()const{QStringList x=nativeBackendsFor(driverLoader_,"mount");x<<"simulated"<<"serial-lx200"<<"ascom-alpaca"<<"oal";
#ifdef OAS_HAVE_INDI
x<<"indi";
#endif
return x;}
QStringList ApplicationController::focuserBackends()const{QStringList x=nativeBackendsFor(driverLoader_,"focuser");x<<"simulated"<<"gemini-eaf"<<"ascom-alpaca"<<"oal";
#ifdef OAS_HAVE_INDI
x<<"indi";
#endif
return x;}
QStringList ApplicationController::solverBackends()const{
    QStringList x;
    if(astapSolver_&&astapSolver_->available(nullptr))x<<"astap";
    x<<"catalog-pattern"<<"neural";
    return x;
}
bool ApplicationController::selectSolver(const QString&name,QString*e){
    if(name=="catalog-pattern")solver_=catalogSolver_;
    else if(name=="astap"){
        QString why;
        if(!astapSolver_||!astapSolver_->available(&why)){if(e)*e=why.isEmpty()?"ASTAP is not available on this node":why;return false;}
        solver_=astapSolver_;
    }
    else if(name=="neural")solver_=neuralSolver_;
    else{if(e)*e="Unknown solver backend";return false;}
    emit logMessage("Selected solver: "+solver_->name());return true;
}
bool ApplicationController::loadCatalog(const QString&p,QString*e){bool ok=catalog_->loadCsv(p,e);if(ok)emit logMessage(QString("Loaded %1 catalog stars").arg(catalog_->stars().size()));return ok;}
bool ApplicationController::loadNeuralModel(const QString&p,QString*e){bool ok=neuralSolver_->loadModel(p,e);if(ok)emit logMessage("Neural model loaded: "+p);return ok;}
bool ApplicationController::connectCamera(const QString&b,const QString&e,QString*err){
    if(!ensureResourcesAvailable({"camera"},err))return false;
    QString note;auto d=makeCameraDevice(driverLoader_,b,e,err,&note);if(!d)return false;
    if(!d->connectDevice(err))return false;camera_=std::move(d);settings_.saveCameraBinding({camera_->backendName(),e,true});
    if(!note.isEmpty())emit logMessage(note);emit logMessage("Main camera connected: "+camera_->displayName());emitState();return true;
}
bool ApplicationController::connectGuideCamera(const QString&b,const QString&e,QString*err){
    if(!ensureResourcesAvailable({"camera.guide"},err))return false;
    QString note;auto d=makeCameraDevice(driverLoader_,b,e,err,&note);if(!d)return false;
    if(!d->connectDevice(err))return false;guideCamera_=std::move(d);settings_.saveGuideCameraBinding({guideCamera_->backendName(),e,true});
    if(!note.isEmpty())emit logMessage(note);emit logMessage("Guide camera connected: "+guideCamera_->displayName());emitState();return true;
}
bool ApplicationController::connectMount(const QString&b,const QString&e,QString*err){if(!ensureResourcesAvailable({"mount"},err))return false;std::shared_ptr<IMount>d;QString driverId,deviceId;if(parseNativeBackendKey(b,driverId,deviceId)){auto desc=nativeDescriptor(driverLoader_,b,"mount",err);if(desc.isEmpty())return false;d=std::make_shared<NativeOalMount>(driverLoader_,desc);}
else if(b=="simulated")d=std::make_shared<SimulatedMount>();else if(b=="serial-lx200")d=std::make_shared<SerialLx200Mount>(e);else if(b=="ascom-alpaca")d=std::make_shared<AlpacaMount>(QUrl(e));else if(b=="oal")d=std::make_shared<OalMountClient>(QUrl(e));
#ifdef OAS_HAVE_INDI
else if(b=="indi")d=std::make_shared<IndiMount>(e);
#endif
else{if(err)*err="Unknown mount backend";return false;}if(!d->connectDevice(err))return false;mount_=std::move(d);settings_.saveMountBinding({mount_->backendName(),e,true});emit logMessage("Mount connected: "+mount_->displayName());emitState();return true;}
bool ApplicationController::connectFocuser(const QString&b,const QString&e,QString*err){if(!ensureResourcesAvailable({"focuser"},err))return false;std::shared_ptr<IFocuser>d;QString driverId,deviceId;if(parseNativeBackendKey(b,driverId,deviceId)){auto desc=nativeDescriptor(driverLoader_,b,"focuser",err);if(desc.isEmpty())return false;d=std::make_shared<NativeOalFocuser>(driverLoader_,desc);}
else if(b=="simulated")d=std::make_shared<SimulatedFocuser>();else if(b=="gemini-eaf")d=std::make_shared<GeminiEafFocuser>(e);else if(b=="ascom-alpaca")d=std::make_shared<AlpacaFocuser>(QUrl(e));else if(b=="oal")d=std::make_shared<OalFocuserClient>(QUrl(e));
#ifdef OAS_HAVE_INDI
else if(b=="indi")d=std::make_shared<IndiFocuser>(e);
#endif
else{if(err)*err="Unknown focuser backend";return false;}if(!d->connectDevice(err))return false;focuser_=std::move(d);settings_.saveFocuserBinding({focuser_->backendName(),e,true});emit logMessage("Focuser connected: "+focuser_->displayName());emitState();return true;}
bool ApplicationController::ensureResourcesAvailable(const QStringList&resources,QString*error)const{
    for(const auto &resource:resources){QString owner;if(operations_.isResourceLocked(resource,&owner)){if(error)*error=QString("Resource %1 is locked by operation %2").arg(resource,owner);return false;}}
    return true;
}
bool ApplicationController::disconnectCamera(QString*error){if(!ensureResourcesAvailable({"camera"},error))return false;if(camera_)camera_->disconnectDevice();camera_.reset();auto b=settings_.cameraBinding();b.autoConnect=false;settings_.saveCameraBinding(b);emit logMessage("Main camera disconnected");emitState();return true;}
bool ApplicationController::disconnectGuideCamera(QString*error){if(!ensureResourcesAvailable({"camera.guide"},error))return false;if(guideCamera_)guideCamera_->disconnectDevice();guideCamera_.reset();auto b=settings_.guideCameraBinding();b.autoConnect=false;settings_.saveGuideCameraBinding(b);emit logMessage("Guide camera disconnected");emitState();return true;}
bool ApplicationController::disconnectMount(QString*error){if(!ensureResourcesAvailable({"mount"},error))return false;if(mount_)mount_->disconnectDevice();mount_.reset();auto b=settings_.mountBinding();b.autoConnect=false;settings_.saveMountBinding(b);emit logMessage("Mount disconnected");emitState();return true;}
bool ApplicationController::disconnectFocuser(QString*error){if(!ensureResourcesAvailable({"focuser"},error))return false;if(focuser_)focuser_->disconnectDevice();focuser_.reset();auto b=settings_.focuserBinding();b.autoConnect=false;settings_.saveFocuserBinding(b);emit logMessage("Focuser disconnected");emitState();return true;}
bool ApplicationController::disconnectAll(QString*error){if(!ensureResourcesAvailable({"camera","camera.guide","mount","focuser"},error))return false;disconnectDevices(true);return true;}
void ApplicationController::disconnectDevices(bool clearAutoConnect){
    if(camera_)camera_->disconnectDevice();if(guideCamera_)guideCamera_->disconnectDevice();if(mount_)mount_->disconnectDevice();if(focuser_)focuser_->disconnectDevice();camera_.reset();guideCamera_.reset();mount_.reset();focuser_.reset();
    if(clearAutoConnect){auto c=settings_.cameraBinding();c.autoConnect=false;settings_.saveCameraBinding(c);auto g=settings_.guideCameraBinding();g.autoConnect=false;settings_.saveGuideCameraBinding(g);auto m=settings_.mountBinding();m.autoConnect=false;settings_.saveMountBinding(m);auto f=settings_.focuserBinding();f.autoConnect=false;settings_.saveFocuserBinding(f);}
    emitState();
}
bool ApplicationController::restoreConfiguredDevices(QStringList *errors){
    bool allOk=true;
    auto restore=[&](const QString &kind,const DeviceBinding &b,auto fn){
        if(!b.autoConnect||b.backend.isEmpty())return;
        QString e;if(!(this->*fn)(b.backend,b.endpoint,&e)){allOk=false;if(errors)errors->append(kind+": "+e);emit logMessage("Auto-connect "+kind+" failed: "+e);}
    };
    if(!camera_)restore("camera",settings_.cameraBinding(),&ApplicationController::connectCamera);
    if(!guideCamera_)restore("guide camera",settings_.guideCameraBinding(),&ApplicationController::connectGuideCamera);
    if(!mount_)restore("mount",settings_.mountBinding(),&ApplicationController::connectMount);
    if(!focuser_)restore("focuser",settings_.focuserBinding(),&ApplicationController::connectFocuser);
    return allOk;
}
void ApplicationController::commitCapturedFrame(const CameraFrame&f){
    previousFrame_=lastFrame_;lastFrame_=f;emit frameCaptured(toQImage(f.image),f.id);emit logMessage(QString("Captured %1 × %2 frame %3").arg(f.image.cols).arg(f.image.rows).arg(f.id));
    if(oalWsServer_)oalWsServer_->broadcast("frameReady",QJsonObject{{"frameId",f.id},{"capturedUtc",f.capturedUtc.toString(Qt::ISODateWithMs)},{"width",f.image.cols},{"height",f.image.rows},{"exposureSec",f.exposureSec},{"gain",f.gain}});
    emitState();
}
bool ApplicationController::capture(const ExposureRequest&r,CameraFrame*out,QString*err){if(!ensureResourcesAvailable({"camera"},err))return false;if(!camera_){if(err)*err="No camera connected";return false;}CameraFrame f;if(!camera_->capture(r,f,err))return false;commitCapturedFrame(f);if(out)*out=f;return true;}
QString ApplicationController::startCapture(const ExposureRequest&r,QString*error){
    if(!camera_){if(error)*error="No camera connected";return{};}auto cam=camera_;
    const QString id=operations_.submit("camera.exposure",{"camera"},true,[this,cam,r](OperationContext&ctx){
        OperationOutcome out;ctx.reportProgress(0.02,"configuring",{{"exposureSec",r.exposureSec},{"gain",r.gain},{"binX",r.binX},{"binY",r.binY}});
        if(ctx.isCancellationRequested()){out.cancelled=true;return out;}
        ctx.reportProgress(0.08,"exposing",{{"exposureSec",r.exposureSec}});CameraFrame frame;QString err;
        // The camera resource lock guarantees serialization. Long captures run on
        // the operation worker so the node HTTP/WebSocket event loop remains responsive.
        if(!cam->capture(r,frame,&err)){if(ctx.isCancellationRequested()){out.cancelled=true;return out;}out.problem={{"code","CAPTURE_FAILED"},{"message",err}};return out;}
        if(ctx.isCancellationRequested()){out.cancelled=true;return out;}ctx.reportProgress(0.90,"readout",{{"frameId",frame.id}});
        QMetaObject::invokeMethod(this,[this,frame](){commitCapturedFrame(frame);},Qt::BlockingQueuedConnection);
        out.success=true;out.result=QJsonObject{{"frameId",frame.id},{"capturedUtc",frame.capturedUtc.toString(Qt::ISODateWithMs)},{"width",frame.image.cols},{"height",frame.image.rows},{"exposureSec",frame.exposureSec},{"gain",frame.gain},{"previewResource",QString("/api/v1/frames/%1/preview").arg(frame.id)}};ctx.reportProgress(1.0,"completed");return out;
    });
    emit logMessage(QString("Exposure operation accepted: %1 (%2 s)").arg(id).arg(r.exposureSec,0,'f',4));return id;
}
QString ApplicationController::startGuideCapture(const ExposureRequest&r,QString*error){
    if(!guideCamera_){if(error)*error="No guide camera connected";return{};}auto cam=guideCamera_;
    const QString id=operations_.submit("camera.guide.exposure",{"camera.guide"},true,[this,cam,r](OperationContext&ctx){
        OperationOutcome out;ctx.reportProgress(0.05,"exposing",{{"exposureSec",r.exposureSec}});
        ThreadMarshalledCamera proxy(this,cam);CameraFrame frame;QString err;
        if(!proxy.capture(r,frame,&err)){if(ctx.isCancellationRequested()){out.cancelled=true;return out;}out.problem={{"code","GUIDE_CAPTURE_FAILED"},{"message",err}};return out;}
        if(ctx.isCancellationRequested()){out.cancelled=true;return out;}
        QMetaObject::invokeMethod(this,[this,frame](){lastGuideFrame_=frame;emit logMessage(QString("Guide frame %1: %2 x %3").arg(frame.id).arg(frame.image.cols).arg(frame.image.rows));emitState();},Qt::BlockingQueuedConnection);
        out.success=true;out.result=QJsonObject{{"frameId",frame.id},{"width",frame.image.cols},{"height",frame.image.rows},{"exposureSec",frame.exposureSec},{"role","guide"}};ctx.reportProgress(1.0,"completed");return out;
    });
    emit logMessage(QString("Guide exposure operation accepted: %1 (%2 s)").arg(id).arg(r.exposureSec,0,'f',4));return id;
}
SolveResult ApplicationController::solveLast(const SolveHint&h){if(lastFrame_.image.empty()){lastSolve_.message="No captured frame";lastSolve_.success=false;}else lastSolve_=solver_->solve(lastFrame_,profile_,h);auto j=solveToJson(lastSolve_);QJsonArray stars;for(const auto&s:lastSolve_.imageStars)stars.append(QJsonObject{{"x",s.positionPx.x},{"y",s.positionPx.y},{"flux",s.flux},{"peak",s.peak},{"hfrPx",s.hfrPx}});j["imageStars"]=stars;emit solveCompleted(j);if(oalWsServer_)oalWsServer_->broadcast("solveResult",j);emit logMessage(lastSolve_.message);emitState();return lastSolve_;}
AutofocusResult ApplicationController::autofocus(const AutofocusRequest&r){AutofocusResult x;QString busy;if(!ensureResourcesAvailable({"camera","focuser"},&busy)){x.message=busy;}else if(!camera_||!focuser_){x.message="Camera and focuser must be connected";}else x=autofocusEngine_.run(*camera_,*focuser_,r,[this](const FocusSample&s){QJsonObject j{{"position",s.position},{"score",s.score},{"spread",s.spread}};emit autofocusProgress(j);if(oalWsServer_)oalWsServer_->broadcast("autofocusProgress",j);});auto j=autofocusToJson(x);emit autofocusCompleted(j);if(oalWsServer_)oalWsServer_->broadcast("autofocusResult",j);emit logMessage(x.message);emitState();return x;}
QString ApplicationController::startAutofocus(const AutofocusRequest&r,QString*error){
    if(!camera_||!focuser_){if(error)*error="Camera and focuser must be connected";return{};}
    auto cam=camera_;auto foc=focuser_;const int coarseCount=std::max(1,r.rangeSteps/std::max(1,r.coarseStep)+1);const int fineHalf=std::max(r.coarseStep*2,r.fineStep*3);const int fineCount=std::max(1,(2*fineHalf)/std::max(1,r.fineStep)+1);const int expected=coarseCount+fineCount;
    const QString id=operations_.submit("autofocus.run",{"camera","focuser"},true,[this,cam,foc,r,expected](OperationContext&ctx){
        ThreadMarshalledCamera cameraProxy(this,cam);ThreadMarshalledFocuser focuserProxy(this,foc);int sampleNo=0;
        ctx.reportProgress(0.0,"starting");
        auto result=autofocusEngine_.run(cameraProxy,focuserProxy,r,[this,&ctx,&sampleNo,expected](const FocusSample&s){
            ++sampleNo;QJsonObject j{{"position",s.position},{"score",s.score},{"spread",s.spread},{"sample",sampleNo},{"expectedSamples",expected}};
            ctx.reportProgress(std::min(0.95,double(sampleNo)/std::max(1,expected)),"focus.scan",j);
            QMetaObject::invokeMethod(this,[this,j](){emit autofocusProgress(j);if(oalWsServer_)oalWsServer_->broadcast("autofocusProgress",j);},Qt::QueuedConnection);
        },[&ctx](){return ctx.isCancellationRequested();});
        OperationOutcome out;out.result=autofocusToJson(result);
        if(ctx.isCancellationRequested()||result.message=="Autofocus cancelled"){focuserProxy.halt(nullptr);out.cancelled=true;return out;}
        if(result.success){out.success=true;ctx.reportProgress(1.0,"completed");}
        else out.problem={{"code","AUTOFOCUS_FAILED"},{"message",result.message}};
        return out;
    });
    emit logMessage("Autofocus operation accepted: "+id);return id;
}
bool ApplicationController::cancelOperation(const QString&id,QString*error){
    const auto before=operations_.operationJson(id);const QString kind=before.value("kind").toString();const bool runningExposure=kind=="camera.exposure"&&before.value("state").toString()=="running";const bool runningGuideExposure=kind=="camera.guide.exposure"&&before.value("state").toString()=="running";
    if(!operations_.cancel(id,error))return false;
    if(runningExposure&&camera_&&camera_->canAbortExposure()){QString abortError;if(!camera_->abortExposure(&abortError)&&!abortError.isEmpty())emit logMessage("Exposure abort warning: "+abortError);}
    else if(runningExposure&&camera_)emit logMessage("Exposure cancellation requested; this camera backend cannot interrupt an in-progress capture, so the frame will be discarded when readout returns");
    if(runningGuideExposure&&guideCamera_&&guideCamera_->canAbortExposure())guideCamera_->abortExposure(nullptr);
    return true;
}
QJsonObject ApplicationController::operation(const QString&id,QString*error)const{auto o=operations_.operationJson(id);if(o.isEmpty()&&error)*error="Operation not found";return o;}
QJsonArray ApplicationController::operations(bool activeOnly)const{return operations_.operationsJson(activeOnly);}
FrameMotion ApplicationController::estimateLastMotion(){FrameMotion m;if(previousFrame_.image.empty()||lastFrame_.image.empty()){emit logMessage("Two captured frames are required");return m;}auto a=starDetector_.detect(previousFrame_.image);auto b=starDetector_.detect(lastFrame_.image);m=motionEstimator_.estimate(a,b);QJsonObject j{{"valid",m.valid},{"dxPx",m.dxPx},{"dyPx",m.dyPx},{"rotationDeg",m.rotationDeg},{"scale",m.scale},{"inliers",m.inliers}};emit motionEstimated(j);if(oalWsServer_)oalWsServer_->broadcast("motion",j);return m;}
void ApplicationController::requestSystemLocation(){
#ifdef OAS_HAVE_POSITIONING
    if(!positionSource_){positionSource_=QGeoPositionInfoSource::createDefaultSource(this);if(!positionSource_){emit logMessage("No system positioning source is available");return;}connect(positionSource_,&QGeoPositionInfoSource::positionUpdated,this,[this](const QGeoPositionInfo&i){auto c=i.coordinate();profile_.observer.latitudeDeg=c.latitude();profile_.observer.longitudeDeg=c.longitude();if(c.type()==QGeoCoordinate::Coordinate3D)profile_.observer.elevationM=c.altitude();settings_.saveProfile(profile_);emit profileChanged();emit logMessage(QString("System location: %1, %2").arg(c.latitude(),0,'f',6).arg(c.longitude(),0,'f',6));emitState();});connect(positionSource_,&QGeoPositionInfoSource::errorOccurred,this,[this](QGeoPositionInfoSource::Error){emit logMessage("System location request failed");});}positionSource_->requestUpdate(10000);
#else
    emit logMessage("Qt Positioning was not available at build time; enter location manually");
#endif
}
bool ApplicationController::slewMount(const EquatorialCoord&t,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->slewTo(t,e);if(ok)emit logMessage(QString("Mount slew accepted: RA=%1°, DEC=%2°").arg(t.raDeg,0,'f',6).arg(t.decDeg,0,'f',6));emitState();return ok;}
QString ApplicationController::startMountSlew(const EquatorialCoord&t,QString*e){
    if(!mount_){if(e)*e="No mount connected";return{};}auto mount=mount_;
    const QString id=operations_.submit("mount.slew",{"mount"},true,[this,mount,t](OperationContext&ctx){
        ThreadMarshalledMount proxy(this,mount);QString err;OperationOutcome out;ctx.reportProgress(0.05,"commanding",{{"raDeg",t.raDeg},{"decDeg",t.decDeg}});
        if(!proxy.slewTo(t,&err)){out.problem={{"code","MOUNT_SLEW_FAILED"},{"message",err}};return out;}
        ctx.reportProgress(0.25,"slewing");
        for(int i=0;i<3000;++i){
            if(ctx.isCancellationRequested()){proxy.abortMotion(nullptr);out.cancelled=true;return out;}
            MountStatus st;if(!proxy.status(st,&err)){out.problem={{"code","MOUNT_STATUS_FAILED"},{"message",err}};return out;}
            if(!st.slewing){out.success=true;out.result=QJsonObject{{"raDeg",st.coordinate.raDeg},{"decDeg",st.coordinate.decDeg},{"tracking",st.tracking},{"parked",st.parked}};ctx.reportProgress(1.0,"completed");return out;}
            ctx.reportProgress(0.25,"slewing",{{"raDeg",st.coordinate.raDeg},{"decDeg",st.coordinate.decDeg}});QThread::msleep(200);
        }
        proxy.abortMotion(nullptr);out.problem={{"code","MOUNT_SLEW_TIMEOUT"},{"message","Mount slew did not complete within 10 minutes"}};return out;
    });
    emit logMessage(QString("Mount slew operation accepted: %1 → RA=%2°, DEC=%3°").arg(id).arg(t.raDeg,0,'f',6).arg(t.decDeg,0,'f',6));return id;
}
bool ApplicationController::abortMountMotion(QString*e){QString owner;if(operations_.isResourceLocked("mount",&owner))operations_.cancel(owner,nullptr);if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->abortMotion(e);if(ok)emit logMessage("Mount motion abort requested");emitState();return ok;}
bool ApplicationController::syncMount(const EquatorialCoord&t,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->syncTo(t,e);if(ok)emit logMessage(QString("Mount sync: RA=%1°, DEC=%2°").arg(t.raDeg,0,'f',6).arg(t.decDeg,0,'f',6));emitState();return ok;}
bool ApplicationController::mountStatus(MountStatus&s,QString*e)const{if(!mount_){if(e)*e="No mount connected";return false;}return mount_->status(s,e);}
bool ApplicationController::focuserStatus(FocuserStatus&s,QString*e)const{if(!focuser_){if(e)*e="No focuser connected";return false;}return focuser_->status(s,e);}
bool ApplicationController::moveFocuser(int p,QString*e){if(!ensureResourcesAvailable({"focuser"},e))return false;if(!focuser_){if(e)*e="No focuser connected";return false;}bool ok=focuser_->moveAbsolute(p,e);emitState();return ok;}
bool ApplicationController::haltFocuser(QString*e){QString owner;if(operations_.isResourceLocked("focuser",&owner))operations_.cancel(owner,nullptr);if(!focuser_){if(e)*e="No focuser connected";return false;}bool ok=focuser_->halt(e);if(ok)emit logMessage("Focuser halt requested");return ok;}
bool ApplicationController::setMountTracking(bool v,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->setTracking(v,e);if(ok)emit logMessage(QString("Mount tracking %1").arg(v?"ON":"OFF"));emitState();return ok;}
bool ApplicationController::parkMount(bool v,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->park(v,e);if(ok)emit logMessage(QString("Mount %1").arg(v?"parked":"unparked"));emitState();return ok;}
bool ApplicationController::pulseGuide(GuideDirection d,int ms,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}return mount_->pulseGuide(d,ms,e);}
GuidingStatus ApplicationController::startGuiding(){if(lastSolve_.success)guiding_.setTarget({lastSolve_.raDeg,lastSolve_.decDeg});auto s=guiding_.status();auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
GuidingStatus ApplicationController::stopGuiding(){guiding_.stop();auto s=guiding_.status();auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
GuidingStatus ApplicationController::guideUsingLastSolve(){QString busy;if(!ensureResourcesAvailable({"mount"},&busy)){emit logMessage("Guide update skipped: "+busy);return guiding_.status();}auto s=guiding_.update({lastSolve_.raDeg,lastSolve_.decDeg},mount_.get());auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
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
bool ApplicationController::startStellariumServer(quint16 port,QString*e){
    if(!stellariumServer_){
        stellariumServer_=std::make_unique<StellariumTelescopeServer>(this);
        connect(stellariumServer_.get(), &StellariumTelescopeServer::logMessage, this, [this](const QString &message){ emit logMessage(message); });
    }
    if(!stellariumServer_->start(port,e))return false;settings_.saveStellarium(true,port);emitState();return true;
}
void ApplicationController::stopStellariumServer(){if(stellariumServer_)stellariumServer_->stop();settings_.saveStellarium(false,settings_.stellariumPort());emitState();}
bool ApplicationController::stellariumRunning()const{return stellariumServer_&&stellariumServer_->isRunning();}
quint16 ApplicationController::stellariumPort()const{return stellariumServer_&&stellariumServer_->isRunning()?stellariumServer_->port():settings_.stellariumPort();}
void ApplicationController::refreshState(){emitState();}
QJsonArray ApplicationController::devicesJson()const{QJsonArray a;auto add=[&](const std::shared_ptr<IDevice>&d,const QString&type,const QString&role,const DeviceBinding&binding){if(d){const QString backend=d->backendName();a.append(QJsonObject{{"id",d->id()},{"type",type},{"role",role},{"name",d->displayName()},{"backend",backend},{"endpoint",binding.endpoint},{"connected",d->connectionState()==ConnectionState::Connected},{"nativeOal",backend.startsWith("native:")}});}};add(camera_,"camera","main",settings_.cameraBinding());add(guideCamera_,"camera","guide",settings_.guideCameraBinding());add(mount_,"mount","main",settings_.mountBinding());add(focuser_,"focuser","main",settings_.focuserBinding());return a;}
QJsonObject ApplicationController::cameraStatusJson()const{QJsonObject j{{"connected",bool(camera_)},{"backend",camera_?camera_->backendName():QString()},{"name",camera_?camera_->displayName():QString()}};if(camera_){auto s=camera_->sensorSize();j["width"]=s.width();j["height"]=s.height();j["canAbortExposure"]=camera_->canAbortExposure();}return j;}
bool ApplicationController::frameById(const QString&id,CameraFrame&frame,QString*error)const{if(!lastFrame_.image.empty()&&(id==lastFrame_.id||id=="latest")){frame=lastFrame_;return true;}if(!lastGuideFrame_.image.empty()&&(id==lastGuideFrame_.id||id=="latest-guide")){frame=lastGuideFrame_;return true;}if(!previousFrame_.image.empty()&&id==previousFrame_.id){frame=previousFrame_;return true;}if(error)*error="Frame is no longer available in the in-memory preview cache";return false;}
QJsonObject ApplicationController::stateJson()const{QJsonObject j{{"timestampUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"devices",devicesJson()},{"solve",solveToJson(lastSolve_)},{"session",sessionJson(scheduler_.status())},{"operations",operations_.operationsJson(true)},{"resourceLocks",operations_.locksJson()},{"stellarium",QJsonObject{{"running",stellariumRunning()},{"port",int(stellariumPort())}}}};if(!lastFrame_.image.empty())j["lastFrame"]=QJsonObject{{"frameId",lastFrame_.id},{"capturedUtc",lastFrame_.capturedUtc.toString(Qt::ISODateWithMs)},{"width",lastFrame_.image.cols},{"height",lastFrame_.image.rows},{"exposureSec",lastFrame_.exposureSec},{"gain",lastFrame_.gain},{"role","main"}};if(!lastGuideFrame_.image.empty())j["lastGuideFrame"]=QJsonObject{{"frameId",lastGuideFrame_.id},{"capturedUtc",lastGuideFrame_.capturedUtc.toString(Qt::ISODateWithMs)},{"width",lastGuideFrame_.image.cols},{"height",lastGuideFrame_.image.rows},{"exposureSec",lastGuideFrame_.exposureSec},{"gain",lastGuideFrame_.gain},{"role","guide"}};MountStatus m;if(mountStatus(m,nullptr))j["mount"]=QJsonObject{{"raDeg",m.coordinate.raDeg},{"decDeg",m.coordinate.decDeg},{"tracking",m.tracking},{"slewing",m.slewing},{"parked",m.parked},{"pierSide",m.pierSide}};FocuserStatus f;if(focuserStatus(f,nullptr)){QJsonObject fj{{"position",f.position},{"moving",f.moving}};if(f.temperatureC)fj["temperatureC"]=*f.temperatureC;j["focuser"]=fj;}auto g=guiding_.status();j["guiding"]=QJsonObject{{"active",g.active},{"raErrorArcsec",g.raErrorArcsec},{"decErrorArcsec",g.decErrorArcsec},{"rmsArcsec",g.rmsArcsec}};return j;}
QJsonObject ApplicationController::nodeInfoJson()const{
    return {{"nodeId",QCoreApplication::applicationName()+"@"+QSysInfo::machineHostName()},
            {"version",QString::fromLatin1(OAS_VERSION)},
            {"httpRunning",bool(oalServer_&&oalServer_->isRunning())},
            {"httpPort",oalServer_?int(oalServer_->port()):0},
            {"wsRunning",bool(oalWsServer_&&oalWsServer_->isRunning())},
            {"wsPort",settings_.wsPort()},
            {"nativeDriverCount",driverLoader_?int(driverLoader_->drivers().size()):0},
            {"controlExecution","node-local"}};
}
bool ApplicationController::refreshNativeDiscovery(QString *error){
    if(!driverLoader_){if(error)*error="Native OAL driver registry unavailable";return false;}
    QStringList errors;const auto devices=driverLoader_->refreshDevices(&errors);
    if(!errors.isEmpty()){for(const auto&e:errors)emit logMessage("Native discovery warning: "+e);if(error)*error=errors.join("; ");}
    emit logMessage(QString("Native OAL discovery refreshed: %1 device(s)").arg(devices.size()));
    emitState();
    return errors.isEmpty();
}
QJsonArray ApplicationController::availableSerialPorts()const{
    QJsonArray out;
    for(const auto &info:QSerialPortInfo::availablePorts()){
        QJsonObject p{{"port",info.portName()},{"systemLocation",info.systemLocation()},{"description",info.description()},{"manufacturer",info.manufacturer()},{"serialNumber",info.serialNumber()}};
        if(info.hasVendorIdentifier())p["vendorId"]=int(info.vendorIdentifier());
        if(info.hasProductIdentifier())p["productId"]=int(info.productIdentifier());
        out.append(p);
    }
    return out;
}
QString ApplicationController::nativeSerialPortOverride(const QString&driverId)const{
    if(driverId=="oal.gemini")return qEnvironmentVariable("OAL_GEMINI_PORT").trimmed();
    if(driverId=="oal.skywatcher")return qEnvironmentVariable("OAL_SKYWATCHER_PORT").trimmed();
    return {};
}
bool ApplicationController::setNativeSerialPortOverride(const QString&driverId,const QString&port,QString*error){
    const QString value=port.trimmed();QString envName;
    if(driverId=="oal.gemini")envName="OAL_GEMINI_PORT";
    else if(driverId=="oal.skywatcher")envName="OAL_SKYWATCHER_PORT";
    else{if(error)*error="Serial-port override is supported only for oal.gemini and oal.skywatcher";return false;}
    const QByteArray env=envName.toUtf8();
    if(value.isEmpty())qunsetenv(env.constData());else qputenv(env.constData(),value.toUtf8());
    settings_.saveNativeSerialPort(driverId,value);
    emit logMessage(value.isEmpty()?QString("%1 serial discovery set to automatic scan").arg(driverId):QString("%1 serial discovery pinned to %2").arg(driverId,value));
    return refreshNativeDiscovery(error);
}
QJsonArray ApplicationController::nativeDriversJson()const{return driverLoader_?driverLoader_->drivers():QJsonArray{};}
QJsonArray ApplicationController::nativeDevicesJson()const{return driverLoader_?driverLoader_->devices():QJsonArray{};}
QJsonObject ApplicationController::nativeCapabilitiesJson(const QString&driverId,const QString&deviceId,QString*error)const{return driverLoader_?driverLoader_->capabilities(driverId,deviceId,error):QJsonObject{};}
void ApplicationController::emitState(){auto j=stateJson();emit stateChanged(j);if(oalWsServer_)oalWsServer_->broadcast("state",j);}
QImage ApplicationController::toQImage(const cv::Mat&i){if(i.empty())return{};cv::Mat rgb;if(i.channels()==1){cv::Mat u8;if(i.depth()==CV_16U){double mn,mx;cv::minMaxLoc(i,&mn,&mx);i.convertTo(u8,CV_8U,255.0/std::max(1.0,mx-mn),-mn*255.0/std::max(1.0,mx-mn));}else i.convertTo(u8,CV_8U);cv::cvtColor(u8,rgb,cv::COLOR_GRAY2RGB);}else cv::cvtColor(i,rgb,cv::COLOR_BGR2RGB);return QImage(rgb.data,rgb.cols,rgb.rows,int(rgb.step),QImage::Format_RGB888).copy();}
} // namespace oas
