#include "backends/oal_native_devices.h"
#include "core/equatorial_frames.h"
#include "oal/driver_api.h"
#include "oal/driver_plugin_loader.h"

#include <QDateTime>
#include <QJsonArray>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <opencv2/core.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace oas {
namespace {
double skySeparationDeg(const EquatorialCoord &a,const EquatorialCoord &b){
    constexpr double kPi=3.14159265358979323846;
    const double ra1=a.raDeg*kPi/180.0,ra2=b.raDeg*kPi/180.0;
    const double d1=a.decDeg*kPi/180.0,d2=b.decDeg*kPi/180.0;
    const double c=std::sin(d1)*std::sin(d2)+std::cos(d1)*std::cos(d2)*std::cos(ra1-ra2);
    return std::acos(std::clamp(c,-1.0,1.0))*180.0/kPi;
}
QByteArray fitsCard(const QString &key,const QString &value){
    QByteArray c=(key.leftJustified(8,' ')+"= "+value).toLatin1();
    c=c.left(80);if(c.size()<80)c.append(QByteArray(80-c.size(),' '));return c;
}
QByteArray fitsCommentCard(const QString &text){QByteArray c=text.toLatin1().left(80);if(c.size()<80)c.append(QByteArray(80-c.size(),' '));return c;}
bool writeFitsScienceFrame(const cv::Mat &image,const QString &path,const CameraFrame &frame,QString *error){
    if(image.empty()||(image.depth()!=CV_8U&&image.depth()!=CV_16U)||(image.channels()!=1&&image.channels()!=3)){
        if(error)*error="FITS science writer supports 8/16-bit mono or RGB frames";return false;
    }
    QFile f(path);if(!f.open(QIODevice::WriteOnly)){if(error)*error="Could not create science frame: "+path;return false;}
    QByteArray h;
    h+=fitsCard("SIMPLE","                    T");
    h+=fitsCard("BITPIX",image.depth()==CV_8U?"                    8":"                   16");
    h+=fitsCard("NAXIS",image.channels()==1?"                    2":"                    3");
    h+=fitsCard("NAXIS1",QString::number(image.cols).rightJustified(20,' '));
    h+=fitsCard("NAXIS2",QString::number(image.rows).rightJustified(20,' '));
    if(image.channels()==3)h+=fitsCard("NAXIS3","                    3");
    if(image.depth()==CV_16U){h+=fitsCard("BSCALE","                    1");h+=fitsCard("BZERO","                32768");}
    h+=fitsCard("DATE-OBS",QString("'%1'").arg(frame.capturedUtc.toUTC().toString("yyyy-MM-dd'T'HH:mm:ss.zzz")).leftJustified(20,' '));
    h+=fitsCard("EXPTIME",QString::number(frame.exposureSec,'f',6).rightJustified(20,' '));
    h+=fitsCard("GAIN",QString::number(frame.gain).rightJustified(20,' '));
    h+=fitsCard("XBINNING",QString::number(frame.binX).rightJustified(20,' '));
    h+=fitsCard("YBINNING",QString::number(frame.binY).rightJustified(20,' '));
    h+=fitsCommentCard("END");
    const int hp=(2880-(h.size()%2880))%2880;if(hp)h.append(QByteArray(hp,' '));
    if(f.write(h)!=h.size()){if(error)*error="Could not write FITS header";return false;}
    auto writeSample=[&](int y,int x,int ch)->bool{
        if(image.depth()==CV_8U){const uchar *row=image.ptr<uchar>(y);char b=char(row[x*image.channels()+ch]);return f.write(&b,1)==1;}
        const ushort *row=image.ptr<ushort>(y);const qint16 signedValue=qint16(int(row[x*image.channels()+ch])-32768);char b[2]={char((quint16(signedValue)>>8)&0xff),char(quint16(signedValue)&0xff)};return f.write(b,2)==2;
    };
    // FITS color cube is planar; mono is a single plane.
    for(int ch=0;ch<image.channels();++ch)for(int y=0;y<image.rows;++y)for(int x=0;x<image.cols;++x)if(!writeSample(y,x,ch)){if(error)*error="Could not write FITS image data";return false;}
    const qint64 dataBytes=qint64(image.rows)*image.cols*image.channels()*(image.depth()==CV_8U?1:2);
    const int dp=int((2880-(dataBytes%2880))%2880);if(dp)f.write(QByteArray(dp,0));f.close();return true;
}
QString defaultSciencePath(const QString &driverId,const CameraFrame &frame){
    QString vendor=driverId.section('.',1,1).toUpper();if(vendor.isEmpty())vendor="Camera";
    QString root=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);if(root.isEmpty())root=QDir::homePath();
    QDir dir(QDir(root).filePath("OpenAstroLink/"+vendor));dir.mkpath(".");
    const QString stamp=frame.capturedUtc.toLocalTime().toString("yyyyMMdd_HHmmss_zzz");return dir.filePath(vendor+"_"+stamp+".fits");
}
}

QString nativeBackendKey(const QString &driverId, const QString &deviceId) {
    return "native:" + driverId + "/" + QString::fromUtf8(QUrl::toPercentEncoding(deviceId));
}

bool parseNativeBackendKey(const QString &key, QString &driverId, QString &deviceId) {
    if (!key.startsWith("native:")) return false;
    const QString rest = key.mid(7);
    const int slash = rest.indexOf('/');
    if (slash <= 0 || slash == rest.size() - 1) return false;
    driverId = rest.left(slash);
    deviceId = QUrl::fromPercentEncoding(rest.mid(slash + 1).toUtf8());
    return !driverId.isEmpty() && !deviceId.isEmpty();
}

NativeOalDeviceBase::NativeOalDeviceBase(std::shared_ptr<OalDriverPluginLoader> loader,
                                         QJsonObject descriptor)
    : loader_(std::move(loader)),
      driverId_(descriptor.value("driverId").toString()),
      deviceId_(descriptor.value("id").toString()),
      name_(descriptor.value("name").toString(deviceId_)),
      descriptor_(std::move(descriptor)) {}

bool NativeOalDeviceBase::invokeOk(const QString &method, const QJsonObject &request,
                                   QJsonObject *data, QString *error,
                                   const QString &operationId) const {
    if (!loader_) { if (error) *error = "Native driver registry unavailable"; return false; }
    QString e;
    const auto response = loader_->invoke(driverId_, deviceId_, method, request, &e, operationId);
    if (response.isEmpty()) { if (error) *error = e; return false; }
    if (!response.value("ok").toBool(true)) {
        const auto problem = response.value("error").toObject();
        if (error) *error = problem.value("message").toString(e.isEmpty() ? "Native driver call failed" : e);
        return false;
    }
    if (data) *data = response.value("data").toObject();
    return true;
}

QJsonObject NativeOalDeviceBase::capabilities(QString *error) const {
    return loader_ ? loader_->capabilities(driverId_, deviceId_, error) : QJsonObject{};
}

NativeOalCamera::NativeOalCamera(std::shared_ptr<OalDriverPluginLoader> l, const QJsonObject &d)
    : NativeOalDeviceBase(std::move(l), d) {}

bool NativeOalCamera::connectDevice(QString *error) {
    state_ = ConnectionState::Connecting;
    if (!invokeOk("device.connect", {}, nullptr, error)) { state_ = ConnectionState::Error; return false; }
    state_ = ConnectionState::Connected;
    const auto sensor=capabilities(nullptr).value("camera").toObject().value("sensor").toObject();
    sensorSizeCache_={sensor.value("widthPx").toInt(),sensor.value("heightPx").toInt()};
    return true;
}
void NativeOalCamera::disconnectDevice() {
    if (state_ == ConnectionState::Disconnected) return;
    invokeOk("device.disconnect", {}, nullptr, nullptr);
    state_ = ConnectionState::Disconnected;
}

namespace {
bool decodeNativeCameraFrame(const std::shared_ptr<OalDriverPluginLoader> &loader,
                             const QString &driverId,const QString &deviceId,
                             const QJsonObject &data,const ExposureRequest &r,const QSize &sensorSize,
                             CameraFrame &frame,bool allowScienceSave,QString *error){
    const quint64 token=data.value("frameToken").toVariant().toULongLong();
    if(!token){if(error)*error="Native camera returned no frameToken";return false;}
    NativeDriverFrame native;if(!loader->takePublishedFrame(token,native,error))return false;
    const int w=int(native.width),h=int(native.height);if(w<=0||h<=0){if(error)*error="Native camera returned invalid frame dimensions";return false;}
    int cvType=-1;switch(native.pixelFormat){
    case OAL_PIXEL_MONO8:case OAL_PIXEL_BAYER8:cvType=CV_8UC1;break;
    case OAL_PIXEL_MONO16:case OAL_PIXEL_BAYER16:cvType=CV_16UC1;break;
    case OAL_PIXEL_RGB8:cvType=CV_8UC3;break;
    case OAL_PIXEL_RGB16:cvType=CV_16UC3;break;
    default:if(native.channels==1&&native.bitsPerSample<=8)cvType=CV_8UC1;else if(native.channels==1&&native.bitsPerSample<=16)cvType=CV_16UC1;else if(native.channels==3&&native.bitsPerSample<=8)cvType=CV_8UC3;else if(native.channels==3&&native.bitsPerSample<=16)cvType=CV_16UC3;break;}
    if(cvType<0){if(error)*error="Unsupported native camera pixel format";return false;}
    const size_t elem=CV_ELEM_SIZE(cvType),minStride=size_t(w)*elem,stride=native.strideBytes?size_t(native.strideBytes):minStride;
    if(stride<minStride||native.bytes.size()<qsizetype(stride*size_t(h))){if(error)*error="Native camera frame buffer is shorter than declared geometry";return false;}
    frame.image=cv::Mat(h,w,cvType,native.bytes.data(),stride).clone();
    frame.id=native.frameId.isEmpty()?QString("native-%1").arg(token):native.frameId;
    frame.capturedUtc=native.capturedUnixNs>0?QDateTime::fromMSecsSinceEpoch(native.capturedUnixNs/1000000,Qt::UTC):QDateTime::currentDateTimeUtc();
    frame.exposureSec=native.exposureSec>0.0?native.exposureSec:r.exposureSec;frame.gain=int(native.gain);
    frame.offset=int(std::lround(native.metadata.value("actualOffset").toDouble(r.offset)));
    int actualBinX=1,actualBinY=1;const int requestedBinX=std::max(1,r.binX),requestedBinY=std::max(1,r.binY);
    const int sensorW=sensorSize.width(),sensorH=sensorSize.height();
    // Infer the bin actually delivered by the native driver from geometry. A
    // requested bin that the hardware silently ignored cannot silently corrupt plate scale
    // in downstream astrometry; the adaptive solver can then software-bin explicitly.
    if(r.roi.width<=0&&r.roi.height<=0&&sensorW>0&&sensorH>0){const int expectedW=std::max(1,sensorW/requestedBinX),expectedH=std::max(1,sensorH/requestedBinY);if(std::abs(w-expectedW)<=1)actualBinX=requestedBinX;if(std::abs(h-expectedH)<=1)actualBinY=requestedBinY;}
    else {actualBinX=requestedBinX;actualBinY=requestedBinY;}
    frame.binX=actualBinX;frame.binY=actualBinY;frame.source=deviceId;
    frame.bayerEncoded=(native.pixelFormat==OAL_PIXEL_BAYER8||native.pixelFormat==OAL_PIXEL_BAYER16)||native.metadata.value("rawSensor").toBool(false);
    frame.bayerPattern=native.metadata.value("bayerPattern").toString().trimmed().toUpper();
    // A rawSensor flag alone can also describe a monochrome sensor. Only
    // advertise Bayer encoding when the pixel format says so or the driver
    // publishes an explicit CFA pattern.
    if(frame.bayerPattern.isEmpty()&&native.pixelFormat!=OAL_PIXEL_BAYER8&&native.pixelFormat!=OAL_PIXEL_BAYER16)frame.bayerEncoded=false;
    frame.scienceFilePath=data.value("savedPath").toString();if(frame.scienceFilePath.isEmpty())frame.scienceFilePath=native.metadata.value("scienceFilePath").toString();if(frame.scienceFilePath.isEmpty())frame.scienceFilePath=native.metadata.value("originalPath").toString();
    if(allowScienceSave&&r.saveRaw&&frame.scienceFilePath.isEmpty()){QString path=r.savePath;if(path.isEmpty())path=defaultSciencePath(driverId,frame);else if(QFileInfo(path).suffix().isEmpty()){QDir dir(path);dir.mkpath(".");path=dir.filePath(QFileInfo(defaultSciencePath(driverId,frame)).fileName());}if(!writeFitsScienceFrame(frame.image,path,frame,error))return false;frame.scienceFilePath=path;}
    return true;
}
}

bool NativeOalCamera::capture(const ExposureRequest &r, CameraFrame &frame, QString *error) {
    QJsonObject req{{"exposureSec",r.exposureSec},{"gain",r.gain},{"offset",r.offset},{"binX",r.binX},{"binY",r.binY},{"saveRaw",r.saveRaw}};
    if(!r.savePath.isEmpty())req["savePath"]=r.savePath;if(r.roi.width>0&&r.roi.height>0)req["roi"]=QJsonObject{{"x",r.roi.x},{"y",r.roi.y},{"width",r.roi.width},{"height",r.roi.height}};
    QJsonObject data;if(!invokeOk("camera.capture",req,&data,error))return false;
    return decodeNativeCameraFrame(loader_,driverId_,deviceId_,data,r,sensorSizeCache_,frame,true,error);
}

bool NativeOalCamera::nativeLiveSupported() const {
    return capabilities(nullptr).value("camera").toObject().value("streaming").toObject().value("supported").toBool(false);
}
bool NativeOalCamera::startNativeLive(const LiveViewRequest &r,QString *error){
    QJsonObject req{{"exposureSec",r.exposureSec},{"gain",r.gain},{"offset",r.offset},{"binX",r.binX},{"binY",r.binY},{"targetFps",r.targetFps}};
    if(r.roi.width>0&&r.roi.height>0)req["roi"]=QJsonObject{{"x",r.roi.x},{"y",r.roi.y},{"width",r.roi.width},{"height",r.roi.height}};
    if(!invokeOk("camera.liveStart",req,nullptr,error))return false;liveRequest_=r;return true;
}
bool NativeOalCamera::nextNativeLiveFrame(CameraFrame &frame,int timeoutMs,QString *error){
    QJsonObject data;if(!invokeOk("camera.liveFrame",{{"timeoutMs",timeoutMs}},&data,error))return false;
    ExposureRequest r;r.exposureSec=liveRequest_.exposureSec;r.gain=liveRequest_.gain;r.offset=liveRequest_.offset;r.binX=liveRequest_.binX;r.binY=liveRequest_.binY;r.roi=liveRequest_.roi;r.saveRaw=false;
    return decodeNativeCameraFrame(loader_,driverId_,deviceId_,data,r,sensorSizeCache_,frame,false,error);
}
bool NativeOalCamera::stopNativeLive(QString *error){return invokeOk("camera.liveStop",{},nullptr,error);}

bool NativeOalCamera::canAbortExposure() const {
    const auto c = capabilities(nullptr).value("camera").toObject().value("exposure").toObject();
    return c.value("abortSupported").toBool(false);
}
bool NativeOalCamera::abortExposure(QString *error) {
    return invokeOk("camera.abortExposure", {}, nullptr, error);
}
QSize NativeOalCamera::sensorSize() const {
    if(sensorSizeCache_.isValid())return sensorSizeCache_;
    const auto sensor=capabilities(nullptr).value("camera").toObject().value("sensor").toObject();
    return {sensor.value("widthPx").toInt(),sensor.value("heightPx").toInt()};
}

NativeOalMount::NativeOalMount(std::shared_ptr<OalDriverPluginLoader> l, const QJsonObject &d,
                                   MountGeometryConfig geometry, ObserverLocation observer)
    : NativeOalDeviceBase(std::move(l), d), geometry_(std::move(geometry), observer) {
    const auto mountCaps=capabilities(nullptr).value("mount").toObject();
    geometryAware_=mountCaps.value("rawAxes").toObject().value("supported").toBool(false);
}
bool NativeOalMount::connectDevice(QString *error) {
    state_=ConnectionState::Connecting;
    if(!invokeOk("device.connect",{},nullptr,error)){state_=ConnectionState::Error;return false;}
    state_=ConnectionState::Connected;
    alignmentSource_.clear();homeAlignmentNote_.clear();
    tryAutoHomeSync(nullptr);
    return true;
}
void NativeOalMount::disconnectDevice(){if(state_==ConnectionState::Disconnected)return;invokeOk("mount.abort",{},nullptr,nullptr);invokeOk("device.disconnect",{},nullptr,nullptr);parking_=false;parked_=false;state_=ConnectionState::Disconnected;}
bool NativeOalMount::rawAxisStatus(MechanicalAxes &axes,bool *slewing,QString *error,QJsonObject *diagnostics) const {
    QJsonObject d;if(!invokeOk("mount.axisStatus",{},&d,error))return false;
    axes={d.value("axis1Deg").toDouble(),d.value("axis2Deg").toDouble(),true};
    if(slewing)*slewing=d.value("running1").toBool()||d.value("running2").toBool()||d.value("goto1").toBool()||d.value("goto2").toBool();
    if(diagnostics){for(const char *key:{"axis1ControllerCounts","axis2ControllerCounts","axis1HomeZeroCounts","axis2HomeZeroCounts","countsPerRev1","countsPerRev2","timerFreq","firmware","wireProtocol"})if(d.contains(key))(*diagnostics)[key]=d.value(key);}
    return true;
}
bool NativeOalMount::rawAxisGoto(const MechanicalAxes &axes,double limit,QString *error){
    return invokeOk("mount.gotoAxes",{{"axis1Deg",axes.axis1Deg},{"axis2Deg",axes.axis2Deg},{"maxAxisDeltaDeg",limit}},nullptr,error);
}
bool NativeOalMount::tryAutoHomeSync(QString *error){
    if(!geometryAware_||!geometry_.config().autoHomeSync)return false;
    const bool standardEqDriveHome=(driverId()=="oal.eqdrive"&&!geometry_.config().customHome&&geometry_.config().nativeCoordinateModelVersion>=4);
    if(!geometry_.config().customHome&&!standardEqDriveHome)return false;
    MechanicalAxes actual;QString e;
    if(!rawAxisStatus(actual,nullptr,&e)){homeAlignmentNote_="Home reference status unavailable: "+e;if(error)*error=e;return false;}
    const double expected1=standardEqDriveHome?0.0:geometry_.config().homeAxis1Deg;
    const double expected2=standardEqDriveHome?0.0:geometry_.config().homeAxis2Deg;
    auto delta=[](double a,double b){double d=std::fmod(a-b,360.0);if(d>180)d-=360;if(d<-180)d+=360;return std::abs(d);};
    const double d1=delta(actual.axis1Deg,expected1);
    const double d2=delta(actual.axis2Deg,expected2);
    const double tol=std::clamp(geometry_.config().homeToleranceDeg,0.1,15.0);
    if(d1>tol||d2>tol){homeAlignmentNote_=QString("Home reference not applied: current axes differ from %1 Home by dAxis1=%2deg dAxis2=%3deg (tolerance %4deg)").arg(standardEqDriveHome?"standard EQDrive mechanical zero":"saved").arg(d1,0,'f',4).arg(d2,0,'f',4).arg(tol,0,'f',2);if(error)*error=homeAlignmentNote_;return false;}
    if(!geometry_.syncHome(actual,QDateTime::currentDateTimeUtc(),&e)){homeAlignmentNote_="Home reference rejected: "+e;if(error)*error=e;return false;}
    alignmentSource_=standardEqDriveHome?"eqdrive-zero-home":"auto-home";
    homeAlignmentNote_=QString("%1 Home reference accepted: dAxis1=%2deg dAxis2=%3deg tolerance=%4deg").arg(standardEqDriveHome?"standard EQDrive mechanical zero":"saved").arg(d1,0,'f',4).arg(d2,0,'f',4).arg(tol,0,'f',2);
    if(error)error->clear();return true;
}
bool NativeOalMount::status(MountStatus &s, QString *error){
    if(geometryAware_){
        MechanicalAxes axes;bool moving=false;QJsonObject axisDiagnostics;if(!rawAxisStatus(axes,&moving,error,&axisDiagnostics))return false;
        if(parking_&&!moving){
            auto delta=[](double a,double b){double d=std::fmod(a-b,360.0);if(d>180)d-=360;if(d<-180)d+=360;return std::abs(d);};
            parked_=delta(axes.axis1Deg,parkTarget_.axis1Deg)<=0.25&&delta(axes.axis2Deg,parkTarget_.axis2Deg)<=0.25;parking_=false;
        }
        s.connection=state_;s.axes=axes;s.geometryType=mountGeometryTypeName(geometry_.config().type);s.slewing=moving;s.parked=parked_||parking_;
        s.diagnostics["alignmentSource"]=alignmentSource_.isEmpty()?(geometry_.synced()?"restored":"unsynced"):alignmentSource_;
        s.diagnostics["autoHomeSyncEnabled"]=geometry_.config().autoHomeSync;
        s.diagnostics["customHome"]=geometry_.config().customHome;
        s.diagnostics["homeAxis1Deg"]=geometry_.config().homeAxis1Deg;
        s.diagnostics["homeAxis2Deg"]=geometry_.config().homeAxis2Deg;
        s.diagnostics["homeToleranceDeg"]=geometry_.config().homeToleranceDeg;
        s.diagnostics["nativeCoordinateModelVersion"]=geometry_.config().nativeCoordinateModelVersion;
        s.diagnostics["standardEqDriveZeroHome"]=(driverId()=="oal.eqdrive"&&!geometry_.config().customHome&&geometry_.config().nativeCoordinateModelVersion>=4);
        for(auto it=axisDiagnostics.begin();it!=axisDiagnostics.end();++it)s.diagnostics[it.key()]=it.value();
        s.diagnostics["axis1Deg"]=axes.axis1Deg;s.diagnostics["axis2Deg"]=axes.axis2Deg;
        if(lastCommandedTarget_.valid){s.diagnostics["lastTargetAxis1Deg"]=lastCommandedTarget_.axis1Deg;s.diagnostics["lastTargetAxis2Deg"]=lastCommandedTarget_.axis2Deg;}
        if(!homeAlignmentNote_.isEmpty())s.diagnostics["homeAlignmentNote"]=homeAlignmentNote_;
        EquatorialCoord sky;if(geometry_.skyFromAxes(axes,sky,QDateTime::currentDateTimeUtc(),nullptr)){s.coordinate=sky;s.coordinateValid=true;}else{s.coordinate={0,0,EquatorialFrame::J2000};s.coordinateValid=false;}s.pierSide=geometry_.pierSide();s.diagnostics["pierSide"]=s.pierSide;
        // Tracking state is still owned by the driver. Query compatibility status
        // for this flag only; axis/sky coordinates come exclusively from Core geometry.
        QJsonObject d;if(invokeOk("mount.status",{},&d,nullptr))s.tracking=d.value("tracking").toBool();
        return true;
    }
    QJsonObject d;if(!invokeOk("mount.status",{},&d,error))return false;s.connection=state_;s.coordinate={d.value("raDeg").toDouble(),d.value("decDeg").toDouble(),equatorialFrameFromString(d.value("coordinateFrame").toString("J2000"))};s.coordinateValid=d.contains("coordinateValid")?d.value("coordinateValid").toBool():true;s.tracking=d.value("tracking").toBool();s.slewing=d.value("slewing").toBool();s.parked=d.value("parked").toBool();s.pierSide=d.value("pierSide").toString("unknown");s.geometryType="backend-native";return true;
}
bool NativeOalMount::slewTo(const EquatorialCoord&t,QString*e){
    if(!geometryAware_)return invokeOk("mount.slew",{{"raDeg",t.raDeg},{"decDeg",t.decDeg}},nullptr,e);
    if(parked_){if(e)*e="Mount is parked; unpark before GOTO";return false;}
    if(parking_){if(e)*e="Mount is currently parking; unpark or ABORT before GOTO";return false;}
    const QDateTime utc=QDateTime::currentDateTimeUtc();
    MechanicalAxes current,target;if(!rawAxisStatus(current,nullptr,e))return false;
    EquatorialCoord currentSky;if(!geometry_.skyFromAxes(current,currentSky,utc,e))return false;
    const auto targetJ2000=convertEquatorialFrame(t,EquatorialFrame::J2000,utc);
    const double skySep=skySeparationDeg(convertEquatorialFrame(currentSky,EquatorialFrame::J2000,utc),targetJ2000);
    const double skyLimit=std::clamp(geometry_.config().maxGotoAxisDeltaDeg,0.1,180.0);
    if(skySep>skyLimit){if(e)*e=QString("Sky GOTO exceeds safety limit %1 deg: separation=%2 deg").arg(skyLimit,0,'f',1).arg(skySep,0,'f',3);return false;}
    if(!geometry_.axesForSky(t,current,target,utc,e))return false;
    lastCommandedTarget_=target;
    // Near the celestial pole a small angular displacement can legitimately
    // require a large RA-axis rotation because RA is singular at Dec=+/-90.
    // Safety is therefore expressed in true sky separation. The transport keeps
    // a separate absolute mechanical ceiling of 180 degrees per axis.
    return rawAxisGoto(target,180.0,e);
}
bool NativeOalMount::abortMotion(QString*e){const bool ok=invokeOk("mount.abort",{},nullptr,e);if(ok){parking_=false;parked_=false;}return ok;}
bool NativeOalMount::syncTo(const EquatorialCoord&t,QString*e){
    if(!geometryAware_)return invokeOk("mount.sync",{{"raDeg",t.raDeg},{"decDeg",t.decDeg}},nullptr,e);
    MechanicalAxes axes;if(!rawAxisStatus(axes,nullptr,e))return false;
    const auto utc=QDateTime::currentDateTimeUtc();
    const auto jnow=convertEquatorialFrame(t,EquatorialFrame::JNow,utc);
    if(driverId()=="oal.eqdrive"&&geometry_.config().nativeCoordinateModelVersion>=4&&
       geometry_.config().type==MountGeometryType::GermanEquatorial&&std::abs(jnow.decDeg)>80.0){
        const bool standard=!geometry_.config().customHome;
        const double h1=standard?0.0:geometry_.config().homeAxis1Deg,h2=standard?0.0:geometry_.config().homeAxis2Deg;
        auto delta=[](double a,double b){double d=std::fmod(a-b,360.0);if(d>180)d-=360;if(d<-180)d+=360;return std::abs(d);};
        const double tol=std::clamp(geometry_.config().homeToleranceDeg,0.1,15.0);
        if(delta(axes.axis1Deg,h1)<=tol&&delta(axes.axis2Deg,h2)<=tol){
            const bool ok=geometry_.syncHome(axes,utc,e);
            if(ok){alignmentSource_="polar-home-sync";homeAlignmentNote_="Near-pole Sync at Home: RA was intentionally ignored because RA is singular at the celestial pole; mechanical Home preserved the polar-frame orientation.";}
            return ok;
        }
    }
    const bool ok=geometry_.sync(t,axes,utc,e);
    if(ok){alignmentSource_="manual-sync";homeAlignmentNote_.clear();}
    return ok;
}
void NativeOalMount::configureGeometry(const MountGeometryConfig &c,const ObserverLocation &o){
    const bool wasSynced=geometry_.synced();const auto old=geometry_.config();
    const bool parkChanged=old.customPark!=c.customPark||std::abs(old.parkAxis1Deg-c.parkAxis1Deg)>1e-10||std::abs(old.parkAxis2Deg-c.parkAxis2Deg)>1e-10;
    geometry_.configure(c,o);
    if(wasSynced&&!geometry_.synced())alignmentSource_.clear();
    if(!geometry_.synced()&&state_==ConnectionState::Connected)tryAutoHomeSync(nullptr);
    if(parkChanged){parked_=false;parking_=false;}
}
bool NativeOalMount::setSiteTime(const ObserverLocation &site,const QDateTime &,QString*e){
    const bool wasSynced=geometry_.synced();
    geometry_.configure(geometry_.config(),site);
    if(wasSynced&&!geometry_.synced())alignmentSource_.clear();
    if(!geometry_.synced()&&state_==ConnectionState::Connected)tryAutoHomeSync(nullptr);
    if(e)e->clear();
    return true;
}
bool NativeOalMount::setTracking(bool v,TrackingRate rate,QString*e){
    if(geometryAware_){
        if(v&&parking_){if(e)*e="Cannot enable tracking while a Park slew is active";return false;}
        if(v&&!geometry_.synced()){if(e)*e="Sync the mount on a known sky position before enabling tracking";return false;}
        const int direction=geometry_.trackingAxis1Direction();
        if(direction==0&&v){if(e)*e="This mount geometry requires two-axis tracking; raw native rate-vector tracking is not implemented yet";return false;}
        return invokeOk("mount.setTracking",{{"enabled",v},{"axis1Direction",direction},{"rate",trackingRateName(rate)}},nullptr,e);
    }
    return invokeOk("mount.setTracking",{{"enabled",v},{"rate",trackingRateName(rate)}},nullptr,e);
}
bool NativeOalMount::park(bool v,QString*e){
    if(!geometryAware_)return invokeOk("mount.park",{{"parked",v}},nullptr,e);
    if(!v){
        // Unpark while a mechanical park GOTO is still running means STOP, not
        // merely changing a logical checkbox. This is a safety path.
        if(parking_&&!abortMotion(e))return false;
        parking_=false;parked_=false;return true;
    }
    const bool standardEqDrivePark=(driverId()=="oal.eqdrive"&&geometry_.config().nativeCoordinateModelVersion>=4&&!geometry_.config().customPark);
    if(!geometry_.config().customPark&&!standardEqDrivePark){if(e)*e="Mechanical Park is not calibrated. Move the mount manually to a safe park position and use 'Set current mechanical axes as Park' first.";return false;}
    // Never enter a park slew while sidereal tracking remains active.
    if(!invokeOk("mount.setTracking",{{"enabled",false},{"axis1Direction",0}},nullptr,e))return false;
    const auto target=standardEqDrivePark?MechanicalAxes{0.0,0.0,true}:geometry_.parkAxes();if(!rawAxisGoto(target,180.0,e))return false;parkTarget_=target;parking_=true;parked_=false;return true;
}
bool NativeOalMount::pulseGuide(GuideDirection dir,int ms,QString*e){QString d;switch(dir){case GuideDirection::North:d="north";break;case GuideDirection::South:d="south";break;case GuideDirection::East:d="east";break;case GuideDirection::West:d="west";break;}return invokeOk("mount.pulseGuide",{{"direction",d},{"durationMs",ms}},nullptr,e);}
bool NativeOalMount::manualSlew(int axis1Direction,int axis2Direction,int rateLevel,QString*e){
    parking_=false;parked_=false;
    return invokeOk("mount.manualSlew",{{"axis1Direction",std::clamp(axis1Direction,-1,1)},{"axis2Direction",std::clamp(axis2Direction,-1,1)},{"rateLevel",std::clamp(rateLevel,0,9)}},nullptr,e);
}

NativeOalFocuser::NativeOalFocuser(std::shared_ptr<OalDriverPluginLoader> l, const QJsonObject &d)
    : NativeOalDeviceBase(std::move(l), d) {}
bool NativeOalFocuser::connectDevice(QString*e){state_=ConnectionState::Connecting;if(!invokeOk("device.connect",{},nullptr,e)){state_=ConnectionState::Error;return false;}state_=ConnectionState::Connected;return true;}
void NativeOalFocuser::disconnectDevice(){if(state_==ConnectionState::Disconnected)return;invokeOk("device.disconnect",{},nullptr,nullptr);state_=ConnectionState::Disconnected;}
bool NativeOalFocuser::status(FocuserStatus&s,QString*e){QJsonObject d;if(!invokeOk("focuser.status",{},&d,e))return false;s.connection=state_;s.position=d.value("position").toInt();s.moving=d.value("moving").toBool();if(d.contains("temperatureC"))s.temperatureC=d.value("temperatureC").toDouble();else s.temperatureC.reset();return true;}
bool NativeOalFocuser::moveAbsolute(int p,QString*e){return invokeOk("focuser.moveAbsolute",{{"position",p}},nullptr,e);}
bool NativeOalFocuser::moveRelative(int d,QString*e){return invokeOk("focuser.moveRelative",{{"delta",d}},nullptr,e);}
bool NativeOalFocuser::halt(QString*e){return invokeOk("focuser.halt",{},nullptr,e);}

} // namespace oas
