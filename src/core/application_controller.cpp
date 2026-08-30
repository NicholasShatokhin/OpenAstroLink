#include "core/application_controller.h"
#include "core/equatorial_frames.h"
#include "algorithms/pattern_plate_solver.h"
#include "algorithms/astap_solver.h"
#include "algorithms/neural_solver.h"
#include "algorithms/star_catalog.h"
#include "backends/alpaca_devices.h"
#include "backends/ascom_classic_mount.h"
#include "backends/synscan_network_mount.h"
#include "backends/synscan_app_mount.h"
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
#include <QElapsedTimer>
#include <QDir>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>
#include <QThread>
#include <QSerialPortInfo>
#include <QMetaObject>
#include <QPointer>
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
QJsonObject solveQualityJson(const SolveFrameQuality &q){
    return {{"detectedStars",q.detectedStars},{"background",q.background},{"noiseSigma",q.noiseSigma},
            {"p99",q.p99},{"saturationFraction",q.saturationFraction},{"medianHfrPx",q.medianHfrPx}};
}
// OpenCV names Bayer conversion constants from the opposite corner relative
// to the conventional 2x2 CFA tile label when producing BGR. The mapping
// below is verified with synthetic RGGB/BGGR/GRBG/GBRG mosaics.
int openCvBayerCode(BayerPattern pattern){
    switch(pattern){
    case BayerPattern::RGGB:return cv::COLOR_BayerBG2BGR;
    case BayerPattern::BGGR:return cv::COLOR_BayerRG2BGR;
    case BayerPattern::GRBG:return cv::COLOR_BayerGB2BGR;
    case BayerPattern::GBRG:return cv::COLOR_BayerGR2BGR;
    default:return -1;
    }
}
BayerPattern resolvedBayerPattern(const CameraFrame &frame,const LiveViewRequest &request){
    if(request.bayerPattern!=BayerPattern::Auto)return request.bayerPattern;
    return bayerPatternFromString(frame.bayerPattern,BayerPattern::Auto);
}
bool processLivePreview(CameraFrame &frame,const LiveViewRequest &request,QString *note=nullptr){
    if(!request.debayer||frame.image.empty())return true;
    if(frame.image.channels()==3){if(note)*note="camera already supplies RGB";return true;}
    if(frame.image.channels()!=1){if(note)*note="debayer skipped: preview is not a one-channel CFA frame";return true;}
    const BayerPattern pattern=resolvedBayerPattern(frame,request);const int code=openCvBayerCode(pattern);
    if(code<0){if(note)*note="debayer requested but CFA pattern is unknown; choose RGGB/BGGR/GRBG/GBRG manually";return true;}
    try{cv::Mat color;cv::cvtColor(frame.image,color,code);frame.image=std::move(color);if(note)*note=QString("preview debayer %1").arg(bayerPatternName(pattern));return true;}
    catch(const cv::Exception &e){if(note)*note=QString("debayer failed: %1").arg(e.what());return false;}
}

CameraFrame softwareBinForSolver(CameraFrame frame,int requestedBinX,int requestedBinY){
    if(frame.image.empty())return frame;
    const int actualX=std::max(1,frame.binX),actualY=std::max(1,frame.binY);
    const int targetX=std::max(actualX,std::max(1,requestedBinX));
    const int targetY=std::max(actualY,std::max(1,requestedBinY));
    if(targetX==actualX&&targetY==actualY)return frame;
    const double sx=double(actualX)/double(targetX),sy=double(actualY)/double(targetY);
    const int w=std::max(1,int(std::lround(frame.image.cols*sx)));
    const int h=std::max(1,int(std::lround(frame.image.rows*sy)));
    cv::Mat reduced;cv::resize(frame.image,reduced,cv::Size(w,h),0.0,0.0,cv::INTER_AREA);
    frame.image=std::move(reduced);frame.binX=targetX;frame.binY=targetY;
    return frame;
}
QJsonObject nativeDescriptor(const std::shared_ptr<OalDriverPluginLoader> &loader,const QString &backend,const QString &type,QString *error){
    QString driverId,deviceId;if(!parseNativeBackendKey(backend,driverId,deviceId)){if(error)*error="Invalid native OAL backend key";return{};}
    if(!loader){if(error)*error="Native OAL driver registry unavailable";return{};}
    for(const auto &v:loader->devices()){const auto o=v.toObject();if(o.value("driverId").toString()==driverId&&o.value("id").toString()==deviceId&&o.value("type").toString()==type)return o;}
    if(error)*error=QString("Native OAL %1 device not found: %2/%3").arg(type,driverId,deviceId);return{};
}
QStringList nativeBackendsFor(const std::shared_ptr<OalDriverPluginLoader> &loader,const QString &type){
    QStringList out;if(!loader)return out;for(const auto &v:loader->devices()){const auto o=v.toObject();if(o.value("type").toString()!=type)continue;out<<nativeBackendKey(o.value("driverId").toString(),o.value("id").toString());}out.removeDuplicates();return out;
}

QString inferPersistedSerialPort(const DeviceBinding &binding,const QString &expectedDriverId){
    QString driverId,deviceId;
    if(!binding.backend.startsWith("native:")||!parseNativeBackendKey(binding.backend,driverId,deviceId)||driverId!=expectedDriverId)return {};
    QString token;
    if(driverId=="oal.gemini"&&deviceId.startsWith("gemini-eaf:")) token=deviceId.mid(QStringLiteral("gemini-eaf:").size());
    else if(driverId=="oal.skywatcher"&&deviceId.startsWith("skywatcher-synscan:")) token=deviceId.mid(QStringLiteral("skywatcher-synscan:").size());
    else if(driverId=="oal.eqdrive"&&deviceId.startsWith("eqdrive:")) token=deviceId.mid(QStringLiteral("eqdrive:").size());
    else return {};
    token=token.trimmed();if(token.isEmpty())return {};
    for(const auto &info:QSerialPortInfo::availablePorts()){
        if(info.portName().compare(token,Qt::CaseInsensitive)==0||
           info.systemLocation().compare(token,Qt::CaseInsensitive)==0||
           (!info.serialNumber().trimmed().isEmpty()&&info.serialNumber().trimmed()==token))
            return info.portName();
    }
    // If the old COM/tty name no longer exists, do not pin reconnect to a
    // stale port. Returning empty lets the driver perform normal discovery and
    // the reconnect path can migrate a unique replacement (COM4 -> COM6 is a
    // common Windows USB-UART case).
    return {};
}

class ScopedEnvOverride {
public:
    ScopedEnvOverride(const char *name,const QString &value):name_(name),old_(qgetenv(name)),hadOld_(qEnvironmentVariableIsSet(name)){
        if(!value.isEmpty())qputenv(name_,value.toUtf8());
    }
    ~ScopedEnvOverride(){if(hadOld_)qputenv(name_,old_);else qunsetenv(name_);}
private:
    const char *name_{};QByteArray old_;bool hadOld_{false};
};

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
    if(qEnvironmentVariable("OAL_EQDRIVE_PORT").trimmed().isEmpty()){const QString p=settings_.nativeSerialPort("oal.eqdrive");if(!p.isEmpty())qputenv("OAL_EQDRIVE_PORT",p.toUtf8());}
    catalog_=std::make_shared<StarCatalog>();QString err;QString catalogPath=QDir(QCoreApplication::applicationDirPath()).filePath("config/stars_example.csv");if(!catalog_->loadCsv(catalogPath,&err)){catalogPath=QDir::current().filePath("config/stars_example.csv");catalog_->loadCsv(catalogPath,&err);}catalogSolver_=std::make_shared<PatternPlateSolver>(catalog_);astapSolver_=std::make_shared<AstapSolver>();neuralSolver_=std::make_shared<NeuralSolver>();solver_=astapSolver_->available(nullptr)?astapSolver_:catalogSolver_;
    driverLoader_=std::make_shared<OalDriverPluginLoader>();QStringList driverErrors;const int nativeCount=driverLoader_->scanDefaultPaths(&driverErrors,false);
    connect(driverLoader_.get(),&OalDriverPluginLoader::driverLog,this,[this](const QString&driver,int,const QString&message){emit logMessage(QString("[%1] %2").arg(driver,message));});
    connect(driverLoader_.get(),&OalDriverPluginLoader::driverEvent,this,[this](const QString&driver,const QString&device,const QJsonObject&event){
        QJsonObject p=event;p["driverId"]=driver;p["deviceId"]=device;
        if(oalWsServer_)oalWsServer_->broadcast("driverEvent",p);
        // Driver events are a state-change hint. In particular, native focusers
        // emit moveStarted/position events while the GUI may otherwise have no
        // reason to request a fresh state snapshot. Queue a normal state refresh
        // after the driver invocation has released its per-device serial lock.
        const QString type=event.value("type").toString();
        if(type=="device.discoveryHint"){
            // Vendor hot-plug callbacks are edge notifications, not a guarantee
            // that the body is already enumerable. Canon EDSDK in particular can
            // fire CameraAdded while EdsGetCameraList() still returns zero for a
            // few seconds. Do not consume that edge with an immediate one-shot
            // scan: use a bounded, debounced settle/retry sequence instead.
            if(driver=="oal.canon"){
                const quint64 generation=++canonHotplugGeneration_;
                emit logMessage("Native Canon hot-plug hint received; scheduling settled automatic rediscovery");
                scheduleCanonHotplugRediscovery(generation);
            }else{
                emit logMessage(QString("Native hot-plug hint from %1; scheduling automatic rediscovery").arg(driver));
                QTimer::singleShot(250,this,[this,driver](){if(!shuttingDown_)refreshNativeDiscoveryAsync(QStringList{driver});});
            }
        }
        if(type=="device.disconnected"&&!device.isEmpty()){
            const QString key=nativeBackendKey(driver,device);
            QTimer::singleShot(0,this,[this,key,driver,device](){
                if(shuttingDown_)return;
                auto cancelResource=[this](const QString&r){QString owner;if(operations_.isResourceLocked(r,&owner)&&!owner.isEmpty())operations_.cancel(owner,nullptr);};
                if(camera_&&camera_->backendName()==key){cancelResource("camera");camera_.reset();emit logMessage("Main camera physically disconnected: "+driver+"/"+device);}
                if(guideCamera_&&guideCamera_->backendName()==key){cancelResource("camera.guide");guideCamera_.reset();emit logMessage("Guide camera physically disconnected: "+driver+"/"+device);}
                if(mount_&&mount_->backendName()==key){cancelResource("mount");mount_.reset();emit logMessage("Mount physically disconnected: "+driver+"/"+device);}
                if(focuser_&&focuser_->backendName()==key){cancelResource("focuser");focuser_.reset();emit logMessage("Focuser physically disconnected: "+driver+"/"+device);}
                emitState();
                // Refresh only the affected driver after transport loss so the
                // Devices tab drops the stale descriptor while the persisted
                // auto-connect binding remains available for a later reconnect.
                QTimer::singleShot(350,this,[this,driver](){if(!shuttingDown_)refreshNativeDiscoveryAsync(QStringList{driver});});
            });
        }else if(type=="focuser.moveStarted"||type=="focuser.position"||type=="device.connected")
            QTimer::singleShot(0,this,[this](){if(!shuttingDown_)emitState();});
    });
    const auto loadedNativeDrivers=driverLoader_->drivers();
    QTimer::singleShot(0,this,[this,nativeCount,driverErrors,loadedNativeDrivers](){
        QStringList ids;for(const auto&v:loadedNativeDrivers)ids<<v.toObject().value("driverId").toString();
        emit logMessage(QString("Native OAL driver registry: %1 driver(s)%2").arg(nativeCount).arg(ids.isEmpty()?QString():QString(" [%1]").arg(ids.join(", "))));
        for(const auto&e:driverErrors)emit logMessage("Native driver load warning: "+e);
    });
    #ifdef OAS_HAVE_POSITIONING
    if(profile_.observer.latitudeDeg==0.0&&profile_.observer.longitudeDeg==0.0)QTimer::singleShot(0,this,&ApplicationController::requestSystemLocation);
#endif
    connect(&scheduler_,&Scheduler::statusChanged,this,[this](const SessionStatus&s){auto j=sessionJson(s);emit sessionChanged(j);if(oalWsServer_)oalWsServer_->broadcast("sessionUpdate",j);emitState();});
    // Hardware enumeration is owned by the application/node lifecycle (one
    // initial scan after the control plane is online, then explicit user
    // refreshes).  Do not start a second implicit scan from the controller
    // constructor: vendor SDKs such as QHY may be expensive or stateful.
    connect(&operations_,&OperationManager::operationChanged,this,[this](const QJsonObject&o){
        emit operationChanged(o);
        if(oalWsServer_)oalWsServer_->broadcast("operation",o);
        const QString kind=o.value("kind").toString(),state=o.value("state").toString();
        if(kind=="mount.slew"&&(state=="failed"||state=="cancelled")){
            const QString message=state=="cancelled"?"Mount slew cancelled":o.value("problem").toObject().value("message").toString("Mount slew failed");
            emit logMessage(message);
        }
        if(kind=="autofocus.run"&&(state=="succeeded"||state=="failed"||state=="cancelled")){
            auto result=o.value("result").toObject();
            if(result.isEmpty())result={{"success",false},{"message",state=="cancelled"?"Autofocus cancelled":o.value("problem").toObject().value("message").toString("Autofocus failed")}};
            emit autofocusCompleted(result);
            if(oalWsServer_)oalWsServer_->broadcast("autofocusResult",result);
            emit logMessage(result.value("message").toString());
        }
        // Operation progress already has its own WebSocket event. Rebuilding and
        // broadcasting the complete hardware state for every progress tick can
        // starve the HTTP event loop during CPU-heavy adaptive preprocessing.
        // Emit a full snapshot only at operation start and terminal transitions.
        if(state!="running"||o.value("phase").toString()=="starting")emitState();
    });

    // Native vendor/serial APIs do not all provide hot-remove callbacks. Poll
    // ONLY currently connected native devices from a background thread using
    // each driver's lightweight healthJson. Driver call locks serialize health
    // probes with captures/moves, keeping the Qt/HTTP thread non-blocking.
    nativeHealthTimer_=new QTimer(this);nativeHealthTimer_->setInterval(1800);
    connect(nativeHealthTimer_,&QTimer::timeout,this,[this](){
        if(shuttingDown_||nativeHealthThread_||!driverLoader_)return;
        struct Key{QString driver;QString device;QString resource;};QList<Key> keys;
        auto add=[&](const auto &ptr,const QString &resource){
            if(!ptr)return;QString owner;if(operations_.isResourceLocked(resource,&owner))return;
            QString d,id;if(parseNativeBackendKey(ptr->backendName(),d,id))keys.append({d,id,resource});
        };
        // A health probe must never be allowed to slip between frames of a
        // long-lived camera/focuser/mount operation. Vendor SDK calls that are
        // individually valid can still disturb an active acquisition state
        // machine (observed with QHYCCD). Idle-only probing is both safer and
        // sufficient for physical hot-remove detection.
        add(camera_,"camera");add(guideCamera_,"camera.guide");add(mount_,"mount");add(focuser_,"focuser");
        if(keys.isEmpty())return;
        const auto loader=driverLoader_;QPointer<ApplicationController> self(this);
        auto *thread=QThread::create([loader,keys](){for(const auto &k:keys){QString ignored;loader->health(k.driver,k.device,&ignored);}});
        nativeHealthThread_=thread;
        connect(thread,&QThread::finished,this,[this,thread](){if(nativeHealthThread_==thread)nativeHealthThread_=nullptr;thread->deleteLater();});
        thread->start();
    });
    nativeHealthTimer_->start();
}
ApplicationController::~ApplicationController(){shutdown();}
void ApplicationController::shutdown(){
    if(shuttingDown_)return;
    shuttingDown_=true;
    emit logMessage("OpenAstroLink node shutdown: stopping listeners and active work");
    // Stop accepting new work first.  Do not persist these runtime stops: a
    // process shutdown must not silently disable the user's saved server setup.
    if(stellariumServer_)stellariumServer_->stop();
    if(oalWsServer_)oalWsServer_->stop();
    if(oalServer_)oalServer_->stop();
#ifdef OAS_HAVE_POSITIONING
    if(positionSource_)positionSource_->stopUpdates();
#endif
    if(scheduler_.status().active)scheduler_.stop();
    if(guiding_.status().active)guiding_.stop();
    // OperationManager requests cancellation and waits for all worker-pool tasks
    // while the Qt event dispatcher is still alive (aboutToQuit calls us).
    operations_.shutdown();
    disconnectDevices(false);
    // If a background native-discovery pass is still inside a vendor/serial
    // driver, let it finish before unloading plugin code. Discovery now uses
    // bounded short probes, so this should normally be well below a second or
    // two and avoids a use-after-unload race on shutdown.
    if(nativeHealthTimer_)nativeHealthTimer_->stop();
    if(nativeHealthThread_&&nativeHealthThread_->isRunning()){nativeHealthThread_->quit();nativeHealthThread_->wait(6000);}
    if(nativeDiscoveryThread_&&nativeDiscoveryThread_->isRunning()){
        nativeDiscoveryThread_->quit();
        nativeDiscoveryThread_->wait(6000);
    }
    // Destroy native driver-owned worker threads (notably persistent serial
    // sessions) before QCoreApplication tears down QEventDispatcherWin32.
    if(driverLoader_)driverLoader_->clear();
    emit logMessage("OpenAstroLink node shutdown complete");
}
void ApplicationController::setProfile(const TelescopeProfile&p){profile_=p;settings_.saveProfile(p);if(mount_)mount_->configureGeometry(profile_.mount,profile_.observer);emit profileChanged();emit logMessage("Mount geometry/profile updated; native/direct mounts require a fresh Sync after geometry changes");emitState();}
QStringList ApplicationController::cameraBackends()const{QStringList x=nativeBackendsFor(driverLoader_,"camera");x<<"simulated"<<"opencv"<<"oal";
#ifdef OAS_HAVE_GPHOTO2
x<<"canon-gphoto2";
#endif
return x;}
QStringList ApplicationController::mountBackends()const{QStringList x=nativeBackendsFor(driverLoader_,"mount");x<<"simulated"<<"serial-lx200"<<"synscan-app"<<"synscan-wifi";
#ifdef OAS_HAVE_ASCOM_CLASSIC
x<<"ascom-classic";
#endif
x<<"ascom-alpaca"<<"oal";
#ifdef OAS_HAVE_INDI
x<<"indi";
#endif
return x;}
QStringList ApplicationController::focuserBackends()const{QStringList x=nativeBackendsFor(driverLoader_,"focuser");x<<"simulated"<<"ascom-alpaca"<<"oal";
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
    if(!ensureResourcesAvailable({"solver"},e))return false;
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
bool ApplicationController::loadCatalog(const QString&p,QString*e){if(!ensureResourcesAvailable({"solver"},e))return false;bool ok=catalog_->loadCsv(p,e);if(ok)emit logMessage(QString("Loaded %1 catalog stars").arg(catalog_->stars().size()));return ok;}
bool ApplicationController::loadNeuralModel(const QString&p,QString*e){if(!ensureResourcesAvailable({"solver"},e))return false;bool ok=neuralSolver_->loadModel(p,e);if(ok)emit logMessage("Neural model loaded: "+p);return ok;}
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
bool ApplicationController::connectMount(const QString&b,const QString&e,QString*err){if(!ensureResourcesAvailable({"mount"},err))return false;std::shared_ptr<IMount>d;QString driverId,deviceId;if(parseNativeBackendKey(b,driverId,deviceId)){auto desc=nativeDescriptor(driverLoader_,b,"mount",err);if(desc.isEmpty())return false;d=std::make_shared<NativeOalMount>(driverLoader_,desc,profile_.mount,profile_.observer);}
else if(b=="simulated")d=std::make_shared<SimulatedMount>();else if(b=="serial-lx200")d=std::make_shared<SerialLx200Mount>(e);else if(b=="synscan-app")d=std::make_shared<SynScanAppMount>(e);else if(b=="synscan-wifi")d=std::make_shared<SynScanNetworkMount>(e,profile_.mount,profile_.observer);
#ifdef OAS_HAVE_ASCOM_CLASSIC
else if(b=="ascom-classic")d=std::make_shared<AscomClassicMount>(e);
#endif
else if(b=="ascom-alpaca")d=std::make_shared<AlpacaMount>(QUrl(e));else if(b=="oal")d=std::make_shared<OalMountClient>(QUrl(e));
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
bool ApplicationController::restoreConfiguredDevices(QStringList *errors, bool refreshNative){
    bool allOk=true;
    // Persisted native bindings can legitimately be absent from the startup
    // cache when a USB camera is still enumerating or is hot-plugged after the
    // node starts. Refresh the native registry before each reconnect attempt if
    // an auto-connect native device is still missing. Without this, retrying a
    // saved native:oal.qhy/... key only re-queries the stale cache forever.
    const auto needsNativeRefresh=[&](const std::shared_ptr<IDevice>&current,const DeviceBinding&b){
        return !current&&b.autoConnect&&b.backend.startsWith("native:");
    };
    if(refreshNative&&driverLoader_&&(needsNativeRefresh(camera_,settings_.cameraBinding())||
                       needsNativeRefresh(guideCamera_,settings_.guideCameraBinding())||
                       needsNativeRefresh(mount_,settings_.mountBinding())||
                       needsNativeRefresh(focuser_,settings_.focuserBinding()))){
        // Auto mode should still exploit the exact serial identity from a saved
        // native binding.  In the common Gemini case the persisted device id is
        // gemini-eaf:COM4.  Probe that port first for this reconnect cycle, but
        // do not mutate the user's saved "Auto" discovery preference.
        QString geminiHint,skywatcherHint,eqdriveHint;
        if(qEnvironmentVariable("OAL_GEMINI_PORT").trimmed().isEmpty()&&needsNativeRefresh(focuser_,settings_.focuserBinding()))
            geminiHint=inferPersistedSerialPort(settings_.focuserBinding(),"oal.gemini");
        if(qEnvironmentVariable("OAL_SKYWATCHER_PORT").trimmed().isEmpty()&&needsNativeRefresh(mount_,settings_.mountBinding()))
            skywatcherHint=inferPersistedSerialPort(settings_.mountBinding(),"oal.skywatcher");
        if(qEnvironmentVariable("OAL_EQDRIVE_PORT").trimmed().isEmpty()&&needsNativeRefresh(mount_,settings_.mountBinding()))
            eqdriveHint=inferPersistedSerialPort(settings_.mountBinding(),"oal.eqdrive");
        if(!geminiHint.isEmpty())emit logMessage("Focused reconnect discovery: oal.gemini -> "+geminiHint+" (from persisted binding)");
        if(!skywatcherHint.isEmpty())emit logMessage("Focused reconnect discovery: oal.skywatcher -> "+skywatcherHint+" (from persisted binding)");
        if(!eqdriveHint.isEmpty())emit logMessage("Focused reconnect discovery: oal.eqdrive -> "+eqdriveHint+" (from persisted binding)");
        ScopedEnvOverride geminiEnv("OAL_GEMINI_PORT",geminiHint);
        ScopedEnvOverride skywatcherEnv("OAL_SKYWATCHER_PORT",skywatcherHint);
        ScopedEnvOverride eqdriveEnv("OAL_EQDRIVE_PORT",eqdriveHint);

        // Refresh only the driver(s) that back an actually missing persisted
        // device. v0.2.10.16 refreshed every native driver whenever *one*
        // device (often an unplugged Gemini) was absent. That repeatedly called
        // ScanQHYCCD while a QHY camera was already connected and repeatedly
        // tried to open COM5 while EQMOD/Classic ASCOM legitimately owned it.
        QStringList driversToRefresh;
        const auto addNativeDriver=[&](const std::shared_ptr<IDevice>&current,const DeviceBinding&binding){
            if(!needsNativeRefresh(current,binding))return;QString driverId,deviceId;
            if(parseNativeBackendKey(binding.backend,driverId,deviceId)&&!driverId.isEmpty()&&!driversToRefresh.contains(driverId))driversToRefresh<<driverId;
        };
        addNativeDriver(camera_,settings_.cameraBinding());
        addNativeDriver(guideCamera_,settings_.guideCameraBinding());
        addNativeDriver(mount_,settings_.mountBinding());
        addNativeDriver(focuser_,settings_.focuserBinding());
        QStringList discoveryErrors;const auto nativeDevices=driverLoader_->refreshDevices(driversToRefresh,&discoveryErrors);
        QHash<QString,int> counts;for(const auto &v:nativeDevices)counts[v.toObject().value("driverId").toString()]++;
        // A serial override is an explicit identity choice. If the physical
        // device moved from COM4 to COM6, migrate the persisted native binding
        // to the newly discovered device instead of repeatedly reconnecting the
        // stale COM4 device id. This is especially important for Gemini.
        const auto rebind=[&](const QString&driverId,const QString&type,const QString&port,DeviceBinding binding,auto saver){
            if(port.trimmed().isEmpty())return;QString oldDriver,oldDevice;if(!parseNativeBackendKey(binding.backend,oldDriver,oldDevice)||oldDriver!=driverId)return;
            for(const auto&v:nativeDevices){const auto o=v.toObject();if(o.value("driverId").toString()!=driverId||o.value("type").toString()!=type)continue;const QString discoveredPort=o.value("transport").toObject().value("port").toString();if(discoveredPort.compare(port,Qt::CaseInsensitive)!=0)continue;const QString next=nativeBackendKey(driverId,o.value("id").toString());if(next!=binding.backend){emit logMessage(QString("Persisted %1 binding migrated from %2 to %3 after explicit serial selection %4").arg(type,binding.backend,next,port));binding.backend=next;binding.endpoint.clear();(settings_.*saver)(binding);}break;}
        };
        rebind("oal.gemini","focuser",qEnvironmentVariable("OAL_GEMINI_PORT"),settings_.focuserBinding(),&AppSettings::saveFocuserBinding);
        rebind("oal.skywatcher","mount",qEnvironmentVariable("OAL_SKYWATCHER_PORT"),settings_.mountBinding(),&AppSettings::saveMountBinding);
        rebind("oal.eqdrive","mount",qEnvironmentVariable("OAL_EQDRIVE_PORT"),settings_.mountBinding(),&AppSettings::saveMountBinding);
        // Auto mode: if a port-based persisted serial identity disappeared but
        // exactly one replacement device of that same native driver is now
        // visible, migrate it automatically. This fixes stale Gemini COM4
        // bindings when Windows re-enumerates the same focuser as COM6.
        const auto rebindUniqueAuto=[&](const QString&driverId,const QString&type,DeviceBinding binding,auto saver){
            if(!nativeSerialPortOverride(driverId).isEmpty())return;
            QString oldDriver,oldDevice;if(!parseNativeBackendKey(binding.backend,oldDriver,oldDevice)||oldDriver!=driverId)return;
            QJsonObject only;int matches=0;for(const auto&v:nativeDevices){const auto o=v.toObject();if(o.value("driverId").toString()==driverId&&o.value("type").toString()==type){only=o;++matches;}}
            if(matches!=1)return;const QString next=nativeBackendKey(driverId,only.value("id").toString());if(next==binding.backend)return;
            emit logMessage(QString("Persisted %1 binding auto-migrated from %2 to %3 after unique native rediscovery").arg(type,binding.backend,next));binding.backend=next;binding.endpoint.clear();(settings_.*saver)(binding);
        };
        rebindUniqueAuto("oal.gemini","focuser",settings_.focuserBinding(),&AppSettings::saveFocuserBinding);
        rebindUniqueAuto("oal.skywatcher","mount",settings_.mountBinding(),&AppSettings::saveMountBinding);
        rebindUniqueAuto("oal.eqdrive","mount",settings_.mountBinding(),&AppSettings::saveMountBinding);
        emit logMessage(QString("Native OAL reconnect discovery refreshed [%1]: %2 cached device(s); qhy=%3 gemini=%4 skywatcher=%5 eqdrive=%6")
                            .arg(driversToRefresh.join(",")).arg(nativeDevices.size()).arg(counts.value("oal.qhy")).arg(counts.value("oal.gemini")).arg(counts.value("oal.skywatcher")).arg(counts.value("oal.eqdrive")));
        for(const auto&e:discoveryErrors){emit logMessage("Native reconnect discovery warning: "+e);if(errors)errors->append("native discovery: "+e);}
    }
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

QStringList ApplicationController::missingAutoConnectNativeDrivers() const {
    QStringList out;
    const auto add=[&](const std::shared_ptr<IDevice>&current,const DeviceBinding&binding){
        if(current||!binding.autoConnect||!binding.backend.startsWith("native:"))return;
        QString driverId,deviceId;
        if(parseNativeBackendKey(binding.backend,driverId,deviceId)&&!driverId.isEmpty()&&!out.contains(driverId))
            out<<driverId;
    };
    add(camera_,settings_.cameraBinding());
    add(guideCamera_,settings_.guideCameraBinding());
    add(mount_,settings_.mountBinding());
    add(focuser_,settings_.focuserBinding());
    return out;
}

bool ApplicationController::nativeDiscoveryRunning() const {
    return nativeDiscoveryThread_ && nativeDiscoveryThread_->isRunning();
}

bool ApplicationController::nativeDriverHasCachedDevice(const QString &driverId) const {
    if(!driverLoader_)return false;
    for(const auto &v:driverLoader_->devices())
        if(v.toObject().value("driverId").toString()==driverId)return true;
    return false;
}

void ApplicationController::scheduleCanonHotplugRediscovery(quint64 generation) {
    // EDSDK CameraAdded is observed before some EOS bodies become visible to
    // EdsGetCameraList().  Keep the node responsive and retry only oal.canon at
    // increasing settle delays.  A newer CameraAdded edge supersedes this
    // sequence.  The final pass may hard-reload only the idle Canon driver,
    // which reproduces the successful manual Refresh recovery without touching
    // QHY or serial drivers.
    static constexpr int delaysMs[]={700,2200,4500,8000};
    for(int i=0;i<4;++i){
        QTimer::singleShot(delaysMs[i],this,[this,generation,i](){
            if(shuttingDown_||generation!=canonHotplugGeneration_)return;
            if(nativeDriverHasCachedDevice("oal.canon")){
                if(i>0)emit logMessage("Canon hot-plug rediscovery completed; cancelling remaining retries");
                return;
            }
            const bool hardRecovery=(i==3);
            emit logMessage(QString("Canon hot-plug rediscovery attempt %1/4%2")
                                .arg(i+1)
                                .arg(hardRecovery?" (final EDSDK reload fallback)":""));
            refreshNativeDiscoveryAsync(QStringList{"oal.canon"},hardRecovery);
        });
    }
}

void ApplicationController::refreshNativeDiscoveryAsync(const QStringList &driverIds, bool hardVendorRecovery) {
    if(shuttingDown_||!driverLoader_)return;
    if(nativeDiscoveryRunning()){
        // Never lose an explicit discovery request just because a slow vendor/
        // serial scan is already running. This is especially important when the
        // user changes Gemini from Auto to COM6 while the initial all-driver
        // startup scan is still in flight. A focused hard-recovery request must
        // preserve BOTH its driver scope and its hard-recovery flag.
        if(driverIds.isEmpty())pendingNativeDiscoveryAll_=true;
        else for(const auto&id:driverIds)if(!pendingNativeDiscoveryDrivers_.contains(id))pendingNativeDiscoveryDrivers_<<id;
        if(hardVendorRecovery)pendingNativeDiscoveryHardRecovery_=true;
        return;
    }
    const auto loader=driverLoader_;
    const QStringList ids=driverIds;
    const bool qhyInUse=(camera_&&camera_->backendName().startsWith("native:oal.qhy/"))||(guideCamera_&&guideCamera_->backendName().startsWith("native:oal.qhy/"));
    const bool canonInUse=(camera_&&camera_->backendName().startsWith("native:oal.canon/"))||(guideCamera_&&guideCamera_->backendName().startsWith("native:oal.canon/"));
    const bool hardRecovery=hardVendorRecovery;
    QPointer<ApplicationController> self(this);
    auto *thread=QThread::create([loader,ids,self,qhyInUse,canonInUse,hardRecovery](){
        QStringList errors;
        auto devices=loader->refreshDevices(ids,&errors);
        // A hard recovery is still bounded to an idle vendor driver. Explicit
        // all-driver Refresh keeps its previous QHY+Canon behaviour; a focused
        // hot-plug fallback can now hard-reload only the requested Canon driver.
        if(hardRecovery){
            const auto hasDriver=[&](const QString&driverId){for(const auto&v:loader->drivers())if(v.toObject().value("driverId").toString()==driverId)return true;return false;};
            const auto hasDevice=[&](const QString&driverId){for(const auto&v:devices)if(v.toObject().value("driverId").toString()==driverId)return true;return false;};
            const auto requested=[&](const QString&driverId){return ids.isEmpty()||ids.contains(driverId);};
            if(requested("oal.qhy")&&!qhyInUse&&hasDriver("oal.qhy")&&!hasDevice("oal.qhy")){
                QString restartError;if(loader->restartDriver("oal.qhy",&restartError)){
                    if(self)QMetaObject::invokeMethod(self,[self,ids](){if(self)emit self->logMessage(ids.isEmpty()?"Explicit Refresh: hard-reloaded native QHY driver DLL/SDK after zero-device scan":"Focused recovery: hard-reloaded native QHY driver DLL/SDK after zero-device scan");},Qt::QueuedConnection);
                    devices=loader->refreshDevices(QStringList{"oal.qhy"},&errors);
                }else if(!restartError.isEmpty())errors<<restartError;
            }
            if(requested("oal.canon")&&!canonInUse&&hasDriver("oal.canon")&&!hasDevice("oal.canon")){
                // Canon EDSDK has a thread-affinity requirement that ordinary
                // vendor SDKs such as QHYCCD do not. In the normal startup path
                // the OAL Canon driver is started on ApplicationController's Qt
                // event-loop thread, and EOS object-transfer callbacks work.
                // v0.2.10.28 restarted the Canon DLL here on this short-lived
                // discovery QThread. EdsInitializeSDK() therefore became owned
                // by a worker that exited immediately after discovery: camera
                // commands (ISO/shutter) still succeeded, but
                // kEdsObjectEvent_DirItemRequestTransfer was never delivered.
                // Marshal ONLY the Canon stop/unload/load/start boundary back to
                // the long-lived application event-loop thread. The potentially
                // slow enumeration remains on this worker afterwards.
                QString restartError;
                bool restarted=false;
                if(self){
                    QMetaObject::invokeMethod(self,[self,loader,&restartError,&restarted](){
                        if(!self||self->shuttingDown_){restartError="Canon EDSDK restart cancelled during shutdown";return;}
                        restarted=loader->restartDriver("oal.canon",&restartError);
                    },Qt::BlockingQueuedConnection);
                }else restartError="Canon EDSDK restart cancelled because application controller is unavailable";
                if(restarted){
                    if(self)QMetaObject::invokeMethod(self,[self,ids](){if(self)emit self->logMessage(ids.isEmpty()?"Explicit Refresh: hard-reloaded native Canon EDSDK driver on application event-loop thread after zero-device scan":"Canon hot-plug fallback: hard-reloaded native Canon EDSDK driver on application event-loop thread after zero-device scan");},Qt::QueuedConnection);
                    devices=loader->refreshDevices(QStringList{"oal.canon"},&errors);
                }else if(!restartError.isEmpty())errors<<restartError;
            }
        }
        if(!self)return;
        QMetaObject::invokeMethod(self,[self,ids,errors,devices](){
            if(!self||self->shuttingDown_)return;
            for(const auto&e:errors)emit self->logMessage("Native async discovery warning: "+e);

            // Apply an explicit serial-port selection (or a unique Auto-mode
            // replacement) to a stale persisted native binding after the new
            // catalogue has been populated. The old implementation did this
            // only in the synchronous reconnect path, so an asynchronous GUI
            // COM change could discover the device yet leave the saved binding
            // pointing at the previous COM number.
            const auto migrate=[&](const QString&driverId,const QString&type,DeviceBinding binding,auto saver){
                if(!ids.isEmpty()&&!ids.contains(driverId))return;
                QString oldDriver,oldDevice;
                if(!parseNativeBackendKey(binding.backend,oldDriver,oldDevice)||oldDriver!=driverId)return;
                QJsonObject selected;
                const QString port=self->nativeSerialPortOverride(driverId);
                int matches=0;
                for(const auto&v:devices){
                    const auto o=v.toObject();
                    if(o.value("driverId").toString()!=driverId||o.value("type").toString()!=type)continue;
                    if(!port.isEmpty()){
                        if(o.value("transport").toObject().value("port").toString().compare(port,Qt::CaseInsensitive)==0){selected=o;matches=1;break;}
                    }else{selected=o;++matches;}
                }
                if(port.isEmpty()&&matches!=1)return;
                if(selected.isEmpty())return;
                const QString next=nativeBackendKey(driverId,selected.value("id").toString());
                if(next==binding.backend)return;
                emit self->logMessage(QString("Persisted %1 binding migrated from %2 to %3 after asynchronous native discovery")
                                      .arg(type,binding.backend,next));
                binding.backend=next;binding.endpoint.clear();(self->settings_.*saver)(binding);
            };
            migrate("oal.gemini","focuser",self->settings_.focuserBinding(),&AppSettings::saveFocuserBinding);
            migrate("oal.skywatcher","mount",self->settings_.mountBinding(),&AppSettings::saveMountBinding);
            migrate("oal.eqdrive","mount",self->settings_.mountBinding(),&AppSettings::saveMountBinding);

            emit self->logMessage(QString("Native OAL async discovery refreshed%1: %2 cached device(s)")
                                  .arg(ids.isEmpty()?QString():QString(" [%1]").arg(ids.join(",")))
                                  .arg(devices.size()));
            self->emitState();
            emit self->nativeDiscoveryCompleted(ids);
        },Qt::QueuedConnection);
    });
    nativeDiscoveryThread_=thread;
    connect(thread,&QThread::finished,this,[this,thread](){
        if(nativeDiscoveryThread_==thread)nativeDiscoveryThread_=nullptr;
        thread->deleteLater();
        if(shuttingDown_)return;
        const bool all=pendingNativeDiscoveryAll_;
        const bool hard=pendingNativeDiscoveryHardRecovery_;
        const QStringList pending=pendingNativeDiscoveryDrivers_;
        pendingNativeDiscoveryAll_=false;pendingNativeDiscoveryHardRecovery_=false;pendingNativeDiscoveryDrivers_.clear();
        if(all||!pending.isEmpty())
            QTimer::singleShot(0,this,[this,all,pending,hard](){refreshNativeDiscoveryAsync(all?QStringList{}:pending,hard);});
    });
    thread->start();
}
void ApplicationController::commitCapturedFrame(const CameraFrame&f,bool emitFullState,bool verbose){
    previousFrame_=lastFrame_;lastFrame_=f;previewFrameCache_.push_back(f);while(previewFrameCache_.size()>8)previewFrameCache_.pop_front();emit frameCaptured(toQImage(f.image),f.id);
    if(verbose){
        QString stats;
        if(!f.image.empty()&&f.image.channels()==1){double mn=0,mx=0;cv::minMaxLoc(f.image,&mn,&mx);const auto mean=cv::mean(f.image);stats=QString(" min=%1 max=%2 mean=%3").arg(mn,0,'f',1).arg(mx,0,'f',1).arg(mean[0],0,'f',1);}
        emit logMessage(QString("Captured %1 × %2 frame %3 exp=%4s gain=%5%6%7").arg(f.image.cols).arg(f.image.rows).arg(f.id).arg(f.exposureSec,0,'g',6).arg(f.gain).arg(stats).arg(f.scienceFilePath.isEmpty()?QString():QString(" science=%1").arg(f.scienceFilePath)));
    }
    if(oalWsServer_)oalWsServer_->broadcast("frameReady",QJsonObject{{"frameId",f.id},{"capturedUtc",f.capturedUtc.toString(Qt::ISODateWithMs)},{"width",f.image.cols},{"height",f.image.rows},{"exposureSec",f.exposureSec},{"gain",f.gain},{"live",f.id.startsWith("live-")}});
    if(emitFullState)emitState();
}
void ApplicationController::publishOperationalPreview(const CameraFrame&frame,const QString&purpose){
    CameraFrame f=frame;f.scienceFilePath.clear();f.id=QString("%1-preview-%2").arg(purpose,f.id);
    previewFrameCache_.push_back(f);while(previewFrameCache_.size()>8)previewFrameCache_.pop_front();emit frameCaptured(toQImage(f.image),f.id);
    if(oalWsServer_)oalWsServer_->broadcast("frameReady",QJsonObject{{"frameId",f.id},{"capturedUtc",f.capturedUtc.toString(Qt::ISODateWithMs)},{"width",f.image.cols},{"height",f.image.rows},{"exposureSec",f.exposureSec},{"gain",f.gain},{"operationalPreview",true},{"purpose",purpose}});
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
        QJsonObject result{{"frameId",frame.id},{"capturedUtc",frame.capturedUtc.toString(Qt::ISODateWithMs)},{"width",frame.image.cols},{"height",frame.image.rows},{"exposureSec",frame.exposureSec},{"gain",frame.gain},{"binX",frame.binX},{"binY",frame.binY},{"previewResource",QString("/api/v1/frames/%1/preview").arg(frame.id)}};
        if(!frame.scienceFilePath.isEmpty())result.insert("scienceFilePath",frame.scienceFilePath);
        out.success=true;out.result=result;ctx.reportProgress(1.0,"completed");return out;
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
QString ApplicationController::startLiveView(const LiveViewRequest&request,QString*error){
    if(!camera_){if(error)*error="No camera connected";return{};}
    // Do not use repeated still captures as a fake live view on a DSLR: that
    // would actuate the shutter continuously. Canon gets a dedicated EVF path
    // in a later driver revision.
    if(camera_->backendName().startsWith("native:oal.canon/")){if(error)*error="Canon live view requires the EDSDK EVF transport; repeated still captures are intentionally disabled to protect the shutter. Use QHY/ASI for finder alignment in this release.";return{};}
    LiveViewRequest r=request;r.exposureSec=std::clamp(r.exposureSec,0.0001,10.0);r.gain=std::max(0,r.gain);r.binX=std::clamp(r.binX,1,4);r.binY=std::clamp(r.binY,1,4);r.targetFps=std::clamp(r.targetFps,0.2,10.0);
    // CFA pixels must remain on their native 1x1 lattice for software debayer.
    // Drivers that already return RGB are harmlessly passed through, but forcing
    // 1x1 here keeps raw QHY/ZWO previews color-correct across vendors.
    if(r.debayer&&(r.binX!=1||r.binY!=1)){r.binX=r.binY=1;emit logMessage("Live View debayer enabled: forcing 1x1 readout so the Bayer mosaic remains valid");}
    auto cam=camera_;
    const QString id=operations_.submit("camera.live-view",{"camera"},true,[this,cam,r](OperationContext&ctx){
        OperationOutcome out;int frames=0;QElapsedTimer elapsed;elapsed.start();
        const qint64 targetPeriodMs=qint64(std::lround(1000.0/r.targetFps));
        // QHY has a real SDK streaming mode. Use it instead of repeated
        // ExpQHYCCDSingleFrame/GetQHYCCDSingleFrame calls; HIL showed that
        // short repeated single-frame cycles can wedge the QHY5III462C and
        // poison the next normal exposure. The driver restores single-frame
        // mode on stop.
        auto native=std::dynamic_pointer_cast<NativeOalCamera>(cam);
        const bool nativeStream=native&&native->nativeLiveSupported();
        if(nativeStream){
            QString startError;if(!native->startNativeLive(r,&startError)){out.problem={{"code","LIVE_VIEW_START_FAILED"},{"message",startError}};return out;}
            const auto stopStream=[&](){QString stopError;if(!native->stopNativeLive(&stopError)&&!stopError.isEmpty())QMetaObject::invokeMethod(this,[this,stopError](){emit logMessage("Live View cleanup warning: "+stopError);},Qt::QueuedConnection);};
            while(!ctx.isCancellationRequested()){
                QElapsedTimer cycle;cycle.start();CameraFrame frame;QString err;
                const int timeoutMs=int(std::clamp<qint64>(qint64(std::ceil(r.exposureSec*3000.0))+1000,1000,5000));
                if(!native->nextNativeLiveFrame(frame,timeoutMs,&err)){if(ctx.isCancellationRequested()){stopStream();out.cancelled=true;return out;}stopStream();out.problem={{"code","LIVE_VIEW_FRAME_FAILED"},{"message",err}};return out;}
                if(ctx.isCancellationRequested()){stopStream();out.cancelled=true;return out;}
                QString previewNote;if(!processLivePreview(frame,r,&previewNote)){stopStream();out.problem={{"code","LIVE_VIEW_DEBAYER_FAILED"},{"message",previewNote}};return out;}
                frame.id="live-"+frame.id;frame.scienceFilePath.clear();++frames;
                QMetaObject::invokeMethod(this,[this,frame](){commitCapturedFrame(frame,false,false);},Qt::BlockingQueuedConnection);
                const double actualFps=elapsed.elapsed()>0?1000.0*double(frames)/double(elapsed.elapsed()):0.0;
                ctx.reportProgress(0.0,"live",{{"frames",frames},{"actualFps",actualFps},{"targetFps",r.targetFps},{"frameId",frame.id},{"transport","native-stream"}});
                qint64 sleepMs=targetPeriodMs-cycle.elapsed();while(sleepMs>0&&!ctx.isCancellationRequested()){const int slice=int(std::min<qint64>(sleepMs,25));QThread::msleep(slice);sleepMs-=slice;}
            }
            stopStream();out.cancelled=true;return out;
        }
        ThreadMarshalledCamera proxy(this,cam);
        while(!ctx.isCancellationRequested()){
            QElapsedTimer cycle;cycle.start();ExposureRequest e;e.exposureSec=r.exposureSec;e.gain=r.gain;e.binX=r.binX;e.binY=r.binY;e.saveRaw=false;CameraFrame frame;QString err;
            if(!proxy.capture(e,frame,&err)){if(ctx.isCancellationRequested()){out.cancelled=true;return out;}out.problem={{"code","LIVE_VIEW_CAPTURE_FAILED"},{"message",err}};return out;}
            if(ctx.isCancellationRequested()){out.cancelled=true;return out;}
            QString previewNote;if(!processLivePreview(frame,r,&previewNote)){out.problem={{"code","LIVE_VIEW_DEBAYER_FAILED"},{"message",previewNote}};return out;}
            frame.id="live-"+frame.id;frame.scienceFilePath.clear();++frames;
            QMetaObject::invokeMethod(this,[this,frame](){commitCapturedFrame(frame,false,false);},Qt::BlockingQueuedConnection);
            const double actualFps=elapsed.elapsed()>0?1000.0*double(frames)/double(elapsed.elapsed()):0.0;
            ctx.reportProgress(0.0,"live",{{"frames",frames},{"actualFps",actualFps},{"targetFps",r.targetFps},{"frameId",frame.id},{"transport","repeated-capture"}});
            qint64 sleepMs=targetPeriodMs-cycle.elapsed();while(sleepMs>0&&!ctx.isCancellationRequested()){const int slice=int(std::min<qint64>(sleepMs,25));QThread::msleep(slice);sleepMs-=slice;}
        }
        out.cancelled=true;return out;
    });
    emit logMessage(QString("Live View operation accepted: %1 — %2 s, gain %3, target %4 fps, debayer=%5 %6").arg(id).arg(r.exposureSec,0,'g',5).arg(r.gain).arg(r.targetFps,0,'g',3).arg(r.debayer?"ON":"OFF").arg(bayerPatternName(r.bayerPattern)));return id;
}
SolveResult ApplicationController::solveLast(const SolveHint&h){if(lastFrame_.image.empty()){lastSolve_.message="No captured frame";lastSolve_.success=false;}else lastSolve_=solver_->solve(lastFrame_,profile_,h);auto j=solveToJson(lastSolve_);QJsonArray stars;for(const auto&s:lastSolve_.imageStars)stars.append(QJsonObject{{"x",s.positionPx.x},{"y",s.positionPx.y},{"flux",s.flux},{"peak",s.peak},{"hfrPx",s.hfrPx}});j["imageStars"]=stars;emit solveCompleted(j);if(oalWsServer_)oalWsServer_->broadcast("solveResult",j);emit logMessage(lastSolve_.message);emitState();return lastSolve_;}
QString ApplicationController::startAdaptiveSolve(const AdaptiveSolveRequest&request,QString*error){
    if(!camera_){if(error)*error="No camera connected";return{};}
    if(!solver_){if(error)*error="No plate solver selected";return{};}
    AdaptiveSolveRequest r=request;
    r.exposure.exposureSec=std::clamp(r.exposure.exposureSec,0.001,std::max(0.001,r.maxSingleExposureSec));
    r.exposure.binX=std::max(1,r.exposure.binX);r.exposure.binY=std::max(1,r.exposure.binY);
    r.maxAttempts=std::clamp(r.maxAttempts,1,6);r.stackFrames=std::clamp(r.stackFrames,2,9);r.finalStackFrames=std::clamp(r.finalStackFrames,r.stackFrames,15);
    r.minStarsForSolve=std::clamp(r.minStarsForSolve,4,200);r.exposureGrowth=std::clamp(r.exposureGrowth,1.0,3.0);r.maxSingleExposureSec=std::max(r.exposure.exposureSec,r.maxSingleExposureSec);r.maxCapturePhaseSec=std::clamp(r.maxCapturePhaseSec,15.0,600.0);
    if(r.useMountHint&&mount_&&(!r.hint.raDeg||!r.hint.decDeg)){MountStatus ms;if(mount_->status(ms,nullptr)&&ms.coordinateValid){r.hint.raDeg=ms.coordinate.raDeg;r.hint.decDeg=ms.coordinate.decDeg;emit logMessage(QString("Adaptive solve using mount hint RA=%1 DEC=%2").arg(ms.coordinate.raDeg,0,'f',6).arg(ms.coordinate.decDeg,0,'f',6));}else emit logMessage("Adaptive solve: mount coordinate is not valid yet; continuing without a mount hint");}
    auto cam=camera_;auto solver=solver_;const TelescopeProfile profile=profile_;
    const QString id=operations_.submit("solver.adaptive",{"camera","solver"},true,[this,cam,solver,profile,r](OperationContext&ctx){
        OperationOutcome out;ThreadMarshalledCamera cameraProxy(this,cam);QJsonArray attempts;AdaptivePreparedFrame lastPrepared;SolveResult lastResult;
        const int totalFramesBudget=1+std::max(0,r.maxAttempts-2)*r.stackFrames+(r.maxAttempts>1?r.finalStackFrames:0);int capturedTotal=0;
        QElapsedTimer capturePhaseTimer;capturePhaseTimer.start();const bool canonDslr=cam->backendName().startsWith("native:oal.canon/");double adaptiveExposureSec=r.exposure.exposureSec;
        for(int attempt=0;attempt<r.maxAttempts;++attempt){
            if(ctx.isCancellationRequested()){out.cancelled=true;return out;}
            const int frameCount=attempt==0?1:(attempt==r.maxAttempts-1?r.finalStackFrames:r.stackFrames);
            ExposureRequest exposure=r.exposure;
            exposure.exposureSec=std::clamp(adaptiveExposureSec,0.001,r.maxSingleExposureSec);
            std::vector<CameraFrame> frames;frames.reserve(frameCount);QString captureError;
            for(int i=0;i<frameCount;++i){
                if(ctx.isCancellationRequested()){out.cancelled=true;return out;}
                const double progress=0.04+0.66*double(capturedTotal)/std::max(1,totalFramesBudget);
                ctx.reportProgress(progress,"solve.capture",{{"attempt",attempt+1},{"attempts",r.maxAttempts},{"frame",i+1},{"frames",frameCount},{"exposureSec",exposure.exposureSec},{"gain",exposure.gain},{"binX",exposure.binX},{"binY",exposure.binY}});
                if(capturePhaseTimer.elapsed()>qint64(r.maxCapturePhaseSec*1000.0)){out.problem={{"code","ADAPTIVE_SOLVE_CAPTURE_BUDGET_EXCEEDED"},{"message",QString("Adaptive capture phase exceeded %1 s wall-clock budget").arg(r.maxCapturePhaseSec,0,'f',1)},{"attempt",attempt+1},{"capturedFrames",capturedTotal}};return out;}
                CameraFrame f;if(!cameraProxy.capture(exposure,f,&captureError)){if(ctx.isCancellationRequested()){out.cancelled=true;return out;}out.problem={{"code","ADAPTIVE_SOLVE_CAPTURE_FAILED"},{"message",captureError},{"attempt",attempt+1}};return out;}
                const int sourceW=f.image.cols,sourceH=f.image.rows,sourceBinX=std::max(1,f.binX),sourceBinY=std::max(1,f.binY);
                f=softwareBinForSolver(std::move(f),exposure.binX,exposure.binY);
                if((f.image.cols!=sourceW||f.image.rows!=sourceH)&&(capturedTotal==0||i==0))QMetaObject::invokeMethod(this,[this,sourceW,sourceH,w=f.image.cols,h=f.image.rows,sourceBinX,sourceBinY,bx=f.binX,by=f.binY](){emit logMessage(QString("Adaptive solver software bin: %1x%2 bin %3x%4 -> %5x%6 effective bin %7x%8").arg(sourceW).arg(sourceH).arg(sourceBinX).arg(sourceBinY).arg(w).arg(h).arg(bx).arg(by));},Qt::QueuedConnection);
                frames.push_back(std::move(f));++capturedTotal;
                if(canonDslr&&i+1<frameCount){for(int settle=0;settle<10&&!ctx.isCancellationRequested();++settle)QThread::msleep(25);}
            }
            QString prepWarning;lastPrepared=adaptiveSolvePreprocessor_.prepare(frames,r.registerFrames,r.equalizeBackground,&prepWarning);
            if(lastPrepared.frame.image.empty()){out.problem={{"code","ADAPTIVE_SOLVE_PREPROCESS_FAILED"},{"message",prepWarning.isEmpty()?"Could not prepare solver frame":prepWarning}};return out;}
            QJsonObject diag{{"attempt",attempt+1},{"capturedFrames",frameCount},{"registeredFrames",lastPrepared.registeredFrames},{"singleExposureSec",exposure.exposureSec},{"effectiveExposureSec",lastPrepared.frame.exposureSec},{"quality",solveQualityJson(lastPrepared.quality)}};if(!prepWarning.isEmpty())diag["warning"]=prepWarning;
            const bool finalAttempt=attempt==r.maxAttempts-1;const bool enoughStars=lastPrepared.quality.detectedStars>=r.minStarsForSolve;
            if(!finalAttempt){
                double multiplier=r.exposureGrowth;QString exposureDecision="increase exposure for more stellar signal";
                if(lastPrepared.quality.saturationFraction>0.01||lastPrepared.quality.background>0.45){multiplier=0.65;exposureDecision="reduce exposure: bright/clipped background";}
                else if(lastPrepared.quality.background>=0.12&&lastPrepared.quality.p99>=0.55){multiplier=std::min(1.15,r.exposureGrowth);exposureDecision="small exposure increase: background already adequate";}
                adaptiveExposureSec=std::clamp(exposure.exposureSec*multiplier,0.001,r.maxSingleExposureSec);diag["nextExposureSec"]=adaptiveExposureSec;diag["exposureDecision"]=exposureDecision;
            }
            if(!enoughStars&&!finalAttempt){diag["solverInvoked"]=false;diag["decision"]=QString("Only %1 stars detected; collect more short frames").arg(lastPrepared.quality.detectedStars);attempts.append(diag);ctx.reportProgress(0.72,"solve.quality",diag);continue;}
            SolveHint hint=r.hint;if(hint.raDeg&&hint.decDeg){const double maxRadius=std::clamp(r.hint.searchRadiusDeg,0.1,180.0);hint.searchRadiusDeg=std::min(maxRadius,std::min(180.0,5.0*std::pow(2.0,attempt)));}
            ctx.reportProgress(0.76+0.18*double(attempt)/std::max(1,r.maxAttempts),"solve.astap",{{"attempt",attempt+1},{"detectedStars",lastPrepared.quality.detectedStars},{"searchRadiusDeg",hint.searchRadiusDeg}});
            lastResult=solver->solve(lastPrepared.frame,profile,hint);diag["solverInvoked"]=true;diag["solveSuccess"]=lastResult.success;diag["solverMessage"]=lastResult.message;diag["searchRadiusDeg"]=hint.searchRadiusDeg;attempts.append(diag);
            if(lastResult.success){
                QMetaObject::invokeMethod(this,[this,frame=lastPrepared.frame,result=lastResult](){commitCapturedFrame(frame);lastSolve_=result;auto j=solveToJson(lastSolve_);emit solveCompleted(j);if(oalWsServer_)oalWsServer_->broadcast("solveResult",j);emit logMessage(lastSolve_.message);emitState();},Qt::BlockingQueuedConnection);
                QJsonObject resultJson=solveToJson(lastResult);resultJson["solverFrameId"]=lastPrepared.frame.id;resultJson["attempts"]=attempts;resultJson["quality"]=solveQualityJson(lastPrepared.quality);resultJson["registeredFrames"]=lastPrepared.registeredFrames;resultJson["effectiveExposureSec"]=lastPrepared.frame.exposureSec;out.success=true;out.result=resultJson;ctx.reportProgress(1.0,"completed",{{"attempt",attempt+1},{"solverFrameId",lastPrepared.frame.id}});return out;
            }
        }
        if(!lastPrepared.frame.image.empty())QMetaObject::invokeMethod(this,[this,frame=lastPrepared.frame,result=lastResult](){commitCapturedFrame(frame);lastSolve_=result;auto j=solveToJson(lastSolve_);emit solveCompleted(j);if(oalWsServer_)oalWsServer_->broadcast("solveResult",j);emit logMessage(lastSolve_.message);emitState();},Qt::BlockingQueuedConnection);
        QJsonObject resultJson=solveToJson(lastResult);resultJson["attempts"]=attempts;resultJson["quality"]=solveQualityJson(lastPrepared.quality);resultJson["solverFrameId"]=lastPrepared.frame.id;out.result=resultJson;out.problem={{"code","ADAPTIVE_SOLVE_FAILED"},{"message",lastResult.message.isEmpty()?"Adaptive plate solve exhausted all attempts":lastResult.message},{"attempts",attempts}};return out;
    });
    emit logMessage(QString("Adaptive plate-solve operation accepted: %1 — base %2 s, %3x%4 effective bin, max %5 s, capture budget %6 s").arg(id).arg(r.exposure.exposureSec,0,'f',3).arg(r.exposure.binX).arg(r.exposure.binY).arg(r.maxSingleExposureSec,0,'f',3).arg(r.maxCapturePhaseSec,0,'f',0));return id;
}
AutofocusResult ApplicationController::autofocus(const AutofocusRequest&r){AutofocusResult x;QString busy;if(!ensureResourcesAvailable({"camera","focuser"},&busy)){x.message=busy;}else if(!camera_||!focuser_){x.message="Camera and focuser must be connected";}else x=autofocusEngine_.run(*camera_,*focuser_,r,[this](const FocusSample&s){QJsonObject j{{"position",s.position},{"score",s.score},{"spread",s.spread},{"detectedStars",s.detectedStars}};emit autofocusProgress(j);if(oalWsServer_)oalWsServer_->broadcast("autofocusProgress",j);},{},[this](const CameraFrame&f,int){publishOperationalPreview(f,"af");});auto j=autofocusToJson(x);emit autofocusCompleted(j);if(oalWsServer_)oalWsServer_->broadcast("autofocusResult",j);emit logMessage(x.message);emitState();return x;}
QString ApplicationController::startAutofocus(const AutofocusRequest&r,QString*error){
    if(!camera_||!focuser_){if(error)*error="Camera and focuser must be connected";return{};}
    auto cam=camera_;auto foc=focuser_;const int coarseCount=std::max(1,r.rangeSteps/std::max(1,r.coarseStep)+1);const int fineHalf=std::max(r.coarseStep*2,r.fineStep*3);const int fineCount=std::max(1,(2*fineHalf)/std::max(1,r.fineStep)+1);const int expected=coarseCount+fineCount;
    const QString id=operations_.submit("autofocus.run",{"camera","focuser"},true,[this,cam,foc,r,expected](OperationContext&ctx){
        ThreadMarshalledCamera cameraProxy(this,cam);ThreadMarshalledFocuser focuserProxy(this,foc);int sampleNo=0;
        ctx.reportProgress(0.0,"starting");
        auto result=autofocusEngine_.run(cameraProxy,focuserProxy,r,[this,&ctx,&sampleNo,expected](const FocusSample&s){
            ++sampleNo;QJsonObject j{{"position",s.position},{"score",s.score},{"spread",s.spread},{"detectedStars",s.detectedStars},{"sample",sampleNo},{"expectedSamples",expected}};
            ctx.reportProgress(std::min(0.95,double(sampleNo)/std::max(1,expected)),"focus.scan",j);
            QMetaObject::invokeMethod(this,[this,j](){emit autofocusProgress(j);if(oalWsServer_)oalWsServer_->broadcast("autofocusProgress",j);},Qt::QueuedConnection);
        },[&ctx](){return ctx.isCancellationRequested();},[this](const CameraFrame&f,int position){QMetaObject::invokeMethod(this,[this,f,position](){publishOperationalPreview(f,"af");},Qt::BlockingQueuedConnection);});
        OperationOutcome out;out.result=autofocusToJson(result);
        if(ctx.isCancellationRequested()||result.message=="Autofocus cancelled"){focuserProxy.halt(nullptr);out.cancelled=true;return out;}
        if(result.success){out.success=true;ctx.reportProgress(1.0,"completed");}
        else out.problem={{"code","AUTOFOCUS_FAILED"},{"message",result.message}};
        return out;
    });
    emit logMessage("Autofocus operation accepted: "+id);return id;
}
bool ApplicationController::cancelOperation(const QString&id,QString*error){
    const auto before=operations_.operationJson(id);const QString kind=before.value("kind").toString();const bool runningExposure=kind=="camera.exposure"&&before.value("state").toString()=="running";const bool runningAdaptiveSolve=kind=="solver.adaptive"&&before.value("state").toString()=="running";const bool runningGuideExposure=kind=="camera.guide.exposure"&&before.value("state").toString()=="running";const bool runningLiveView=kind=="camera.live-view"&&before.value("state").toString()=="running";
    if(!operations_.cancel(id,error))return false;
    if(runningExposure&&camera_&&camera_->canAbortExposure()){QString abortError;if(!camera_->abortExposure(&abortError)&&!abortError.isEmpty())emit logMessage("Exposure abort warning: "+abortError);}
    else if(runningExposure&&camera_)emit logMessage("Exposure cancellation requested; this camera backend cannot interrupt an in-progress capture, so the frame will be discarded when readout returns");
    if(runningAdaptiveSolve&&camera_&&camera_->canAbortExposure()){QString abortError;if(!camera_->abortExposure(&abortError)&&!abortError.isEmpty())emit logMessage("Adaptive solve exposure abort warning: "+abortError);}
    else if(runningAdaptiveSolve&&camera_)emit logMessage("Adaptive solve cancellation requested; current short exposure cannot be interrupted by this backend");
    if(runningGuideExposure&&guideCamera_&&guideCamera_->canAbortExposure())guideCamera_->abortExposure(nullptr);
    // Native QHY Live View is a continuous SDK stream, not a single exposure.
    // Let its worker leave the frame loop and call StopQHYCCDLive itself; using
    // CancelQHYCCDExposingAndReadout here corrupts the subsequent still mode.
    if(runningLiveView&&camera_&&!camera_->backendName().startsWith("native:oal.qhy/")&&camera_->canAbortExposure())camera_->abortExposure(nullptr);
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
bool ApplicationController::slewMount(const EquatorialCoord&t,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}const auto target=convertEquatorialFrame(t,EquatorialFrame::J2000);MountStatus before;QString statusError;if(mountStatus(before,&statusError)&&before.coordinateValid){double dra=target.raDeg-before.coordinate.raDeg;while(dra>180.0)dra-=360.0;while(dra<-180.0)dra+=360.0;QString pierError;const QString destinationPier=mount_->destinationPierSide(target,&pierError);emit logMessage(QString("Mount GOTO preflight [J2000]: current RA=%1° DEC=%2° pier=%3 tracking=%4 -> target RA=%5° DEC=%6° (short dRA=%7° dDEC=%8°%9)").arg(before.coordinate.raDeg,0,'f',6).arg(before.coordinate.decDeg,0,'f',6).arg(before.pierSide).arg(before.tracking?"ON":"OFF").arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6).arg(dra,0,'f',6).arg(target.decDeg-before.coordinate.decDeg,0,'f',6).arg(destinationPier.isEmpty()?QString():QString(" destinationPier=%1").arg(destinationPier)));if(!pierError.isEmpty())emit logMessage("Mount DestinationSideOfPier unavailable: "+pierError);}else if(!statusError.isEmpty())emit logMessage("Mount GOTO preflight status unavailable: "+statusError);bool ok=mount_->slewTo(target,e);if(ok)emit logMessage(QString("Mount slew accepted: RA=%1°, DEC=%2° J2000 (no RA/DEC sign inversion)").arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6));emitState();return ok;}
QString ApplicationController::startMountSlew(const EquatorialCoord&t,QString*e){
    if(!mount_){if(e)*e="No mount connected";return{};}const auto target=convertEquatorialFrame(t,EquatorialFrame::J2000);auto mount=mount_;const bool ascomDiagnostics=mount->backendName()=="ascom-classic";
    const QString id=operations_.submit("mount.slew",{"mount"},true,[this,mount,target,ascomDiagnostics](OperationContext&ctx){
        ThreadMarshalledMount proxy(this,mount);QString err;OperationOutcome out;ctx.reportProgress(0.05,"commanding",{{"raDeg",target.raDeg},{"decDeg",target.decDeg},{"coordinateFrame","J2000"}});
        if(!proxy.slewTo(target,&err)){out.problem={{"code","MOUNT_SLEW_FAILED"},{"message",err}};return out;}
        ctx.reportProgress(0.25,"slewing");
        for(int i=0;i<3000;++i){
            if(ctx.isCancellationRequested()){proxy.abortMotion(nullptr);out.cancelled=true;return out;}
            MountStatus st;if(!proxy.status(st,&err)){out.problem={{"code","MOUNT_STATUS_FAILED"},{"message",err}};return out;}
            if(ascomDiagnostics&&(i%5)==0){const auto current=st.coordinateValid?convertEquatorialFrame(st.coordinate,EquatorialFrame::J2000):st.coordinate;emit logMessage(QString("ASCOM slew sample: t=%1 s RA=%2° DEC=%3° pier=%4 tracking=%5 slewing=%6").arg(i*0.2,0,'f',1).arg(current.raDeg,0,'f',6).arg(current.decDeg,0,'f',6).arg(st.pierSide).arg(st.tracking?"ON":"OFF").arg(st.slewing?"YES":"NO"));}
            if(!st.slewing){
                if(st.coordinateValid){
                    auto current=convertEquatorialFrame(st.coordinate,EquatorialFrame::J2000);
                    double dra=target.raDeg-current.raDeg;while(dra>180.0)dra-=360.0;while(dra<-180.0)dra+=360.0;
                    const double ddec=target.decDeg-current.decDeg;
                    if(std::max(std::abs(dra),std::abs(ddec))>0.5){
                        out.problem={{"code","MOUNT_SLEW_INCOMPLETE"},{"message",QString("Mount stopped without reaching target: residual dRA=%1 deg dDEC=%2 deg").arg(dra,0,'f',3).arg(ddec,0,'f',3)}};
                        return out;
                    }
                }
                out.success=true;out.result=QJsonObject{{"raDeg",st.coordinate.raDeg},{"decDeg",st.coordinate.decDeg},{"tracking",st.tracking},{"parked",st.parked}};ctx.reportProgress(1.0,"completed");return out;
            }
            ctx.reportProgress(0.25,"slewing",{{"raDeg",st.coordinate.raDeg},{"decDeg",st.coordinate.decDeg}});QThread::msleep(200);
        }
        proxy.abortMotion(nullptr);out.problem={{"code","MOUNT_SLEW_TIMEOUT"},{"message","Mount slew did not complete within 10 minutes"}};return out;
    });
    emit logMessage(QString("Mount slew operation accepted: %1 → RA=%2°, DEC=%3° J2000").arg(id).arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6));return id;
}
bool ApplicationController::abortMountMotion(QString*e){QString owner;if(operations_.isResourceLocked("mount",&owner))operations_.cancel(owner,nullptr);if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->abortMotion(e);if(ok)emit logMessage("Mount motion abort requested");emitState();return ok;}
bool ApplicationController::syncMount(const EquatorialCoord&t,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}const auto target=convertEquatorialFrame(t,EquatorialFrame::J2000);bool ok=mount_->syncTo(target,e);if(ok)emit logMessage(QString("Mount sync: RA=%1°, DEC=%2° J2000").arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6));emitState();return ok;}
bool ApplicationController::mountStatus(MountStatus&s,QString*e)const{if(!mount_){if(e)*e="No mount connected";return false;}if(!mount_->status(s,e))return false;if(s.coordinateValid)s.coordinate=convertEquatorialFrame(s.coordinate,EquatorialFrame::J2000);return true;}
bool ApplicationController::focuserStatus(FocuserStatus&s,QString*e)const{if(!focuser_){if(e)*e="No focuser connected";return false;}return focuser_->status(s,e);}
bool ApplicationController::moveFocuser(int p,QString*e){if(!ensureResourcesAvailable({"focuser"},e))return false;if(!focuser_){if(e)*e="No focuser connected";return false;}bool ok=focuser_->moveAbsolute(p,e);emitState();return ok;}
bool ApplicationController::haltFocuser(QString*e){QString owner;if(operations_.isResourceLocked("focuser",&owner))operations_.cancel(owner,nullptr);if(!focuser_){if(e)*e="No focuser connected";return false;}bool ok=focuser_->halt(e);if(ok)emit logMessage("Focuser halt requested");return ok;}
bool ApplicationController::setMountTracking(bool v,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->setTracking(v,e);if(ok)emit logMessage(QString("Mount tracking %1").arg(v?"ON":"OFF"));emitState();return ok;}
bool ApplicationController::parkMount(bool v,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->park(v,e);if(ok)emit logMessage(v?"Mount parking slew started":"Mount unpark/parking-stop accepted");emitState();return ok;}
bool ApplicationController::pulseGuide(GuideDirection d,int ms,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}return mount_->pulseGuide(d,ms,e);}
bool ApplicationController::manualMountSlew(int a1,int a2,int rate,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}const bool ok=mount_->manualSlew(std::clamp(a1,-1,1),std::clamp(a2,-1,1),std::clamp(rate,0,9),e);if(ok){emit logMessage(QString("Manual mount slew: axis1=%1 axis2=%2 rate=%3").arg(a1).arg(a2).arg(rate));emitState();}return ok;}
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
bool ApplicationController::frameById(const QString&id,CameraFrame&frame,QString*error)const{if(!lastFrame_.image.empty()&&(id==lastFrame_.id||id=="latest")){frame=lastFrame_;return true;}if(!lastGuideFrame_.image.empty()&&(id==lastGuideFrame_.id||id=="latest-guide")){frame=lastGuideFrame_;return true;}for(auto it=previewFrameCache_.rbegin();it!=previewFrameCache_.rend();++it)if(it->id==id){frame=*it;return true;}if(!previousFrame_.image.empty()&&id==previousFrame_.id){frame=previousFrame_;return true;}if(error)*error="Frame is no longer available in the in-memory preview cache";return false;}
QJsonObject ApplicationController::stateJson()const{auto strings=[](const QStringList&xs){QJsonArray a;for(const auto&x:xs)a.append(x);return a;};QJsonObject j{{"timestampUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"devices",devicesJson()},{"backends",QJsonObject{{"camera",strings(cameraBackends())},{"mount",strings(mountBackends())},{"focuser",strings(focuserBackends())},{"solver",strings(solverBackends())}}},{"solve",solveToJson(lastSolve_)},{"session",sessionJson(scheduler_.status())},{"operations",operations_.operationsJson(true)},{"resourceLocks",operations_.locksJson()},{"stellarium",QJsonObject{{"running",stellariumRunning()},{"port",int(stellariumPort())}}}};if(!lastFrame_.image.empty())j["lastFrame"]=QJsonObject{{"frameId",lastFrame_.id},{"capturedUtc",lastFrame_.capturedUtc.toString(Qt::ISODateWithMs)},{"width",lastFrame_.image.cols},{"height",lastFrame_.image.rows},{"exposureSec",lastFrame_.exposureSec},{"gain",lastFrame_.gain},{"binX",lastFrame_.binX},{"binY",lastFrame_.binY},{"role","main"}};if(!lastGuideFrame_.image.empty())j["lastGuideFrame"]=QJsonObject{{"frameId",lastGuideFrame_.id},{"capturedUtc",lastGuideFrame_.capturedUtc.toString(Qt::ISODateWithMs)},{"width",lastGuideFrame_.image.cols},{"height",lastGuideFrame_.image.rows},{"exposureSec",lastGuideFrame_.exposureSec},{"gain",lastGuideFrame_.gain},{"binX",lastGuideFrame_.binX},{"binY",lastGuideFrame_.binY},{"role","guide"}};MountStatus m;if(mountStatus(m,nullptr)){QJsonObject mj{{"raDeg",m.coordinate.raDeg},{"decDeg",m.coordinate.decDeg},{"coordinateFrame",equatorialFrameName(m.coordinate.frame)},{"coordinateValid",m.coordinateValid},{"tracking",m.tracking},{"slewing",m.slewing},{"parked",m.parked},{"pierSide",m.pierSide},{"geometryType",m.geometryType}};if(m.axes.valid){mj["axis1Deg"]=m.axes.axis1Deg;mj["axis2Deg"]=m.axes.axis2Deg;mj["axesValid"]=true;}else mj["axesValid"]=false;j["mount"]=mj;}FocuserStatus f;if(focuserStatus(f,nullptr)){QJsonObject fj{{"position",f.position},{"moving",f.moving}};if(f.temperatureC)fj["temperatureC"]=*f.temperatureC;j["focuser"]=fj;}auto g=guiding_.status();j["guiding"]=QJsonObject{{"active",g.active},{"raErrorArcsec",g.raErrorArcsec},{"decErrorArcsec",g.decErrorArcsec},{"rmsArcsec",g.rmsArcsec}};return j;}
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
    refreshNativeDiscoveryAsync({},true);
    emit logMessage("Explicit native device discovery started (vendor-neutral; QHY hard SDK recovery allowed if needed)");
    return true;
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
    if(driverId=="oal.eqdrive")return qEnvironmentVariable("OAL_EQDRIVE_PORT").trimmed();
    return {};
}
bool ApplicationController::setNativeSerialPortOverride(const QString&driverId,const QString&port,QString*error){
    const QString value=port.trimmed();QString envName;
    if(driverId=="oal.gemini")envName="OAL_GEMINI_PORT";
    else if(driverId=="oal.skywatcher")envName="OAL_SKYWATCHER_PORT";
    else if(driverId=="oal.eqdrive")envName="OAL_EQDRIVE_PORT";
    else{if(error)*error="Serial-port override is supported only for oal.gemini, oal.skywatcher and oal.eqdrive";return false;}
    const QByteArray env=envName.toUtf8();
    if(value.isEmpty())qunsetenv(env.constData());else qputenv(env.constData(),value.toUtf8());
    settings_.saveNativeSerialPort(driverId,value);
    emit logMessage(value.isEmpty()?QString("%1 serial discovery set to automatic scan").arg(driverId):QString("%1 serial discovery pinned to %2").arg(driverId,value));

    // If this exact native device is already in the live catalogue, selecting
    // the same port must not physically re-open it. Gemini's CH340 controller
    // audibly resets/beeps on open, so the old synchronous "Apply" path caused
    // a needless reset every click and blocked the HTTP request for seconds.
    QJsonObject cached;
    if(driverLoader_&&!value.isEmpty()){
        const QString type=driverId=="oal.gemini"?"focuser":"mount";
        for(const auto&v:driverLoader_->devices()){
            const auto o=v.toObject();
            if(o.value("driverId").toString()!=driverId||o.value("type").toString()!=type)continue;
            if(o.value("transport").toObject().value("port").toString().compare(value,Qt::CaseInsensitive)==0){cached=o;break;}
        }
        if(!cached.isEmpty()){
            const QString key=nativeBackendKey(driverId,cached.value("id").toString());
            if(type=="focuser"){
                auto b=settings_.focuserBinding();QString oldDriver,oldId;
                if(parseNativeBackendKey(b.backend,oldDriver,oldId)&&oldDriver==driverId){b.backend=key;b.endpoint.clear();settings_.saveFocuserBinding(b);}
            }else{
                auto b=settings_.mountBinding();QString oldDriver,oldId;
                if(parseNativeBackendKey(b.backend,oldDriver,oldId)&&oldDriver==driverId){b.backend=key;b.endpoint.clear();settings_.saveMountBinding(b);}
            }
            emit logMessage(QString("%1 on %2 is already present in the native catalogue; no serial reprobe was needed").arg(driverId,value));
            emitState();
            return true;
        }
    }

    // Discovery is deliberately asynchronous: changing a serial selector must
    // never make the GUI wait for Gemini's reset recovery or an EQDrive baud
    // probe. The state WebSocket will refresh the combobox as soon as the
    // device appears. Requests are queued if another startup scan is in flight.
    refreshNativeDiscoveryAsync(QStringList{driverId});
    emitState();
    return true;
}
QJsonArray ApplicationController::nativeDriversJson()const{return driverLoader_?driverLoader_->drivers():QJsonArray{};}
QJsonArray ApplicationController::nativeDevicesJson()const{return driverLoader_?driverLoader_->devices():QJsonArray{};}
QJsonObject ApplicationController::nativeCapabilitiesJson(const QString&driverId,const QString&deviceId,QString*error)const{return driverLoader_?driverLoader_->capabilities(driverId,deviceId,error):QJsonObject{};}
void ApplicationController::emitState(){if(shuttingDown_)return;auto j=stateJson();emit stateChanged(j);if(oalWsServer_)oalWsServer_->broadcast("state",j);}
QImage ApplicationController::toQImage(const cv::Mat&i){if(i.empty())return{};cv::Mat rgb;if(i.channels()==1){cv::Mat u8;if(i.depth()==CV_16U){double mn,mx;cv::minMaxLoc(i,&mn,&mx);i.convertTo(u8,CV_8U,255.0/std::max(1.0,mx-mn),-mn*255.0/std::max(1.0,mx-mn));}else i.convertTo(u8,CV_8U);cv::cvtColor(u8,rgb,cv::COLOR_GRAY2RGB);}else{cv::Mat bgr8;if(i.depth()==CV_16U)i.convertTo(bgr8,CV_8U,255.0/65535.0);else if(i.depth()!=CV_8U){double mn=0,mx=0;cv::minMaxLoc(i.reshape(1),&mn,&mx);i.convertTo(bgr8,CV_8U,255.0/std::max(1.0,mx));}else bgr8=i;cv::cvtColor(bgr8,rgb,cv::COLOR_BGR2RGB);}return QImage(rgb.data,rgb.cols,rgb.rows,int(rgb.step),QImage::Format_RGB888).copy();}
} // namespace oas
