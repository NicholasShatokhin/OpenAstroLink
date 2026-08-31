#include "backends/ascom_classic_mount.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <algorithm>

namespace oas {
AscomClassicMount::AscomClassicMount(QString p):progId_(std::move(p)){}
AscomClassicMount::~AscomClassicMount(){disconnectDevice();}
QString AscomClassicMount::hostExecutable(){
    const QString env=qEnvironmentVariable("OAS_ASCOM_HOST").trimmed();if(!env.isEmpty())return env;
    const QString app=QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QStringList c{QDir(app).filePath("oas-ascom-host.exe"),QDir(app).filePath("ascom-host/oas-ascom-host.exe")};
#else
    const QStringList c{QDir(app).filePath("oas-ascom-host"),QDir(app).filePath("ascom-host/oas-ascom-host")};
#endif
    for(const auto&p:c)if(QFileInfo::exists(p))return p;return c.last();
}
bool AscomClassicMount::ensureHost(QString *error){
#ifndef Q_OS_WIN
    if(error)*error="Classic ASCOM is Windows-only";return false;
#else
    if(host_.state()!=QProcess::NotRunning)return true;const QString exe=hostExecutable();if(!QFileInfo::exists(exe)){if(error)*error="Classic ASCOM helper not found: "+exe+" (build target oas-ascom-host / install ASCOM Platform)";return false;}
    host_.setProgram(exe);host_.setArguments({});host_.setProcessChannelMode(QProcess::SeparateChannels);host_.start();if(!host_.waitForStarted(5000)){if(error)*error="Could not start ASCOM helper: "+host_.errorString();return false;}QJsonObject data;if(!request({{"cmd","hello"}},&data,error,5000))return false;return true;
#endif
}
bool AscomClassicMount::request(const QJsonObject&r,QJsonObject*d,QString*error,int timeoutMs){
    QMutexLocker lock(&ioMutex_);if(host_.state()==QProcess::NotRunning){if(error)*error="ASCOM helper is not running";return false;}
    host_.write(QJsonDocument(r).toJson(QJsonDocument::Compact)+"\n");if(!host_.waitForBytesWritten(std::min(timeoutMs,3000))&&host_.bytesToWrite()>0){if(error)*error="ASCOM helper write timeout";return false;}
    QElapsedTimer timer;timer.start();while(!host_.canReadLine()&&timer.elapsed()<timeoutMs){if(host_.state()==QProcess::NotRunning)break;host_.waitForReadyRead(std::min(100,std::max(1,timeoutMs-int(timer.elapsed()))));}
    if(!host_.canReadLine()){if(error)*error="ASCOM helper response timeout/exit: "+QString::fromUtf8(host_.readAllStandardError());return false;}
    QByteArray line=host_.readLine();while(line.trimmed().isEmpty()&&host_.canReadLine())line=host_.readLine();const auto doc=QJsonDocument::fromJson(line);if(!doc.isObject()){if(error)*error="Invalid ASCOM helper response: "+QString::fromUtf8(line);return false;}const auto o=doc.object();if(!o.value("ok").toBool()){if(error)*error=o.value("error").toObject().value("message").toString("ASCOM call failed");return false;}if(d)*d=o.value("data").toObject();return true;
}
bool AscomClassicMount::connectDevice(QString *error){state_=ConnectionState::Connecting;if(progId_.trimmed().isEmpty()){if(error)*error="Choose or enter an ASCOM Telescope ProgID (for example EQMOD.Telescope)";state_=ConnectionState::Error;return false;}if(!ensureHost(error)){state_=ConnectionState::Error;return false;}QJsonObject d;if(!request({{"cmd","connect"},{"progId",progId_}},&d,error,15000)){state_=ConnectionState::Error;return false;}name_=d.value("name").toString(progId_);updateEquatorialSystem(d.value("equatorialSystem").toInt(-1));state_=ConnectionState::Connected;return true;}
void AscomClassicMount::disconnectDevice(){if(host_.state()!=QProcess::NotRunning){QJsonObject d;request({{"cmd","disconnect"}},&d,nullptr,5000);host_.write("{\"cmd\":\"quit\"}\n");host_.waitForFinished(2000);if(host_.state()!=QProcess::NotRunning)host_.kill();}state_=ConnectionState::Disconnected;}
void AscomClassicMount::updateEquatorialSystem(int value){
    equatorialSystem_=value;
    equatorialFrameAssumed_=false;
    switch(value){
    case 1: // ASCOM equTopocentric. OAL currently models this as its of-date/JNow frame.
        nativeFrame_=EquatorialFrame::JNow;break;
    case 2: // ASCOM equJ2000
        nativeFrame_=EquatorialFrame::J2000;break;
    case 0: // equOther: EQMOD defaults to this even though its manual GOTO defaults to JNOW.
        if(progId_.startsWith("EQMOD.",Qt::CaseInsensitive)){nativeFrame_=EquatorialFrame::JNow;equatorialFrameAssumed_=true;}
        else {nativeFrame_=EquatorialFrame::J2000;equatorialFrameAssumed_=true;}
        break;
    default:
        // J2050/B1950 are not represented by the current two-frame OAL core. Keep
        // the legacy J2000 fallback, but make the assumption visible in diagnostics.
        nativeFrame_=EquatorialFrame::J2000;equatorialFrameAssumed_=true;break;
    }
}
EquatorialCoord AscomClassicMount::toAscomFrame(const EquatorialCoord&c)const{return convertEquatorialFrame(c,nativeFrame_);}
EquatorialCoord AscomClassicMount::fromAscomFrame(const EquatorialCoord&c)const{return convertEquatorialFrame(c,EquatorialFrame::J2000);}
bool AscomClassicMount::status(MountStatus&s,QString*e){
    QJsonObject d;if(!request({{"cmd","status"}},&d,e))return false;
    updateEquatorialSystem(d.value("equatorialSystem").toInt(equatorialSystem_));
    EquatorialCoord native{d.value("raHours").toDouble()*15.0,d.value("decDeg").toDouble(),nativeFrame_};
    s.connection=state_;s.coordinate=fromAscomFrame(native);s.coordinateValid=true;
    s.tracking=d.value("tracking").toBool();s.slewing=d.value("slewing").toBool();s.parked=d.value("parked").toBool();
    const int side=d.value("sideOfPier").toInt(-1);s.pierSide=side==0?"east":side==1?"west":"unknown";s.geometryType="ascom-classic";
    if(equatorialFrameAssumed_){
        d["equatorialFrameAssumed"]=true;
        d["equatorialFrameAssumption"]=(equatorialSystem_==0&&progId_.startsWith("EQMOD.",Qt::CaseInsensitive))
            ? "EQMOD equOther(0) compatibility: treating ASCOM RA/DEC as topocentric/JNow"
            : "Unsupported/unknown ASCOM EquatorialSystem: compatibility fallback active";
    }else d["equatorialFrameAssumed"]=false;
    d["oalAscomFrame"]=(nativeFrame_==EquatorialFrame::JNow?"topocentric/JNow":"J2000");
    s.diagnostics=d;return true;
}
bool AscomClassicMount::slewTo(const EquatorialCoord&t,QString*e){const auto n=toAscomFrame(t);return request({{"cmd","slew"},{"raHours",n.raDeg/15.0},{"decDeg",n.decDeg}},nullptr,e,15000);}
bool AscomClassicMount::abortMotion(QString*e){return request({{"cmd","abort"}},nullptr,e);}
bool AscomClassicMount::syncTo(const EquatorialCoord&t,QString*e){const auto n=toAscomFrame(t);return request({{"cmd","sync"},{"raHours",n.raDeg/15.0},{"decDeg",n.decDeg}},nullptr,e);}
bool AscomClassicMount::setTracking(bool v,TrackingRate rate,QString*e){
    QJsonObject d;const bool ok=request({{"cmd","tracking"},{"enabled",v},{"rate",trackingRateName(rate)}},&d,e);
    if(ok&&d.contains("rateWarning")&&e)e->clear();
    return ok;
}
bool AscomClassicMount::setSiteTime(const ObserverLocation &site,const QDateTime &utc,QString*e){
    QJsonObject d;return request({{"cmd","setSiteTime"},{"latitudeDeg",site.latitudeDeg},{"longitudeDeg",site.longitudeDeg},{"elevationM",site.elevationM},{"utc",utc.toUTC().toString(Qt::ISODateWithMs)}},&d,e,8000);
}
bool AscomClassicMount::park(bool v,QString*e){return request({{"cmd","park"},{"parked",v}},nullptr,e,30000);}
bool AscomClassicMount::setCurrentParkPosition(QString*e){return request({{"cmd","setParkHere"}},nullptr,e,10000);}
bool AscomClassicMount::pulseGuide(GuideDirection dir,int ms,QString*e){int d=0;switch(dir){case GuideDirection::North:d=0;break;case GuideDirection::South:d=1;break;case GuideDirection::East:d=2;break;case GuideDirection::West:d=3;break;}return request({{"cmd","pulseGuide"},{"direction",d},{"durationMs",ms}},nullptr,e,std::max(5000,ms+3000));}
bool AscomClassicMount::manualSlew(int a1,int a2,int rate,QString*e){return request({{"cmd","manualSlew"},{"axis1Direction",std::clamp(a1,-1,1)},{"axis2Direction",std::clamp(a2,-1,1)},{"rateLevel",std::clamp(rate,0,9)}},nullptr,e,5000);}
QString AscomClassicMount::destinationPierSide(const EquatorialCoord&t,QString*e){const auto n=toAscomFrame(t);QJsonObject d;if(!request({{"cmd","destinationPierSide"},{"raHours",n.raDeg/15.0},{"decDeg",n.decDeg}},&d,e,5000))return{};const int side=d.value("sideOfPier").toInt(-1);return side==0?"east":side==1?"west":"unknown";}
bool AscomClassicMount::chooseTelescope(const QString&current,QString&selected,QString*error){
#ifndef Q_OS_WIN
    if(error)*error="ASCOM Chooser is Windows-only";return false;
#else
    const QString exe=hostExecutable();if(!QFileInfo::exists(exe)){if(error)*error="Classic ASCOM helper not found: "+exe;return false;}QProcess p;p.start(exe,{"--choose",current});if(!p.waitForFinished(-1)){if(error)*error="ASCOM Chooser helper failed";return false;}const auto doc=QJsonDocument::fromJson(p.readAllStandardOutput().trimmed());if(!doc.isObject()||!doc.object().value("ok").toBool()){if(error)*error=doc.object().value("error").toObject().value("message").toString("ASCOM Chooser failed");return false;}selected=doc.object().value("data").toObject().value("progId").toString();return !selected.isEmpty();
#endif
}
bool AscomClassicMount::setupTelescope(const QString&progId,QString*error){
#ifndef Q_OS_WIN
    if(error)*error="ASCOM setup is Windows-only";return false;
#else
    if(progId.trimmed().isEmpty()){if(error)*error="Choose an ASCOM Telescope driver first";return false;}
    const QString exe=hostExecutable();if(!QFileInfo::exists(exe)){if(error)*error="Classic ASCOM helper not found: "+exe;return false;}
    QProcess p;p.start(exe,{"--setup",progId});if(!p.waitForFinished(-1)){if(error)*error="ASCOM setup helper failed";return false;}
    const auto doc=QJsonDocument::fromJson(p.readAllStandardOutput().trimmed());if(!doc.isObject()||!doc.object().value("ok").toBool()){if(error)*error=doc.object().value("error").toObject().value("message").toString("ASCOM setup failed");return false;}return true;
#endif
}

}
