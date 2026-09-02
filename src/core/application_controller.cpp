#include "core/application_controller.h"
#include "core/equatorial_frames.h"
#include "core/ser_writer.h"
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
#include <QJsonDocument>
#include <QUrl>
#include <QTimer>
#include <QThread>
#include <QSerialPortInfo>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QFileInfo>
#include <QSysInfo>
#include <QStandardPaths>
#include <opencv2/imgproc.hpp>
#ifdef OAS_HAVE_POSITIONING
#include <QGeoPositionInfoSource>
#include <QGeoCoordinate>
#endif
#include <algorithm>
#include <cmath>

namespace oas {
namespace {
double skyAngularSeparationDeg(const EquatorialCoord&a,const EquatorialCoord&b){
    constexpr double d2r=3.14159265358979323846/180.0;
    const double da=(b.raDeg-a.raDeg)*d2r,d1=a.decDeg*d2r,d2=b.decDeg*d2r;
    const double c=std::sin(d1)*std::sin(d2)+std::cos(d1)*std::cos(d2)*std::cos(da);
    return std::acos(std::clamp(c,-1.0,1.0))/d2r;
}
cv::Rect centeredPlanetaryRoi(const cv::Point2d &center,int width,int height,int sensorW,int sensorH){
    sensorW=std::max(1,sensorW);sensorH=std::max(1,sensorH);
    width=width>0?std::min(width,sensorW):std::min(640,sensorW);height=height>0?std::min(height,sensorH):std::min(480,sensorH);
    if(width>=8)width=std::max(8,(width/8)*8);if(height>=2)height=std::max(2,(height/2)*2);
    int x=int(std::lround(center.x-0.5*width)),y=int(std::lround(center.y-0.5*height));
    x=std::clamp(x,0,std::max(0,sensorW-width));y=std::clamp(y,0,std::max(0,sensorH-height));
    if(x>=2)x=(x/2)*2;if(y>=2)y=(y/2)*2;
    x=std::clamp(x,0,std::max(0,sensorW-width));y=std::clamp(y,0,std::max(0,sensorH-height));
    return {x,y,width,height};
}
QString planetarySerPath(const ObservationBlock &block,int runIndex,const QString &requested){
    QString path=requested.trimmed();
    if(path.isEmpty()){QString base=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);if(base.isEmpty())base=QDir::homePath();QString safe=block.name.trimmed();if(safe.isEmpty())safe="Planet";safe.replace(QRegularExpression("[^A-Za-z0-9_.-]+"),"_");path=QDir(base).filePath(QString("OpenAstroLink/SER/%1_%2_run%3.ser").arg(safe,QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz")).arg(runIndex+1,2,10,QChar('0')));}
    else if(block.planetary.serRuns>1){QFileInfo fi(path);path=QDir(fi.absolutePath()).filePath(QString("%1_run%2.%3").arg(fi.completeBaseName()).arg(runIndex+1,2,10,QChar('0')).arg(fi.suffix().isEmpty()?"ser":fi.suffix()));}
    return path;
}
QString compactJson(const QJsonObject&o){return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));}
QString mountDiagnosticSnapshot(const QString &backend,const MountStatus &st,const ObserverLocation &observer,const QDateTime &utc){
    QStringList parts;parts<<QString("backend=%1").arg(backend)
        <<QString("UTC=%1").arg(utc.toUTC().toString(Qt::ISODateWithMs))
        <<QString("local=%1").arg(utc.toLocalTime().toString(Qt::ISODateWithMs))
        <<QString("site=(%1,%2,%3m)").arg(observer.latitudeDeg,0,'f',6).arg(observer.longitudeDeg,0,'f',6).arg(observer.elevationM,0,'f',1)
        <<QString("LST=%1deg/%2h").arg(localSiderealTimeDeg(observer,utc),0,'f',6).arg(localSiderealTimeDeg(observer,utc)/15.0,0,'f',6)
        <<QString("tracking=%1 slewing=%2 parked=%3 pier=%4 geometry=%5").arg(st.tracking?"ON":"OFF").arg(st.slewing?"YES":"NO").arg(st.parked?"YES":"NO").arg(st.pierSide,st.geometryType);
    if(st.coordinateValid){const auto j2000=convertEquatorialFrame(st.coordinate,EquatorialFrame::J2000,utc);const auto jnow=convertEquatorialFrame(j2000,EquatorialFrame::JNow,utc);const auto hor=equatorialToHorizontal(j2000,observer,utc);parts<<QString("J2000=(%1,%2) JNow=(%3,%4) AzAlt=(%5,%6)").arg(j2000.raDeg,0,'f',6).arg(j2000.decDeg,0,'f',6).arg(jnow.raDeg,0,'f',6).arg(jnow.decDeg,0,'f',6).arg(hor.azDeg,0,'f',6).arg(hor.altDeg,0,'f',6);}
    else parts<<"sky=INVALID/UNSYNCED";
    if(st.axes.valid)parts<<QString("axes=(%1,%2)").arg(st.axes.axis1Deg,0,'f',6).arg(st.axes.axis2Deg,0,'f',6);
    if(!st.diagnostics.isEmpty())parts<<QString("backendDiagnostics=%1").arg(compactJson(st.diagnostics));
    return parts.join(" | ");
}
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
    bool setTracking(bool v,TrackingRate rate=TrackingRate::Sidereal,QString*e=nullptr)override{return direct_?inner_->setTracking(v,rate,e):invokeOnControllerThread(owner_,[this,v,rate,e](){return inner_->setTracking(v,rate,e);});}
    bool park(bool v,QString*e=nullptr)override{return direct_?inner_->park(v,e):invokeOnControllerThread(owner_,[this,v,e](){return inner_->park(v,e);});}
    bool setCurrentParkPosition(QString*e=nullptr)override{return direct_?inner_->setCurrentParkPosition(e):invokeOnControllerThread(owner_,[this,e](){return inner_->setCurrentParkPosition(e);});}
    bool pulseGuide(GuideDirection d,int ms,QString*e=nullptr)override{return direct_?inner_->pulseGuide(d,ms,e):invokeOnControllerThread(owner_,[this,d,ms,e](){return inner_->pulseGuide(d,ms,e);});}
private:QObject *owner_;std::shared_ptr<IMount> inner_;bool direct_{false};
};
}
static QJsonObject sessionJson(const SessionStatus&s){return sessionStatusToJson(s);}
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
        handleSessionOperationUpdate(o);
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
void ApplicationController::setProfile(const TelescopeProfile&p){
    const auto old=profile_;
    const bool transformChanged =
        old.mount.type!=p.mount.type || old.mount.axis1Sign!=p.mount.axis1Sign || old.mount.axis2Sign!=p.mount.axis2Sign ||
        old.mount.nativeCoordinateModelVersion!=p.mount.nativeCoordinateModelVersion ||
        old.mount.preferredPierSide.compare(p.mount.preferredPierSide,Qt::CaseInsensitive)!=0 ||
        std::abs(old.observer.latitudeDeg-p.observer.latitudeDeg)>1e-10 ||
        std::abs(old.observer.longitudeDeg-p.observer.longitudeDeg)>1e-10;
    profile_=p;settings_.saveProfile(p);if(mount_)mount_->configureGeometry(profile_.mount,profile_.observer);
    emit profileChanged();
    if(transformChanged)emit logMessage("Mount coordinate-transform profile changed; native/direct mount Sync is invalidated");
    else emit logMessage("Mount operational/profile settings updated; existing Sync preserved");
    emitState();
}
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
bool ApplicationController::connectMount(const QString&b,const QString&e,QString*err){
    if(!ensureResourcesAvailable({"mount"},err))return false;
    std::shared_ptr<IMount>d;QString driverId,deviceId;
    QString resolvedEndpoint=e.trimmed();
    if(b=="ascom-classic"&&resolvedEndpoint.isEmpty())resolvedEndpoint="EQMOD.Telescope";
    bool migratedDirectMcModel=false;
    auto migrateDirectMotorControllerModel=[this,&migratedDirectMcModel](){
        if(profile_.mount.nativeCoordinateModelVersion>=9)return;
        // v9 retains the v7 EQMOD-style GEM HA/DEC pointing-state geometry,
        // but corrects the direct Motor Controller installation mapping after
        // a second 2026-09-02 HIL that measured an exact east/west sky mirror.
        // Recomputing all four sign combinations from the observed controller
        // deltas shows that the mirror is produced by the DEC/polar-distance
        // axis, not by the hour axis: Axis1=+1, Axis2=-1.
        //
        // The controller counts captured at counterweight-down polar Home stay
        // Axis1=0°, Axis2=0° and the v7 branch equations stay unchanged.
        const int previousVersion=profile_.mount.nativeCoordinateModelVersion;
        const bool hadLegacyCustomPose=profile_.mount.customHome||profile_.mount.customPark;
        profile_.mount.nativeCoordinateModelVersion=9;
        profile_.mount.axis1Sign=1;profile_.mount.axis2Sign=-1;profile_.mount.preferredPierSide="west";
        if(previousVersion<7){
            // v6 and earlier may have encoded incompatible custom coordinate
            // offsets, so keep the one-time v7 reset for those legacy profiles.
            profile_.mount.customHome=false;profile_.mount.customPark=false;
            profile_.mount.homeAxis1Deg=0.0;profile_.mount.homeAxis2Deg=0.0;
            profile_.mount.parkAxis1Deg=0.0;profile_.mount.parkAxis2Deg=0.0;
        }
        profile_.mount.autoHomeSync=true;
        settings_.saveProfile(profile_);emit profileChanged();migratedDirectMcModel=true;
        emit logMessage(previousVersion<7&&hadLegacyCustomPose
            ? "Native direct-MC coordinate model migrated to v9: legacy custom Home/Park cleared; EQMOD GEM pointing-state geometry retained with HIL-qualified physical axis signs Axis1=+1, Axis2=-1."
            : "Native direct-MC coordinate model migrated to v9: v7 EQMOD GEM branch geometry retained; 2026-09-02 encoder/sky recomputation selects direct-MC Axis1=+1, Axis2=-1 (DEC/polar-distance mirror fix).");
    };
    if(parseNativeBackendKey(b,driverId,deviceId)){
        if(driverId=="oal.eqdrive")migrateDirectMotorControllerModel();
        auto desc=nativeDescriptor(driverLoader_,b,"mount",err);if(desc.isEmpty())return false;
        d=std::make_shared<NativeOalMount>(driverLoader_,desc,profile_.mount,profile_.observer);
    }
    else if(b=="simulated")d=std::make_shared<SimulatedMount>();
    else if(b=="serial-lx200")d=std::make_shared<SerialLx200Mount>(e);
    else if(b=="synscan-app")d=std::make_shared<SynScanAppMount>(e);
    else if(b=="synscan-wifi"){migrateDirectMotorControllerModel();d=std::make_shared<SynScanNetworkMount>(e,profile_.mount,profile_.observer);}
#ifdef OAS_HAVE_ASCOM_CLASSIC
    else if(b=="ascom-classic")d=std::make_shared<AscomClassicMount>(resolvedEndpoint);
#endif
    else if(b=="ascom-alpaca")d=std::make_shared<AlpacaMount>(QUrl(e));
    else if(b=="oal")d=std::make_shared<OalMountClient>(QUrl(e));
#ifdef OAS_HAVE_INDI
    else if(b=="indi")d=std::make_shared<IndiMount>(e);
#endif
    else{if(err)*err="Unknown mount backend";return false;}
    if(!d->connectDevice(err))return false;
    mount_=std::move(d);settings_.saveMountBinding({mount_->backendName(),resolvedEndpoint,true});
    emit logMessage("Mount connected: "+mount_->displayName());

    // Classic ASCOM/EQMOD owns its own horizon/hour-angle transform. By
    // default OAL therefore adopts a valid backend site into the OAL profile,
    // keeping one authoritative observatory location instead of two divergent
    // copies. Users can disable this preference to push the OAL site instead.
    if(mount_->backendName()=="ascom-classic"){
        QString autoSiteError;
        if(!ensureMountBackendSiteTime(&autoSiteError))
            emit logMessage("MOUNT SITE AUTO-SYNC WARNING: "+autoSiteError);
    }

    {MountStatus st;QString se;if(mountStatus(st,&se)){
        emit logMessage("Mount diagnostic CONNECT: "+mountDiagnosticSnapshot(mount_->backendName(),st,profile_.observer,QDateTime::currentDateTimeUtc()));
        if(!st.diagnostics.isEmpty()){
            const QString align=st.diagnostics.value("alignmentSource").toString();
            if(align=="auto-home"||align=="eqdrive-zero-home"||align=="polar-home-sync")emit logMessage("Mount automatic/Home alignment restored: "+st.diagnostics.value("homeAlignmentNote").toString());
            else if(st.diagnostics.value("autoHomeSyncEnabled").toBool()&&!st.coordinateValid)emit logMessage("Mount automatic Home alignment not active: "+st.diagnostics.value("homeAlignmentNote").toString());
            const double blat=st.diagnostics.value("siteLatitude").toDouble(999.0),blon=st.diagnostics.value("siteLongitude").toDouble(999.0);
            if(std::abs(blat)<=90.0&&std::abs(blon)<=180.0&&(std::abs(blat-profile_.observer.latitudeDeg)>0.01||std::abs(blon-profile_.observer.longitudeDeg)>0.01))
                emit logMessage(QString("MOUNT SITE WARNING: backend site (%1,%2) differs from OpenAstroLink profile (%3,%4). Automatic synchronization was attempted; do not trust ASCOM GOTO until this warning disappears.").arg(blat,0,'f',6).arg(blon,0,'f',6).arg(profile_.observer.latitudeDeg,0,'f',6).arg(profile_.observer.longitudeDeg,0,'f',6));
            if(st.diagnostics.value("equatorialFrameAssumed").toBool())
                emit logMessage("ASCOM EQUATORIAL FRAME WARNING: "+st.diagnostics.value("equatorialFrameAssumption").toString()+". For EQMOD, explicitly selecting JNOW or J2000 in EQMOD ASCOM Setup is recommended.");
        }
    }else emit logMessage("Mount diagnostic CONNECT status unavailable: "+se);}
    emitState();return true;
}
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
bool ApplicationController::ensureMountBackendSiteTime(QString*error){
    if(!mount_||mount_->backendName()!="ascom-classic"){if(error)error->clear();return true;}
    MountStatus st;QString statusError;
    if(!mount_->status(st,&statusError)){if(error)*error="Could not read ASCOM site/status: "+statusError;return false;}
    const double lat=st.diagnostics.value("siteLatitude").toDouble(999.0);
    const double lon=st.diagnostics.value("siteLongitude").toDouble(999.0);
    const double elev=st.diagnostics.value("siteElevation").toDouble(profile_.observer.elevationM);
    const bool validSite=std::isfinite(lat)&&std::isfinite(lon)&&std::abs(lat)<=90.0&&std::abs(lon)<=180.0;

    if(profile_.mount.preferBackendSite){
        if(validSite){
            const bool changed=std::abs(lat-profile_.observer.latitudeDeg)>1e-8||std::abs(lon-profile_.observer.longitudeDeg)>1e-8||
                               (std::isfinite(elev)&&std::abs(elev-profile_.observer.elevationM)>1e-6);
            if(changed){
                profile_.observer.latitudeDeg=lat;profile_.observer.longitudeDeg=lon;if(std::isfinite(elev))profile_.observer.elevationM=elev;
                settings_.saveProfile(profile_);emit profileChanged();
                emit logMessage(QString("ASCOM backend site adopted into OpenAstroLink profile (backend authoritative): lat=%1 lon=%2 elev=%3m")
                    .arg(profile_.observer.latitudeDeg,0,'f',6).arg(profile_.observer.longitudeDeg,0,'f',6).arg(profile_.observer.elevationM,0,'f',1));
            }
            if(error)error->clear();return true;
        }
        // Equatorial GOTO remains usable even if this particular ASCOM driver
        // cannot expose its site. Do not invent or overwrite a location.
        emit logMessage("ASCOM backend-site preference is enabled, but this driver did not expose a valid SiteLatitude/SiteLongitude; keeping the current OAL profile site.");
        if(error)error->clear();return true;
    }

    if(!validSite){if(error)error->clear();return true;}
    const bool mismatch=std::abs(lat-profile_.observer.latitudeDeg)>0.01||std::abs(lon-profile_.observer.longitudeDeg)>0.01;
    if(!mismatch){if(error)error->clear();return true;}
    QString applyError;const auto utc=QDateTime::currentDateTimeUtc();
    if(!mount_->setSiteTime(profile_.observer,utc,&applyError)){
        QString hint;if(st.diagnostics.value("progId").toString().startsWith("EQMOD.",Qt::CaseInsensitive))
            hint=" EQMOD normally requires 'ASCOM Options -> Allow Site Writes' to be enabled; alternatively set the same latitude/longitude in EQMOD ASCOM Setup.";
        if(error)*error=QString("ASCOM backend site differs from OAL site and automatic correction failed: %1.%2").arg(applyError,hint);return false;
    }
    emit logMessage(QString("ASCOM site/time synchronized from OAL profile: lat=%1 lon=%2 elev=%3m UTC=%4")
        .arg(profile_.observer.latitudeDeg,0,'f',6).arg(profile_.observer.longitudeDeg,0,'f',6).arg(profile_.observer.elevationM,0,'f',1).arg(utc.toString(Qt::ISODateWithMs)));
    if(error)error->clear();return true;
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
    LiveViewRequest r=request;r.exposureSec=std::clamp(r.exposureSec,0.0001,10.0);r.gain=std::max(0,r.gain);r.offset=std::max(0,r.offset);r.binX=std::clamp(r.binX,1,4);r.binY=std::clamp(r.binY,1,4);r.targetFps=std::clamp(r.targetFps,0.2,30.0);if(r.roi.width<0||r.roi.height<0)r.roi={};if(r.recordSer&&r.serPath.trimmed().isEmpty()){QString base=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);if(base.isEmpty())base=QDir::homePath();r.serPath=QDir(base).filePath(QString("OpenAstroLink/SER/Live_%1.ser").arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz")));}
    // CFA pixels must remain on their native 1x1 lattice for software debayer.
    // Drivers that already return RGB are harmlessly passed through, but forcing
    // 1x1 here keeps raw QHY/ZWO previews color-correct across vendors.
    if(r.debayer&&(r.binX!=1||r.binY!=1)){r.binX=r.binY=1;emit logMessage("Live View debayer enabled: forcing 1x1 readout so the Bayer mosaic remains valid");}
    auto cam=camera_;const QString liveCameraBackend=cam->backendName(),liveCameraName=cam->displayName();
    const QString id=operations_.submit("camera.live-view",{"camera"},true,[this,cam,r,liveCameraBackend,liveCameraName](OperationContext&ctx){
        OperationOutcome out;int frames=0;QElapsedTimer elapsed;elapsed.start();std::unique_ptr<SerWriter> ser;auto appendSer=[&](const CameraFrame&raw,QString&err){if(!r.recordSer)return true;if(!ser){ser=std::make_unique<SerWriter>();if(!ser->open(r.serPath,raw,profile_,r,liveCameraBackend,liveCameraName,&err))return false;const QString sidecar=ser->sidecarPath();QMetaObject::invokeMethod(this,[this,path=r.serPath,sidecar](){emit logMessage("SER recording started: "+path+"; metadata sidecar: "+sidecar);},Qt::QueuedConnection);}return ser->append(raw,&err);};auto closeSer=[&](){if(!ser)return;QString ce;const quint32 n=ser->frameCount();const QString path=ser->path(),sidecar=ser->sidecarPath();ser->close(&ce);QMetaObject::invokeMethod(this,[this,path,sidecar,n,ce](){emit logMessage(QString("SER recording finished: %1 frames → %2; metadata → %3%4").arg(n).arg(path).arg(sidecar).arg(ce.isEmpty()?QString():QString(" — WARNING: "+ce)));},Qt::QueuedConnection);ser.reset();};
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
                if(!native->nextNativeLiveFrame(frame,timeoutMs,&err)){if(ctx.isCancellationRequested()){stopStream();closeSer();out.cancelled=true;return out;}stopStream();closeSer();out.problem={{"code","LIVE_VIEW_FRAME_FAILED"},{"message",err}};return out;}
                if(ctx.isCancellationRequested()){stopStream();closeSer();out.cancelled=true;return out;}
                QString serError;if(!appendSer(frame,serError)){stopStream();closeSer();out.problem={{"code","SER_WRITE_FAILED"},{"message",serError}};return out;}
                QString previewNote;if(!processLivePreview(frame,r,&previewNote)){stopStream();closeSer();out.problem={{"code","LIVE_VIEW_DEBAYER_FAILED"},{"message",previewNote}};return out;}
                frame.id="live-"+frame.id;frame.scienceFilePath.clear();++frames;
                QMetaObject::invokeMethod(this,[this,frame](){commitCapturedFrame(frame,false,false);},Qt::BlockingQueuedConnection);
                const double actualFps=elapsed.elapsed()>0?1000.0*double(frames)/double(elapsed.elapsed()):0.0;
                ctx.reportProgress(0.0,"live",{{"frames",frames},{"actualFps",actualFps},{"targetFps",r.targetFps},{"frameId",frame.id},{"transport","native-stream"}});
                qint64 sleepMs=targetPeriodMs-cycle.elapsed();while(sleepMs>0&&!ctx.isCancellationRequested()){const int slice=int(std::min<qint64>(sleepMs,25));QThread::msleep(slice);sleepMs-=slice;}
            }
            stopStream();closeSer();out.cancelled=true;return out;
        }
        ThreadMarshalledCamera proxy(this,cam);
        while(!ctx.isCancellationRequested()){
            QElapsedTimer cycle;cycle.start();ExposureRequest e;e.exposureSec=r.exposureSec;e.gain=r.gain;e.offset=r.offset;e.binX=r.binX;e.binY=r.binY;e.roi=r.roi;e.saveRaw=false;CameraFrame frame;QString err;
            if(!proxy.capture(e,frame,&err)){if(ctx.isCancellationRequested()){closeSer();out.cancelled=true;return out;}closeSer();out.problem={{"code","LIVE_VIEW_CAPTURE_FAILED"},{"message",err}};return out;}
            if(ctx.isCancellationRequested()){closeSer();out.cancelled=true;return out;}
            QString serError;if(!appendSer(frame,serError)){closeSer();out.problem={{"code","SER_WRITE_FAILED"},{"message",serError}};return out;}
            QString previewNote;if(!processLivePreview(frame,r,&previewNote)){closeSer();out.problem={{"code","LIVE_VIEW_DEBAYER_FAILED"},{"message",previewNote}};return out;}
            frame.id="live-"+frame.id;frame.scienceFilePath.clear();++frames;
            QMetaObject::invokeMethod(this,[this,frame](){commitCapturedFrame(frame,false,false);},Qt::BlockingQueuedConnection);
            const double actualFps=elapsed.elapsed()>0?1000.0*double(frames)/double(elapsed.elapsed()):0.0;
            ctx.reportProgress(0.0,"live",{{"frames",frames},{"actualFps",actualFps},{"targetFps",r.targetFps},{"frameId",frame.id},{"transport","repeated-capture"}});
            qint64 sleepMs=targetPeriodMs-cycle.elapsed();while(sleepMs>0&&!ctx.isCancellationRequested()){const int slice=int(std::min<qint64>(sleepMs,25));QThread::msleep(slice);sleepMs-=slice;}
        }
        closeSer();out.cancelled=true;return out;
    });
    emit logMessage(QString("Live View operation accepted: %1 — %2 s, gain %3, offset %4, target %5 fps, debayer=%6 %7").arg(id).arg(r.exposureSec,0,'g',5).arg(r.gain).arg(r.offset).arg(r.targetFps,0,'g',3).arg(r.debayer?"ON":"OFF").arg(bayerPatternName(r.bayerPattern))+(r.recordSer?QString(" SER=%1").arg(r.serPath):QString()));return id;
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
bool ApplicationController::slewMount(const EquatorialCoord&t,QString*e){
    if(!ensureResourcesAvailable({"mount"},e))return false;
    if(!mount_){if(e)*e="No mount connected";return false;}
    if(!ensureMountBackendSiteTime(e)){emit logMessage("Mount GOTO rejected before slew: "+(e?*e:QString()));return false;}
    const QDateTime utc=QDateTime::currentDateTimeUtc();
    const auto target=convertEquatorialFrame(t,EquatorialFrame::J2000,utc);
    const auto targetJNow=convertEquatorialFrame(target,EquatorialFrame::JNow,utc);
    const auto targetHor=equatorialToHorizontal(target,profile_.observer,utc);
    MountStatus before;QString statusError;
    if(mountStatus(before,&statusError)&&before.parked){QString ue;if(!mount_->park(false,&ue)){if(e)*e="Mount is parked and automatic unpark failed: "+ue;emit logMessage("Mount GOTO rejected: "+(e?*e:QString()));return false;}emit logMessage("Mount auto-unparked for GOTO");before={};statusError.clear();}
    if(mountStatus(before,&statusError)&&before.coordinateValid){
        double dra=target.raDeg-before.coordinate.raDeg;while(dra>180.0)dra-=360.0;while(dra<-180.0)dra+=360.0;
        const double ddec=target.decDeg-before.coordinate.decDeg;
        const double skySep=skyAngularSeparationDeg(before.coordinate,target);
        const auto currentJNow=convertEquatorialFrame(before.coordinate,EquatorialFrame::JNow,utc);
        const auto currentHor=equatorialToHorizontal(before.coordinate,profile_.observer,utc);
        QString pierError;const QString destinationPier=mount_->destinationPierSide(target,&pierError);
        emit logMessage(QString("Mount GOTO preflight: backend=%1 UTC=%2 site=(%3,%4,%5m) LST=%6deg | current J2000=(%7,%8) JNow=(%9,%10) AzAlt=(%11,%12) pier=%13 tracking=%14 | target J2000=(%15,%16) JNow=(%17,%18) AzAlt=(%19,%20) | short dRA=%21 dDEC=%22 skySeparation=%23deg%24")
            .arg(mount_->backendName(),utc.toString(Qt::ISODateWithMs)).arg(profile_.observer.latitudeDeg,0,'f',6).arg(profile_.observer.longitudeDeg,0,'f',6).arg(profile_.observer.elevationM,0,'f',1)
            .arg(localSiderealTimeDeg(profile_.observer,utc),0,'f',6)
            .arg(before.coordinate.raDeg,0,'f',6).arg(before.coordinate.decDeg,0,'f',6).arg(currentJNow.raDeg,0,'f',6).arg(currentJNow.decDeg,0,'f',6).arg(currentHor.azDeg,0,'f',6).arg(currentHor.altDeg,0,'f',6)
            .arg(before.pierSide).arg(before.tracking?"ON":"OFF")
            .arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6).arg(targetJNow.raDeg,0,'f',6).arg(targetJNow.decDeg,0,'f',6).arg(targetHor.azDeg,0,'f',6).arg(targetHor.altDeg,0,'f',6)
            .arg(dra,0,'f',6).arg(ddec,0,'f',6).arg(skySep,0,'f',6).arg(destinationPier.isEmpty()?QString():QString(" destinationPier=%1").arg(destinationPier)));
        if(before.axes.valid)emit logMessage(QString("Mount GOTO native preflight: current axis1=%1deg axis2=%2deg skySafetyLimit=%3deg rawMechanicalHardCap=180deg axisSigns=(%4,%5) preferredPier=%6")
            .arg(before.axes.axis1Deg,0,'f',6).arg(before.axes.axis2Deg,0,'f',6).arg(profile_.mount.maxGotoAxisDeltaDeg,0,'f',3).arg(profile_.mount.axis1Sign).arg(profile_.mount.axis2Sign).arg(profile_.mount.preferredPierSide));
        if(!before.diagnostics.isEmpty())emit logMessage("Mount backend preflight diagnostics: "+compactJson(before.diagnostics));
        if(!pierError.isEmpty())emit logMessage("Mount DestinationSideOfPier unavailable: "+pierError);
    }else if(!statusError.isEmpty())emit logMessage("Mount GOTO preflight status unavailable: "+statusError);
    bool ok=mount_->slewTo(target,e);
    if(ok)emit logMessage(QString("Mount slew accepted: RA=%1deg DEC=%2deg J2000 backend=%3").arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6).arg(mount_->backendName()));
    else emit logMessage(QString("Mount slew REJECTED/FAILED: backend=%1 target=(%2,%3) error=%4").arg(mount_->backendName()).arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6).arg(e?*e:QString()));
    emitState();return ok;
}
QString ApplicationController::startMountSlew(const EquatorialCoord&t,QString*e){
    if(!mount_){if(e)*e="No mount connected";return{};}
    if(!ensureMountBackendSiteTime(e))return{};
    {MountStatus st;QString se;if(mountStatus(st,&se)&&st.parked){if(!mount_->park(false,&se)){if(e)*e="Mount is parked and automatic unpark failed: "+se;return{};}emit logMessage("Mount auto-unparked for GOTO operation");}}
    const auto target=convertEquatorialFrame(t,EquatorialFrame::J2000);auto mount=mount_;const bool ascomDiagnostics=mount->backendName()=="ascom-classic";
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
bool ApplicationController::syncMount(const EquatorialCoord&t,QString*e){
    if(!ensureResourcesAvailable({"mount"},e))return false;
    if(!mount_){if(e)*e="No mount connected";return false;}
    const QDateTime utc=QDateTime::currentDateTimeUtc();const auto target=convertEquatorialFrame(t,EquatorialFrame::J2000,utc);
    if(std::abs(target.decDeg)>80.0)emit logMessage("SYNC WARNING: target is within 10deg of the celestial pole. A single near-pole Sync is a weak RA/axis-1 calibration anchor; prefer a plate-solved star field farther from the pole before trusting long native raw-axis GOTO.");
    MountStatus before;if(mountStatus(before,nullptr))emit logMessage("Mount diagnostic BEFORE SYNC: "+mountDiagnosticSnapshot(mount_->backendName(),before,profile_.observer,utc));
    bool ok=mount_->syncTo(target,e);
    if(ok){
        emit logMessage(QString("Mount sync accepted: RA=%1deg DEC=%2deg J2000 UTC=%3 LST=%4deg site=(%5,%6,%7m)")
            .arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6).arg(utc.toString(Qt::ISODateWithMs)).arg(localSiderealTimeDeg(profile_.observer,utc),0,'f',6)
            .arg(profile_.observer.latitudeDeg,0,'f',6).arg(profile_.observer.longitudeDeg,0,'f',6).arg(profile_.observer.elevationM,0,'f',1));
        MountStatus after;if(mountStatus(after,nullptr))emit logMessage("Mount diagnostic AFTER SYNC: "+mountDiagnosticSnapshot(mount_->backendName(),after,profile_.observer,QDateTime::currentDateTimeUtc()));
    }else emit logMessage(QString("Mount sync FAILED: backend=%1 target=(%2,%3) error=%4").arg(mount_->backendName()).arg(target.raDeg,0,'f',6).arg(target.decDeg,0,'f',6).arg(e?*e:QString()));
    emitState();return ok;
}
bool ApplicationController::mountStatus(MountStatus&s,QString*e)const{if(!mount_){if(e)*e="No mount connected";return false;}if(!mount_->status(s,e))return false;if(s.coordinateValid)s.coordinate=convertEquatorialFrame(s.coordinate,EquatorialFrame::J2000);return true;}
bool ApplicationController::focuserStatus(FocuserStatus&s,QString*e)const{if(!focuser_){if(e)*e="No focuser connected";return false;}return focuser_->status(s,e);}
bool ApplicationController::moveFocuser(int p,QString*e){if(!ensureResourcesAvailable({"focuser"},e))return false;if(!focuser_){if(e)*e="No focuser connected";return false;}bool ok=focuser_->moveAbsolute(p,e);emitState();return ok;}
bool ApplicationController::haltFocuser(QString*e){QString owner;if(operations_.isResourceLocked("focuser",&owner))operations_.cancel(owner,nullptr);if(!focuser_){if(e)*e="No focuser connected";return false;}bool ok=focuser_->halt(e);if(ok)emit logMessage("Focuser halt requested");return ok;}
bool ApplicationController::setMountTracking(bool v,TrackingRate rate,QString*e){
    if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}
    MountStatus before;const bool haveBefore=mountStatus(before,nullptr);
    bool ok=mount_->setTracking(v,rate,e);
    if(ok){
        emit logMessage(QString("Mount tracking %1 rate=%2").arg(v?"ON":"OFF",trackingRateName(rate)));
        if(v&&haveBefore){
            const auto baseline=before;const QString backend=mount_->backendName();
            QTimer::singleShot(3000,this,[this,baseline,backend,rate](){
                if(!mount_||mount_->backendName()!=backend)return;MountStatus after;QString err;if(!mountStatus(after,&err)){emit logMessage("Tracking diagnostic status failed: "+err);return;}
                double dra=after.coordinate.raDeg-baseline.coordinate.raDeg;while(dra>180)dra-=360;while(dra<-180)dra+=360;
                const double ddec=after.coordinate.decDeg-baseline.coordinate.decDeg;
                QString axes;if(baseline.axes.valid&&after.axes.valid)axes=QString(" axis1Δ=%1° axis2Δ=%2°").arg(after.axes.axis1Deg-baseline.axes.axis1Deg,0,'f',6).arg(after.axes.axis2Deg-baseline.axes.axis2Deg,0,'f',6);
                emit logMessage(QString("Tracking diagnostic +3s [%1/%2]: RAΔ=%3° DECΔ=%4°%5 tracking=%6")
                    .arg(backend,trackingRateName(rate)).arg(dra,0,'f',6).arg(ddec,0,'f',6).arg(axes).arg(after.tracking?"ON":"OFF"));
            });
        }
    }
    emitState();return ok;
}
bool ApplicationController::setMountSiteTime(const ObserverLocation &site,const QDateTime &utc,QString*e){
    if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}
    if(!mount_->setSiteTime(site,utc.toUTC(),e)){emit logMessage(QString("Mount backend site/time apply failed: backend=%1 site=(%2,%3,%4m) UTC=%5 error=%6").arg(mount_->backendName()).arg(site.latitudeDeg,0,'f',6).arg(site.longitudeDeg,0,'f',6).arg(site.elevationM,0,'f',1).arg(utc.toUTC().toString(Qt::ISODateWithMs),e?*e:QString()));return false;}
    profile_.observer=site;settings_.saveProfile(profile_);emit profileChanged();
    emit logMessage(QString("Mount/Core site/time applied: backend=%1 site=(%2,%3,%4m) UTC=%5").arg(mount_->backendName()).arg(site.latitudeDeg,0,'f',6).arg(site.longitudeDeg,0,'f',6).arg(site.elevationM,0,'f',1).arg(utc.toUTC().toString(Qt::ISODateWithMs)));
    MountStatus st;if(mountStatus(st,nullptr))emit logMessage("Mount diagnostic AFTER SITE/TIME: "+mountDiagnosticSnapshot(mount_->backendName(),st,site,QDateTime::currentDateTimeUtc()));emitState();return true;
}
bool ApplicationController::parkMount(bool v,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}bool ok=mount_->park(v,e);if(ok)emit logMessage(v?"Mount parking slew started":"Mount unpark/parking-stop accepted");emitState();return ok;}
bool ApplicationController::setCurrentMountAsPark(QString*e){
    if(!ensureResourcesAvailable({"mount"},e))return false;
    if(!mount_){if(e)*e="No mount connected";return false;}
    MountStatus st;QString se;if(!mount_->status(st,&se)){if(e)*e=se;return false;}
    if(st.axes.valid&&mount_->backendName().startsWith("native:")){
        auto p=profile_;
        p.mount.homeAxis1Deg=st.axes.axis1Deg;p.mount.homeAxis2Deg=st.axes.axis2Deg;p.mount.customHome=true;p.mount.autoHomeSync=true;
        p.mount.parkAxis1Deg=st.axes.axis1Deg;p.mount.parkAxis2Deg=st.axes.axis2Deg;p.mount.customPark=true;
        setProfile(p);
        emit logMessage(QString("Shared physical Home/Park calibrated for native mount: axis1=%1deg axis2=%2deg. This persists across restarts.").arg(st.axes.axis1Deg,0,'f',6).arg(st.axes.axis2Deg,0,'f',6));
        if(e)e->clear();return true;
    }
    if(!mount_->setCurrentParkPosition(e))return false;
    emit logMessage("Persistent park position stored by active mount backend. For identical native/ASCOM park, run this once in each backend without moving the telescope between calibrations.");
    return true;
}
bool ApplicationController::pulseGuide(GuideDirection d,int ms,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}return mount_->pulseGuide(d,ms,e);}
bool ApplicationController::manualMountSlew(int a1,int a2,int rate,QString*e){if(!ensureResourcesAvailable({"mount"},e))return false;if(!mount_){if(e)*e="No mount connected";return false;}const bool ok=mount_->manualSlew(std::clamp(a1,-1,1),std::clamp(a2,-1,1),std::clamp(rate,0,9),e);if(ok){emit logMessage(QString("Manual mount slew: axis1=%1 axis2=%2 rate=%3").arg(a1).arg(a2).arg(rate));emitState();}return ok;}
GuidingStatus ApplicationController::startGuiding(){if(lastSolve_.success)guiding_.setTarget({lastSolve_.raDeg,lastSolve_.decDeg});auto s=guiding_.status();auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
GuidingStatus ApplicationController::stopGuiding(){guiding_.stop();auto s=guiding_.status();auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
GuidingStatus ApplicationController::guideUsingLastSolve(){QString busy;if(!ensureResourcesAvailable({"mount"},&busy)){emit logMessage("Guide update skipped: "+busy);return guiding_.status();}auto s=guiding_.update({lastSolve_.raDeg,lastSolve_.decDeg},mount_.get());auto j=QJsonObject{{"active",s.active},{"raErrorArcsec",s.raErrorArcsec},{"decErrorArcsec",s.decErrorArcsec},{"rmsArcsec",s.rmsArcsec}};emit guidingChanged(j);if(oalWsServer_)oalWsServer_->broadcast("guidingUpdate",j);return s;}
void ApplicationController::clearPolarSamples(){polarSamples_.clear();emit polarSampleCountChanged(0);if(oalWsServer_)oalWsServer_->broadcast("polarSampleCount",QJsonObject{{"count",0}});}
bool ApplicationController::addPolarSample(QString*e){if(!lastSolve_.success){if(e)*e="Last plate solve is invalid";return false;}MountStatus m;if(!mountStatus(m,e))return false;polarSamples_.push_back({lastSolve_,m.coordinate.raDeg});int n=int(polarSamples_.size());emit polarSampleCountChanged(n);if(oalWsServer_)oalWsServer_->broadcast("polarSampleCount",QJsonObject{{"count",n}});return true;}
bool ApplicationController::slewPolarRaOffset(double d,QString*e){MountStatus m;if(!mountStatus(m,e))return false;EquatorialCoord t=m.coordinate;t.raDeg=std::fmod(t.raDeg+d+360.0,360.0);return slewMount(t,e);}
PolarAlignmentResult ApplicationController::estimatePolarAlignment(){return polarEstimator_.estimate(polarSamples_,profile_.observer,QDateTime::currentDateTimeUtc());}
bool ApplicationController::setObservationPlan(const ObservationPlan&plan,QString*e){
    if(plan.blocks.empty()){if(e)*e="Observation plan has no blocks";return false;}
    if(scheduler_.status().active){if(e)*e="Cannot replace an active observation plan";return false;}
    for(const auto &block:plan.blocks){
        if(block.name.trimmed().isEmpty()){if(e)*e="Observation block name is required";return false;}
        if(block.coordinate.decDeg < -90.0 || block.coordinate.decDeg > 90.0){if(e)*e="Observation block DEC is outside [-90,+90]";return false;}
        if(block.mode==ObservationMode::DsoFits){
            if(block.dso.frameCount<1){if(e)*e="DSO block frameCount must be >= 1";return false;}
            if(block.dso.exposure.exposureSec<=0.0){if(e)*e="DSO exposure must be > 0";return false;}
        }else{
            if(block.planetary.serRuns<1){if(e)*e="Planetary block serRuns must be >= 1";return false;}
            if(block.planetary.durationSec<=0.0){if(e)*e="Planetary SER duration must be > 0";return false;}
            if(block.planetary.stream.exposureSec<=0.0){if(e)*e="Planetary stream exposure must be > 0";return false;}
            if(block.planetary.roiWidth<0||block.planetary.roiHeight<0){if(e)*e="Planetary ROI dimensions cannot be negative";return false;}
        }
    }
    scheduler_.setPlan(plan);
    emit logMessage(QString("Observation plan loaded: %1 (%2 block(s))").arg(plan.name).arg(plan.blocks.size()));
    return true;
}
bool ApplicationController::setSessionPlan(const QString&name,const std::vector<SessionTarget>&targets,QString*e){
    if(targets.empty()){if(e)*e="No targets supplied";return false;}
    ObservationPlan plan;plan.name=name;plan.blocks.reserve(targets.size());
    for(int i=0;i<int(targets.size());++i)plan.blocks.push_back(observationBlockFromLegacy(targets[size_t(i)],i));
    return setObservationPlan(plan,e);
}
bool ApplicationController::startSession(QString*e){
    if(!scheduler_.start()){if(e)*e="No observation blocks supplied or session already active";return false;}
    sessionRecenterAttempt_=0;planetaryRoi_={};planetaryLastCentroid_={};planetaryMountCalibration_={};
    emit logMessage(QString("Observation session started: %1").arg(scheduler_.status().id));
    QTimer::singleShot(0,this,[this](){scheduleSessionStep();});
    return true;
}
void ApplicationController::stopSession(){
    const auto st=scheduler_.status();
    scheduler_.stop("Stopped by operator");
    if(!st.currentOperationId.isEmpty())cancelOperation(st.currentOperationId,nullptr);
    emit logMessage("Observation session stopped by operator");
}

bool ApplicationController::sessionRecenterDue(const ObservationBlock&block)const{
    if(block.mode!=ObservationMode::DsoFits)return false;
    const int n=scheduler_.status().currentBlockCompletedFrames;
    if(n==0)return block.dso.recenter.beforeFirstFrame;
    return block.dso.recenter.everyNFrames>0 && (n%block.dso.recenter.everyNFrames)==0;
}
bool ApplicationController::sessionAutofocusDue(const ObservationBlock&block)const{
    if(block.mode!=ObservationMode::DsoFits)return false;
    const int n=scheduler_.status().currentBlockCompletedFrames;
    if(n==0)return block.dso.autofocus.beforeFirstFrame;
    return block.dso.autofocus.everyNFrames>0 && (n%block.dso.autofocus.everyNFrames)==0;
}
bool ApplicationController::planetaryAutofocusDue(const ObservationBlock&block)const{
    if(block.mode!=ObservationMode::PlanetarySer)return false;
    const int n=scheduler_.status().currentBlockCompletedFrames;
    if(n==0)return block.planetary.autofocus.beforeFirstRun;
    return block.planetary.autofocus.everyNRuns>0 && (n%block.planetary.autofocus.everyNRuns)==0;
}

void ApplicationController::startCurrentDsoSlew(){
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("No current observation block");return;}
    QString e;const QString id=startMountSlew(block->coordinate,&e);
    if(id.isEmpty()){scheduler_.fail("Scheduler slew could not start: "+e);return;}
    scheduler_.setStep("slew",id);
    emit logMessage(QString("Scheduler [%1] slew -> %2 RA=%3 DEC=%4").arg(block->id).arg(block->name).arg(block->coordinate.raDeg,0,'f',6).arg(block->coordinate.decDeg,0,'f',6));
}
void ApplicationController::startCurrentDsoSolve(){
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("No current observation block");return;}
    AdaptiveSolveRequest r;r.exposure.exposureSec=block->dso.recenter.solveExposureSec;r.exposure.saveRaw=false;r.useMountHint=true;
    r.hint.raDeg=block->coordinate.raDeg;r.hint.decDeg=block->coordinate.decDeg;r.hint.searchRadiusDeg=20.0;
    QString e;const QString id=startAdaptiveSolve(r,&e);
    if(id.isEmpty()){scheduler_.fail("Scheduler plate solve could not start: "+e);return;}
    scheduler_.setStep("solve",id);
}
void ApplicationController::startCurrentDsoAutofocus(){
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("No current observation block");return;}
    QString e;const QString id=startAutofocus(block->dso.autofocus.request,&e);
    if(id.isEmpty()){scheduler_.fail("Scheduler autofocus could not start: "+e);return;}
    scheduler_.setStep("autofocus",id);
}
void ApplicationController::startCurrentDsoCapture(){
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("No current observation block");return;}
    ExposureRequest r=block->dso.exposure;r.saveRaw=true;
    QString e;const QString id=startCapture(r,&e);
    if(id.isEmpty()){scheduler_.fail("Scheduler science exposure could not start: "+e);return;}
    scheduler_.setStep("capture",id);
    emit logMessage(QString("Scheduler [%1] science frame %2/%3: %4 s%5")
        .arg(block->id).arg(scheduler_.status().currentBlockCompletedFrames+1).arg(block->dso.frameCount)
        .arg(r.exposureSec,0,'g',8).arg(block->dso.filter.isEmpty()?QString():QString(" filter=%1 (metadata until filter-wheel executor lands)").arg(block->dso.filter)));
}

void ApplicationController::startCurrentPlanetarySlew(){
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("No current planetary block");return;}
    QString e;const QString id=startMountSlew(block->coordinate,&e);if(id.isEmpty()){scheduler_.fail("Planetary scheduler slew could not start: "+e);return;}
    scheduler_.setStep("planetary-slew",id);
    emit logMessage(QString("Planetary scheduler [%1] GOTO %2 before full-frame acquisition").arg(block->id,block->name));
}
void ApplicationController::startCurrentPlanetaryAcquire(const QString &step){
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("No current planetary block");return;}
    ExposureRequest r;r.exposureSec=block->planetary.stream.exposureSec;r.gain=block->planetary.stream.gain;r.offset=block->planetary.stream.offset;r.binX=block->planetary.stream.binX;r.binY=block->planetary.stream.binY;r.saveRaw=false;
    QString e;const QString id=startCapture(r,&e);if(id.isEmpty()){scheduler_.fail("Planetary full-frame acquisition could not start: "+e);return;}scheduler_.setStep(step,id);
}
void ApplicationController::startCurrentPlanetaryAutofocus(){
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("No current planetary block");return;}
    AutofocusRequest r=block->planetary.autofocus.request;r.mode=AutofocusMode::Planet;r.autoPlanetRoi=true;
    QString e;const QString id=startAutofocus(r,&e);if(id.isEmpty()){scheduler_.fail("Planetary autofocus could not start: "+e);return;}scheduler_.setStep("planetary-autofocus",id);
}
void ApplicationController::startCurrentPlanetaryCalibration(){
    const auto block=scheduler_.currentBlock();if(!block||!camera_||!mount_){scheduler_.fail("Planetary mount calibration requires camera and mount");return;}
    const auto cam=camera_;
    const auto mount=mount_;
    const auto target=convertEquatorialFrame(block->coordinate,EquatorialFrame::J2000);const double step=block->planetary.tracking.calibrationArcsec;
    ExposureRequest req;req.exposureSec=block->planetary.stream.exposureSec;req.gain=block->planetary.stream.gain;req.offset=block->planetary.stream.offset;req.binX=block->planetary.stream.binX;req.binY=block->planetary.stream.binY;req.saveRaw=false;
    const TrackingRate trackingRate=block->planetary.trackingRate;
    const QString id=operations_.submit("planetary.mount-calibration",{"camera","mount"},true,[this,cam,mount,target,step,req,trackingRate](OperationContext&ctx){
        OperationOutcome out;ThreadMarshalledCamera camera(this,cam);ThreadMarshalledMount mnt(this,mount);PlanetDetector detector;
        auto waitIdle=[&](QString &err){for(int i=0;i<600;++i){if(ctx.isCancellationRequested()){mnt.abortMotion(nullptr);return false;}MountStatus s;if(!mnt.status(s,&err))return false;if(!s.slewing)return true;QThread::msleep(100);}err="Mount micro-slew did not settle within 60 s";return false;};
        auto captureCentroid=[&](cv::Point2d &c,QString &err,const QString &purpose){CameraFrame f;if(!camera.capture(req,f,&err))return false;const auto d=detector.detect(f.image);if(!d.found){err="Planet detector lost the target during mount calibration";return false;}c=d.centroidPx;QMetaObject::invokeMethod(this,[this,f,purpose](){publishOperationalPreview(f,purpose);},Qt::BlockingQueuedConnection);return true;};
        QString err;cv::Point2d base,raPt,decPt;ctx.reportProgress(0.05,"planetary.calibration.base");if(!captureCentroid(base,err,"planet-cal-base")){out.problem={{"code","PLANET_NOT_FOUND"},{"message",err}};return out;}
        const double cosDec=std::max(0.1,std::abs(std::cos(target.decDeg*3.14159265358979323846/180.0)));
        EquatorialCoord raTarget=target;raTarget.raDeg=std::fmod(raTarget.raDeg+step/(3600.0*cosDec)+360.0,360.0);ctx.reportProgress(0.20,"planetary.calibration.ra");if(!mnt.slewTo(raTarget,&err)||!waitIdle(err)){out.problem={{"code","MOUNT_CALIBRATION_RA_FAILED"},{"message",err}};return out;}QThread::msleep(350);if(!captureCentroid(raPt,err,"planet-cal-ra")){out.problem={{"code","PLANET_NOT_FOUND"},{"message",err}};return out;}
        if(!mnt.slewTo(target,&err)||!waitIdle(err)){out.problem={{"code","MOUNT_CALIBRATION_RETURN_FAILED"},{"message",err}};return out;}QThread::msleep(350);
        EquatorialCoord decTarget=target;decTarget.decDeg=std::clamp(decTarget.decDeg+step/3600.0,-89.9,89.9);ctx.reportProgress(0.55,"planetary.calibration.dec");if(!mnt.slewTo(decTarget,&err)||!waitIdle(err)){out.problem={{"code","MOUNT_CALIBRATION_DEC_FAILED"},{"message",err}};return out;}QThread::msleep(350);if(!captureCentroid(decPt,err,"planet-cal-dec")){out.problem={{"code","PLANET_NOT_FOUND"},{"message",err}};return out;}
        if(!mnt.slewTo(target,&err)||!waitIdle(err)){out.problem={{"code","MOUNT_CALIBRATION_RETURN_FAILED"},{"message",err}};return out;}mnt.setTracking(true,trackingRate,nullptr);QThread::msleep(350);
        const double a=(raPt.x-base.x)/step,c=(raPt.y-base.y)/step,b=(decPt.x-base.x)/step,d=(decPt.y-base.y)/step,det=a*d-b*c;
        if(std::abs(det)<1e-5){out.problem={{"code","MOUNT_CALIBRATION_DEGENERATE"},{"message","Planetary RA/DEC calibration vectors are degenerate; mount correction will not be safe"}};return out;}
        out.success=true;out.result=QJsonObject{{"m00",a},{"m01",b},{"m10",c},{"m11",d},{"determinant",det},{"calibrationArcsec",step}};ctx.reportProgress(1.0,"completed");return out;
    });
    scheduler_.setStep("planetary-calibration",id);
}
void ApplicationController::startCurrentPlanetarySer(){
    const auto block=scheduler_.currentBlock();if(!block||!camera_){scheduler_.fail("Planetary SER requires the main camera");return;}
    auto cam=camera_;auto mount=mount_;const int runIndex=scheduler_.status().currentBlockCompletedFrames;LiveViewRequest request=block->planetary.stream;request.recordSer=true;request.roi=planetaryRoi_;request.serPath=planetarySerPath(*block,runIndex,request.serPath);
    const QSize nativeSensor=cam->sensorSize();const int sensorW=std::max(1,nativeSensor.width()/std::max(1,request.binX)),sensorH=std::max(1,nativeSensor.height()/std::max(1,request.binY));
    const auto tracking=block->planetary.tracking;const auto calibration=planetaryMountCalibration_;const TrackingRate trackingRate=block->planetary.trackingRate;const double duration=block->planetary.durationSec;const TelescopeProfile profile=profile_;const QString backend=cam->backendName(),cameraName=cam->displayName();
    QStringList resources{"camera"};if(tracking.mountCorrections&&mount)resources<<"mount";
    const QString id=operations_.submit("camera.planetary-ser",resources,true,[this,cam,mount,request,tracking,calibration,trackingRate,duration,profile,backend,cameraName,sensorW,sensorH](OperationContext&ctx)mutable{
        OperationOutcome out;ThreadMarshalledCamera camera(this,cam);std::unique_ptr<ThreadMarshalledMount> mnt;if(mount)mnt=std::make_unique<ThreadMarshalledMount>(this,mount);PlanetDetector detector;SerWriter ser;QElapsedTimer elapsed;elapsed.start();int lost=0;quint32 frames=0;cv::Rect roi=request.roi;int lastMountFrame=-100000;
        auto native=std::dynamic_pointer_cast<NativeOalCamera>(cam);const bool nativeStream=native&&native->nativeLiveSupported();bool streamStarted=false;
        auto startStream=[&](QString &err){if(!nativeStream)return true;request.roi=roi;if(!native->startNativeLive(request,&err))return false;streamStarted=true;return true;};
        auto stopStream=[&](){if(nativeStream&&streamStarted){QString e;native->stopNativeLive(&e);streamStarted=false;if(!e.isEmpty())QMetaObject::invokeMethod(this,[this,e](){emit logMessage("Planetary stream restart warning: "+e);},Qt::QueuedConnection);}};
        auto waitMountIdle=[&](QString &err){if(!mnt)return false;for(int i=0;i<600;++i){if(ctx.isCancellationRequested()){mnt->abortMotion(nullptr);return false;}MountStatus s;if(!mnt->status(s,&err))return false;if(!s.slewing)return true;QThread::msleep(100);}err="Planetary correction slew timeout";return false;};
        QString err;if(!startStream(err)){out.problem={{"code","PLANETARY_STREAM_START_FAILED"},{"message",err}};return out;}
        const qint64 targetPeriodMs=qint64(std::lround(1000.0/std::clamp(request.targetFps,0.2,200.0)));
        while(!ctx.isCancellationRequested()&&elapsed.elapsed()<qint64(duration*1000.0)){
            QElapsedTimer cycle;cycle.start();CameraFrame raw;
            if(nativeStream){const int timeoutMs=int(std::clamp<qint64>(qint64(std::ceil(request.exposureSec*3000.0))+1000,1000,5000));if(!native->nextNativeLiveFrame(raw,timeoutMs,&err)){stopStream();ser.close(nullptr);out.problem={{"code","PLANETARY_FRAME_FAILED"},{"message",err}};return out;}}
            else {ExposureRequest er;er.exposureSec=request.exposureSec;er.gain=request.gain;er.offset=request.offset;er.binX=request.binX;er.binY=request.binY;er.roi=roi;er.saveRaw=false;if(!camera.capture(er,raw,&err)){ser.close(nullptr);out.problem={{"code","PLANETARY_FRAME_FAILED"},{"message",err}};return out;}}
            if(!ser.isOpen()){request.roi=roi;if(!ser.open(request.serPath,raw,profile,request,backend,cameraName,&err)){stopStream();out.problem={{"code","SER_WRITE_FAILED"},{"message",err}};return out;}}
            if(!ser.append(raw,&err)){stopStream();ser.close(nullptr);out.problem={{"code","SER_WRITE_FAILED"},{"message",err}};return out;}frames=ser.frameCount();
            const auto det=detector.detect(raw.image);if(!det.found){++lost;if(lost>=tracking.lostTargetFrames){stopStream();ser.close(nullptr);out.problem={{"code","PLANET_LOST"},{"message",QString("Planet was not detected for %1 consecutive SER frames").arg(lost)}};return out;}}
            else{
                lost=0;const cv::Point2d global{roi.x+det.centroidPx.x,roi.y+det.centroidPx.y};const cv::Point2d localError{det.centroidPx.x-0.5*roi.width,det.centroidPx.y-0.5*roi.height};
                cv::Rect proposed=roi;if(tracking.allowRoiShift&&std::hypot(localError.x,localError.y)>=tracking.roiShiftThresholdPx)proposed=centeredPlanetaryRoi(global,roi.width,roi.height,sensorW,sensorH);
                const cv::Point2d sensorError{global.x-0.5*sensorW,global.y-0.5*sensorH};
                const bool mountDue=tracking.mountCorrections&&mnt&&calibration.valid&&std::hypot(sensorError.x,sensorError.y)>=tracking.mountCorrectionThresholdPx&&int(frames)-lastMountFrame>=std::max(10,int(request.targetFps*2.0));
                if(mountDue){
                    const double a=calibration.pixelsPerArcsec(0,0),b=calibration.pixelsPerArcsec(0,1),c=calibration.pixelsPerArcsec(1,0),d=calibration.pixelsPerArcsec(1,1),detm=a*d-b*c;
                    if(std::abs(detm)>1e-8){const double wantX=-sensorError.x,wantY=-sensorError.y;double raArc=(d*wantX-b*wantY)/detm,decArc=(-c*wantX+a*wantY)/detm;const double mag=std::hypot(raArc,decArc);if(mag>tracking.maxMountCorrectionArcsec){const double k=tracking.maxMountCorrectionArcsec/mag;raArc*=k;decArc*=k;}
                        stopStream();MountStatus ms;if(mnt->status(ms,&err)&&ms.coordinateValid){auto t=convertEquatorialFrame(ms.coordinate,EquatorialFrame::J2000);const double cosDec=std::max(0.1,std::abs(std::cos(t.decDeg*3.14159265358979323846/180.0)));t.raDeg=std::fmod(t.raDeg+raArc/(3600.0*cosDec)+360.0,360.0);t.decDeg=std::clamp(t.decDeg+decArc/3600.0,-89.9,89.9);if(mnt->slewTo(t,&err)&&waitMountIdle(err)){mnt->setTracking(true,trackingRate,nullptr);if(tracking.mountSettleMs>0)QThread::msleep(unsigned(tracking.mountSettleMs));roi=centeredPlanetaryRoi({0.5*sensorW,0.5*sensorH},roi.width,roi.height,sensorW,sensorH);QJsonObject extra{{"type","mount-correction"},{"raArcsec",raArc},{"decArcsec",decArc},{"sensorErrorX",sensorError.x},{"sensorErrorY",sensorError.y}};ser.appendRoiEvent(frames,roi,"mount-correction",extra,nullptr);lastMountFrame=int(frames);request.roi=roi;if(!startStream(err)){ser.close(nullptr);out.problem={{"code","PLANETARY_STREAM_RESTART_FAILED"},{"message",err}};return out;}}
                            else {QMetaObject::invokeMethod(this,[this,err](){emit logMessage("Planetary mount correction skipped after failure: "+err);},Qt::QueuedConnection);request.roi=roi;if(!startStream(err)){ser.close(nullptr);out.problem={{"code","PLANETARY_STREAM_RESTART_FAILED"},{"message",err}};return out;}}}
                        else {request.roi=roi;if(!startStream(err)){ser.close(nullptr);out.problem={{"code","PLANETARY_STREAM_RESTART_FAILED"},{"message",err}};return out;}}
                    }
                } else if(proposed!=roi){stopStream();roi=proposed;request.roi=roi;QJsonObject extra{{"type","roi-shift"},{"centroidGlobalX",global.x},{"centroidGlobalY",global.y}};ser.appendRoiEvent(frames,roi,"tracker-shift",extra,nullptr);if(!startStream(err)){ser.close(nullptr);out.problem={{"code","PLANETARY_STREAM_RESTART_FAILED"},{"message",err}};return out;}}
                ctx.reportProgress(std::clamp(double(elapsed.elapsed())/(duration*1000.0),0.0,0.99),"planetary.recording",{{"frames",int(frames)},{"centroidX",det.centroidPx.x},{"centroidY",det.centroidPx.y},{"roiX",roi.x},{"roiY",roi.y},{"roiWidth",roi.width},{"roiHeight",roi.height},{"serPath",request.serPath}});
            }
            CameraFrame preview=raw;QString note;processLivePreview(preview,request,&note);preview.id="live-"+preview.id;preview.scienceFilePath.clear();QMetaObject::invokeMethod(this,[this,preview](){commitCapturedFrame(preview,false,false);},Qt::BlockingQueuedConnection);
            if(!nativeStream){qint64 sleepMs=targetPeriodMs-cycle.elapsed();while(sleepMs>0&&!ctx.isCancellationRequested()){const int slice=int(std::min<qint64>(sleepMs,25));QThread::msleep(slice);sleepMs-=slice;}}
        }
        stopStream();if(ctx.isCancellationRequested()){ser.close(nullptr);out.cancelled=true;return out;}if(!ser.close(&err)){out.problem={{"code","SER_FINALIZE_FAILED"},{"message",err}};return out;}
        out.success=true;out.result=QJsonObject{{"serPath",request.serPath},{"metadataPath",QFileInfo(request.serPath).absolutePath()+"/"+QFileInfo(request.serPath).completeBaseName()+".txt"},{"roiProvenancePath",QFileInfo(request.serPath).absolutePath()+"/"+QFileInfo(request.serPath).completeBaseName()+".roi.jsonl"},{"frames",int(frames)},{"finalRoi",QJsonObject{{"x",roi.x},{"y",roi.y},{"width",roi.width},{"height",roi.height}}}};ctx.reportProgress(1.0,"completed");return out;
    });
    scheduler_.setStep("planetary-ser",id);emit logMessage(QString("Planetary SER run %1/%2 started: %3 ROI=(%4,%5 %6x%7)").arg(runIndex+1).arg(block->planetary.serRuns).arg(request.serPath).arg(request.roi.x).arg(request.roi.y).arg(request.roi.width).arg(request.roi.height));
}

void ApplicationController::scheduleSessionStep(){
    const auto st=scheduler_.status();if(!st.active)return;
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("Observation plan cursor is outside the block list");return;}
    if(st.currentStep=="prepare-block"){
        sessionRecenterAttempt_=0;
        if(block->mode==ObservationMode::PlanetarySer){planetaryRoi_={};planetaryLastCentroid_={};planetaryMountCalibration_={};startCurrentPlanetarySlew();}
        else startCurrentDsoSlew();
        return;
    }
    if(block->mode==ObservationMode::PlanetarySer){
        if(st.currentStep=="planetary-acquire-pending"){startCurrentPlanetaryAcquire("planetary-acquire");return;}
        if(st.currentStep=="planetary-reacquire-pending"){startCurrentPlanetaryAcquire("planetary-reacquire");return;}
        if(st.currentStep=="planetary-postcal-acquire-pending"){startCurrentPlanetaryAcquire("planetary-postcal-acquire");return;}
        if(st.currentStep=="planetary-autofocus-pending"){startCurrentPlanetaryAutofocus();return;}
        if(st.currentStep=="planetary-calibration-pending"){startCurrentPlanetaryCalibration();return;}
        if(st.currentStep=="planetary-ser-pending"){startCurrentPlanetarySer();return;}
        return;
    }
    if(st.currentStep=="solve-pending"){startCurrentDsoSolve();return;}
    if(st.currentStep=="autofocus-pending"){startCurrentDsoAutofocus();return;}
    if(st.currentStep=="capture-pending"){startCurrentDsoCapture();return;}
}

void ApplicationController::handleSessionOperationUpdate(const QJsonObject&o){
    const auto st=scheduler_.status();if(!st.active||st.currentOperationId.isEmpty())return;if(o.value("id").toString()!=st.currentOperationId)return;
    const QString state=o.value("state").toString();if(state!="succeeded"&&state!="failed"&&state!="cancelled")return;
    const auto block=scheduler_.currentBlock();if(!block){scheduler_.fail("Observation block disappeared while an operation was running");return;}const QString step=st.currentStep;
    if(state!="succeeded"){const QString message=state=="cancelled"?QString("Scheduler step %1 was cancelled").arg(step):o.value("problem").toObject().value("message").toString(QString("Scheduler step %1 failed").arg(step));scheduler_.fail(message);emit logMessage("Observation session FAILED: "+message);return;}
    scheduler_.clearOperation();

    if(block->mode==ObservationMode::PlanetarySer){
        if(step=="planetary-slew"){
            QString te;if(!setMountTracking(true,block->planetary.trackingRate,&te))emit logMessage("Planetary tracking could not be enabled after GOTO: "+te);
            scheduler_.setStep("planetary-acquire-pending");QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;
        }
        if(step=="planetary-acquire"||step=="planetary-reacquire"||step=="planetary-postcal-acquire"){
            const auto d=planetDetector_.detect(lastFrame_.image);if(!d.found){scheduler_.fail("Planet was not detected in the full-frame acquisition after GOTO");return;}planetaryLastCentroid_=d.centroidPx;
            const int w=block->planetary.roiWidth>0?block->planetary.roiWidth:640,h=block->planetary.roiHeight>0?block->planetary.roiHeight:480;planetaryRoi_=centeredPlanetaryRoi(d.centroidPx,w,h,lastFrame_.image.cols,lastFrame_.image.rows);
            emit logMessage(QString("Planet acquired at (%1,%2), confidence=%3; hardware ROI=(%4,%5 %6x%7)").arg(d.centroidPx.x,0,'f',1).arg(d.centroidPx.y,0,'f',1).arg(d.confidence,0,'f',3).arg(planetaryRoi_.x).arg(planetaryRoi_.y).arg(planetaryRoi_.width).arg(planetaryRoi_.height));
            if(step=="planetary-acquire"&&planetaryAutofocusDue(*block)){scheduler_.setStep("planetary-autofocus-pending");}
            else if(block->planetary.tracking.mountCorrections&&block->planetary.tracking.autoCalibrateMount&&!planetaryMountCalibration_.valid&&step!="planetary-postcal-acquire")scheduler_.setStep("planetary-calibration-pending");
            else scheduler_.setStep("planetary-ser-pending");
            QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;
        }
        if(step=="planetary-autofocus"){
            scheduler_.setStep("planetary-reacquire-pending");QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;
        }
        if(step=="planetary-calibration"){
            const auto r=o.value("result").toObject();planetaryMountCalibration_.pixelsPerArcsec=cv::Matx22d(r.value("m00").toDouble(),r.value("m01").toDouble(),r.value("m10").toDouble(),r.value("m11").toDouble());planetaryMountCalibration_.valid=true;
            emit logMessage(QString("Planetary mount image-response calibration ready: [[%1,%2],[%3,%4]] px/arcsec").arg(planetaryMountCalibration_.pixelsPerArcsec(0,0),0,'g',6).arg(planetaryMountCalibration_.pixelsPerArcsec(0,1),0,'g',6).arg(planetaryMountCalibration_.pixelsPerArcsec(1,0),0,'g',6).arg(planetaryMountCalibration_.pixelsPerArcsec(1,1),0,'g',6));
            scheduler_.setStep("planetary-postcal-acquire-pending");QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;
        }
        if(step=="planetary-ser"){
            const auto r=o.value("result").toObject(),fr=r.value("finalRoi").toObject();if(!fr.isEmpty())planetaryRoi_=cv::Rect(fr.value("x").toInt(),fr.value("y").toInt(),fr.value("width").toInt(),fr.value("height").toInt());scheduler_.markFrameCompleted();const auto after=scheduler_.status();
            emit logMessage(QString("Planetary SER completed: %1 (%2 frames), ROI provenance=%3").arg(r.value("serPath").toString()).arg(r.value("frames").toInt()).arg(r.value("roiProvenancePath").toString()));
            if(after.currentBlockCompletedFrames>=block->planetary.serRuns){scheduler_.advanceBlock();QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;}
            scheduler_.setStep("planetary-acquire-pending");const int delay=int(std::lround(block->planetary.pauseSec*1000.0));QTimer::singleShot(std::max(0,delay),this,[this](){scheduleSessionStep();});return;
        }
        return;
    }

    if(step=="slew"){
        sessionRecenterAttempt_=0;if(sessionRecenterDue(*block))scheduler_.setStep("solve-pending");else if(sessionAutofocusDue(*block))scheduler_.setStep("autofocus-pending");else scheduler_.setStep("capture-pending");QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;
    }
    if(step=="recenter-slew"){scheduler_.setStep("solve-pending");QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;}
    if(step=="solve"){
        const auto result=o.value("result").toObject();EquatorialCoord solved;solved.raDeg=result.value("raDeg").toDouble();solved.decDeg=result.value("decDeg").toDouble();solved.frame=equatorialFrameFromString(result.value("coordinateFrame").toString("J2000"));const auto solvedJ2000=convertEquatorialFrame(solved,EquatorialFrame::J2000);const auto targetJ2000=convertEquatorialFrame(block->coordinate,EquatorialFrame::J2000);const double errorArcmin=60.0*skyAngularSeparationDeg(solvedJ2000,targetJ2000);emit logMessage(QString("Scheduler solve/recenter: block=%1 pointing error=%2 arcmin attempt=%3/%4").arg(block->id).arg(errorArcmin,0,'f',3).arg(sessionRecenterAttempt_).arg(block->dso.recenter.maxAttempts));
        if(errorArcmin>block->dso.recenter.toleranceArcmin){if(sessionRecenterAttempt_>=block->dso.recenter.maxAttempts){scheduler_.fail(QString("Recenter did not converge: %1 arcmin > %2 arcmin tolerance").arg(errorArcmin,0,'f',3).arg(block->dso.recenter.toleranceArcmin,0,'f',3));return;}QString e;if(!syncMount(solvedJ2000,&e)){scheduler_.fail("Plate-solve Sync failed before recenter: "+e);return;}++sessionRecenterAttempt_;QString e2;const QString id=startMountSlew(targetJ2000,&e2);if(id.isEmpty()){scheduler_.fail("Recenter slew could not start: "+e2);return;}scheduler_.setStep("recenter-slew",id);return;}
        sessionRecenterAttempt_=0;if(sessionAutofocusDue(*block))scheduler_.setStep("autofocus-pending");else scheduler_.setStep("capture-pending");QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;
    }
    if(step=="autofocus"){scheduler_.setStep("capture-pending");QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;}
    if(step=="capture"){
        scheduler_.markFrameCompleted();const auto after=scheduler_.status();if(after.currentBlockCompletedFrames>=block->dso.frameCount){emit logMessage(QString("Scheduler block completed: %1 (%2 science frame(s))").arg(block->name).arg(after.currentBlockCompletedFrames));scheduler_.advanceBlock();QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;}sessionRecenterAttempt_=0;if(sessionRecenterDue(*block))scheduler_.setStep("solve-pending");else if(sessionAutofocusDue(*block))scheduler_.setStep("autofocus-pending");else scheduler_.setStep("capture-pending");QTimer::singleShot(0,this,[this](){scheduleSessionStep();});return;
    }
}
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
QJsonArray ApplicationController::devicesJson()const{
    QJsonArray a;
    auto add=[&](const std::shared_ptr<IDevice>&d,const QString&type,const QString&role,const DeviceBinding&binding){
        if(d){const QString backend=d->backendName();a.append(QJsonObject{{"id",d->id()},{"type",type},{"role",role},{"name",d->displayName()},{"backend",backend},{"endpoint",binding.endpoint},{"connected",d->connectionState()==ConnectionState::Connected},{"nativeOal",backend.startsWith("native:")},{"implemented",true}});}
    };
    add(camera_,"camera","main",settings_.cameraBinding());add(guideCamera_,"camera","guide",settings_.guideCameraBinding());add(mount_,"mount","main",settings_.mountBinding());add(focuser_,"focuser","main",settings_.focuserBinding());
    // Reserve first-class OAL device categories now so API/UI clients can build
    // stable observatory layouts before hardware-specific drivers land. These
    // entries are intentionally inert: no fake connect/control semantics.
    const struct {const char*id;const char*type;const char*name;} stubs[]={
        {"stub-filter-wheel","filter-wheel","Filter wheel"},
        {"stub-rotator","rotator","Camera/field rotator"},
        {"stub-dome","dome","Dome / roll-off roof"},
        {"stub-weather","weather","Weather station"},
        {"stub-gps","gps","GPS / GNSS receiver"},
        {"stub-power","power","Power distribution / switch"},
        {"stub-cover-calibrator","cover-calibrator","Cover / flat-field calibrator"},
        {"stub-safety-monitor","safety-monitor","Observatory safety monitor"}
    };
    for(const auto&s:stubs)a.append(QJsonObject{{"id",QString::fromLatin1(s.id)},{"type",QString::fromLatin1(s.type)},{"role","main"},{"name",QString::fromLatin1(s.name)},{"backend","stub"},{"endpoint",""},{"connected",false},{"nativeOal",false},{"implemented",false},{"status","placeholder"}});
    return a;
}
QJsonObject ApplicationController::cameraStatusJson()const{QJsonObject j{{"connected",bool(camera_)},{"backend",camera_?camera_->backendName():QString()},{"name",camera_?camera_->displayName():QString()}};if(camera_){auto s=camera_->sensorSize();j["width"]=s.width();j["height"]=s.height();j["canAbortExposure"]=camera_->canAbortExposure();}return j;}
bool ApplicationController::frameById(const QString&id,CameraFrame&frame,QString*error)const{if(!lastFrame_.image.empty()&&(id==lastFrame_.id||id=="latest")){frame=lastFrame_;return true;}if(!lastGuideFrame_.image.empty()&&(id==lastGuideFrame_.id||id=="latest-guide")){frame=lastGuideFrame_;return true;}for(auto it=previewFrameCache_.rbegin();it!=previewFrameCache_.rend();++it)if(it->id==id){frame=*it;return true;}if(!previousFrame_.image.empty()&&id==previousFrame_.id){frame=previousFrame_;return true;}if(error)*error="Frame is no longer available in the in-memory preview cache";return false;}
QJsonObject ApplicationController::stateJson()const{auto strings=[](const QStringList&xs){QJsonArray a;for(const auto&x:xs)a.append(x);return a;};QJsonObject j{{"timestampUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"devices",devicesJson()},{"backends",QJsonObject{{"camera",strings(cameraBackends())},{"mount",strings(mountBackends())},{"focuser",strings(focuserBackends())},{"solver",strings(solverBackends())}}},{"solve",solveToJson(lastSolve_)},{"session",sessionJson(scheduler_.status())},{"operations",operations_.operationsJson(true)},{"resourceLocks",operations_.locksJson()},{"stellarium",QJsonObject{{"running",stellariumRunning()},{"port",int(stellariumPort())}}}};if(!lastFrame_.image.empty())j["lastFrame"]=QJsonObject{{"frameId",lastFrame_.id},{"capturedUtc",lastFrame_.capturedUtc.toString(Qt::ISODateWithMs)},{"width",lastFrame_.image.cols},{"height",lastFrame_.image.rows},{"exposureSec",lastFrame_.exposureSec},{"gain",lastFrame_.gain},{"binX",lastFrame_.binX},{"binY",lastFrame_.binY},{"role","main"}};if(!lastGuideFrame_.image.empty())j["lastGuideFrame"]=QJsonObject{{"frameId",lastGuideFrame_.id},{"capturedUtc",lastGuideFrame_.capturedUtc.toString(Qt::ISODateWithMs)},{"width",lastGuideFrame_.image.cols},{"height",lastGuideFrame_.image.rows},{"exposureSec",lastGuideFrame_.exposureSec},{"gain",lastGuideFrame_.gain},{"binX",lastGuideFrame_.binX},{"binY",lastGuideFrame_.binY},{"role","guide"}};MountStatus m;if(mountStatus(m,nullptr)){QJsonObject mj{{"raDeg",m.coordinate.raDeg},{"decDeg",m.coordinate.decDeg},{"coordinateFrame",equatorialFrameName(m.coordinate.frame)},{"coordinateValid",m.coordinateValid},{"tracking",m.tracking},{"slewing",m.slewing},{"parked",m.parked},{"pierSide",m.pierSide},{"geometryType",m.geometryType}};if(m.axes.valid){mj["axis1Deg"]=m.axes.axis1Deg;mj["axis2Deg"]=m.axes.axis2Deg;mj["axesValid"]=true;}else mj["axesValid"]=false;if(!m.diagnostics.isEmpty())mj["diagnostics"]=m.diagnostics;j["mount"]=mj;}FocuserStatus f;if(focuserStatus(f,nullptr)){QJsonObject fj{{"position",f.position},{"moving",f.moving}};if(f.temperatureC)fj["temperatureC"]=*f.temperatureC;j["focuser"]=fj;}auto g=guiding_.status();j["guiding"]=QJsonObject{{"active",g.active},{"raErrorArcsec",g.raErrorArcsec},{"decErrorArcsec",g.decErrorArcsec},{"rmsArcsec",g.rmsArcsec}};return j;}
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
QImage ApplicationController::toQImage(const cv::Mat&i){if(i.empty())return{};cv::Mat rgb;if(i.channels()==1){cv::Mat u8;if(i.depth()==CV_16U){
        // Preserve the physical sensor level for histogram/exposure control.
        // The old per-frame min/max normalization made a 1 ms and a 16 ms QHY
        // frame look almost identical to the histogram assistant, producing the
        // observed x2/x0.5 oscillation. Display auto-stretch is applied later in
        // MainWindow::renderCameraFrame and must remain separate from measurement.
        i.convertTo(u8,CV_8U,255.0/65535.0);
    }else i.convertTo(u8,CV_8U);cv::cvtColor(u8,rgb,cv::COLOR_GRAY2RGB);}else{cv::Mat bgr8;if(i.depth()==CV_16U)i.convertTo(bgr8,CV_8U,255.0/65535.0);else if(i.depth()!=CV_8U){double mn=0,mx=0;cv::minMaxLoc(i.reshape(1),&mn,&mx);i.convertTo(bgr8,CV_8U,255.0/std::max(1.0,mx));}else bgr8=i;cv::cvtColor(bgr8,rgb,cv::COLOR_BGR2RGB);}return QImage(rgb.data,rgb.cols,rgb.rows,int(rgb.step),QImage::Format_RGB888).copy();}
} // namespace oas
