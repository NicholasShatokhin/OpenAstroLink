#include "oal/driver_api.h"
#include "astep_protocol.h"
#include "../skywatcher/motor_controller_protocol.h"
#include "../common_blocking_serial_session.h"

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QSerialPortInfo>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>

namespace {
// Protocol basis: official EQDrive ASTEP specification (St/Pos/Speed/Slew/Goto/Cg/Drv; coordinates in degrees).
constexpr double kSiderealDegPerHour = 360.0 / (86164.0905 / 3600.0);

enum class WireProtocol { Astep, MotorController };

struct Device {
    QString id;
    QString port;
    QString serial;
    QString description{QStringLiteral("EQDrive mount (native OAL)")};
    QString firmware;
    int baudRate{0};
    bool dtr{false};
    bool rts{false};
    bool connected{false};
    WireProtocol protocol{WireProtocol::Astep};
    quint32 countsPerRev1{0};
    quint32 countsPerRev2{0};
    quint32 timerFreq{0};
    int configuredAxisDirection1{1};
    int configuredAxisDirection2{1};
    bool coordinateSynced{false};
    double syncAxis1Deg{0.0};
    double syncAxis2Deg{0.0};
    double syncRaDeg{0.0};
    double syncDecDeg{0.0};
    qint64 syncUtcMs{0};
    // Sky-coordinate derivative per positive ASTEP axis degree. These are
    // explicit driver config values because EQDrive installations may reverse
    // either motor in firmware. The defaults match a conventional GEM setup.
    double raSkyPerAxis{-1.0};
    double decSkyPerAxis{1.0};
    std::shared_ptr<OalBlockingSerialSession> session;
};

OalDriverHostV2 gHost{};
QMutex gMutex;
QHash<QString,Device> gDevices;
QStringList gConfiguredPorts;
QList<int> gBaudRates{115200,921600,9600,19200,38400,57600,230400,460800};
int gProbeTimeoutMs{700};
int gCommandTimeoutMs{2500};
int gOpenSettleMs{250};
double gRaSkyPerAxis{-1.0};
double gDecSkyPerAxis{1.0};
double gGuideRateDegPerHour{kSiderealDegPerHour*0.5};
double gMaxNativeGotoDeg{15.0};

char *copyString(const QByteArray &bytes){auto*p=static_cast<char*>(gHost.allocate(gHost.hostContext,std::size_t(bytes.size()+1)));if(!p)return nullptr;std::memcpy(p,bytes.constData(),std::size_t(bytes.size()));p[bytes.size()]='\0';return p;}
const char *json(const QJsonObject&o){return copyString(QJsonDocument(o).toJson(QJsonDocument::Compact));}
const char *json(const QJsonArray&a){return copyString(QJsonDocument(a).toJson(QJsonDocument::Compact));}
const char *ok(const QJsonObject&d={}){return json(QJsonObject{{"ok",true},{"data",d}});}
const char *fail(const QString&c,const QString&m){return json(QJsonObject{{"ok",false},{"error",QJsonObject{{"code",c},{"message",m}}}});}
void logMessage(int level,const QString&m){if(!gHost.log)return;const auto b=m.toUtf8();gHost.log(gHost.hostContext,level,"oal.eqdrive",b.constData());}
void emitEvent(const Device&d,const QString&type,const QJsonObject&p={}){if(!gHost.emitEvent)return;const auto b=QJsonDocument(QJsonObject{{"type",type},{"payload",p}}).toJson(QJsonDocument::Compact);const auto id=d.id.toUtf8();gHost.emitEvent(gHost.hostContext,"oal.eqdrive",id.constData(),b.constData());}

QString makeId(const QString&port){
    for(const auto&info:QSerialPortInfo::availablePorts())if(info.portName()==port||info.systemLocation()==port){const QString s=info.serialNumber().trimmed();if(!s.isEmpty())return "eqdrive:"+s;}
    QString safe=port;safe.replace('/','_').replace('\\','_').replace(':','_');return "eqdrive:"+safe;
}
void fillMetadata(Device&d){for(const auto&info:QSerialPortInfo::availablePorts())if(info.portName()==d.port||info.systemLocation()==d.port){d.serial=info.serialNumber();if(!info.description().isEmpty())d.description=info.description()+QStringLiteral(" / EQDrive");break;}}

bool likelyEqDrivePort(const QSerialPortInfo&info){
    if(info.hasVendorIdentifier()&&info.vendorIdentifier()==0x10c4)return true; // CP210x used by current EQDrive generations.
    const QString text=(info.description()+" "+info.manufacturer()).toLower();
    return text.contains("eqdrive")||text.contains("cp210");
}
QStringList candidatePorts(){
    QStringList ports=gConfiguredPorts;const QString env=qEnvironmentVariable("OAL_EQDRIVE_PORT").trimmed();if(!env.isEmpty())ports.prepend(env);
    if(ports.isEmpty())for(const auto&info:QSerialPortInfo::availablePorts())if(likelyEqDrivePort(info))ports<<info.portName();
    ports.removeDuplicates();return ports;
}

bool exchange(const Device&d,const QByteArray&cmd,QByteArray*reply,int timeoutMs){return d.session&&d.session->exchange(oal::eqdrive::line(cmd),reply,timeoutMs,true,'\r');}
std::optional<oal::eqdrive::Status> statusRaw(const Device&d){QByteArray r;if(!exchange(d,"St",&r,gCommandTimeoutMs))return std::nullopt;return oal::eqdrive::parseStatus(r);}
std::optional<std::pair<double,double>> positionRaw(const Device&d){QByteArray r;if(!exchange(d,"Pos",&r,gCommandTimeoutMs))return std::nullopt;return oal::eqdrive::parsePosition(r);}
bool commandOk(const Device&d,const QByteArray&cmd,const QByteArray&prefix){QByteArray r;return exchange(d,cmd,&r,gCommandTimeoutMs)&&oal::eqdrive::responseOk(r,prefix);}
QString f(double value,int precision=6){return QLocale::c().toString(value,'f',precision);}

QString protocolName(const Device&d){return d.protocol==WireProtocol::MotorController?QStringLiteral("skywatcher-motor-controller"):QStringLiteral("eqdrive-astep");}

bool mcExchange(const Device&d,const std::string&cmd,QByteArray*reply,int timeoutMs){
    if(!d.session)return false;
    QByteArray r;
    const QByteArray bytes(cmd.data(),int(cmd.size()));
    if(!d.session->exchange(bytes,&r,timeoutMs,true,'\r')){if(reply)*reply=r;return false;}
    if(reply)*reply=r;
    const auto rs=r.toStdString();
    return oal::skywatcher_mc::isNormalResponse(rs);
}

bool identifyMotorController(Device&d,int timeoutMs){
    QByteArray v1,v2,a1,a2,b;
    const auto q=[&](const std::string&cmd,QByteArray&reply){return mcExchange(d,cmd,&reply,timeoutMs);};
    if(!q(oal::skywatcher_mc::getVersion(1),v1))return false;
    if(!q(oal::skywatcher_mc::getVersion(2),v2))return false;
    if(!q(oal::skywatcher_mc::getCountsPerRev(1),a1))return false;
    if(!q(oal::skywatcher_mc::getCountsPerRev(2),a2))return false;
    const auto c1=oal::skywatcher_mc::decodeU24(oal::skywatcher_mc::responsePayload(a1.toStdString()));
    const auto c2=oal::skywatcher_mc::decodeU24(oal::skywatcher_mc::responsePayload(a2.toStdString()));
    if(!c1||!c2||*c1==0||*c2==0)return false;
    d.countsPerRev1=*c1;d.countsPerRev2=*c2;
    if(q(oal::skywatcher_mc::getTimerFrequency(1),b)){
        const auto t=oal::skywatcher_mc::decodeU24(oal::skywatcher_mc::responsePayload(b.toStdString()));
        if(t)d.timerFreq=*t;
    }
    d.protocol=WireProtocol::MotorController;
    d.firmware=QString("EQMOD/Sky-Watcher MC axis1=%1 axis2=%2")
                   .arg(QString::fromStdString(oal::skywatcher_mc::responsePayload(v1.toStdString())))
                   .arg(QString::fromStdString(oal::skywatcher_mc::responsePayload(v2.toStdString())));
    logMessage(1,QString("%1 @ %2: EQMOD-compatible Motor Controller protocol verified; counts/rev=%3,%4 timer=%5")
                     .arg(d.port).arg(d.baudRate).arg(d.countsPerRev1).arg(d.countsPerRev2).arg(d.timerFreq));
    return true;
}

bool mcReadAxis(const Device&d,int axis,double&axisDeg,bool&running,bool&gotoMode,bool&initialized,QString*error=nullptr){
    QByteArray p,st;
    if(!mcExchange(d,oal::skywatcher_mc::getPosition(axis),&p,gCommandTimeoutMs)||
       !mcExchange(d,oal::skywatcher_mc::getStatus(axis),&st,gCommandTimeoutMs)){
        if(error)*error=d.session?d.session->lastExchangeDiagnostic():QString("serial session unavailable");
        return false;
    }
    const auto pos=oal::skywatcher_mc::decodePosition(p.toStdString());
    const auto status=oal::skywatcher_mc::parseStatus(st.toStdString());
    if(!pos||!status.valid){if(error)*error="Could not parse EQMOD-compatible motor-controller axis state";return false;}
    const double cpr=axis==1?double(d.countsPerRev1):double(d.countsPerRev2);
    if(cpr<=0){if(error)*error="Invalid motor-controller counts-per-revolution";return false;}
    axisDeg=double(*pos)*360.0/cpr;running=status.running;gotoMode=status.gotoMode;initialized=status.initialized;return true;
}

std::optional<std::pair<double,double>> mcPositionRaw(const Device&d){
    double a1=0,a2=0;bool r=false,g=false,i=false;QString e;
    if(!mcReadAxis(d,1,a1,r,g,i,&e)||!mcReadAxis(d,2,a2,r,g,i,&e))return std::nullopt;
    return std::pair<double,double>{a1,a2};
}
std::optional<std::pair<double,double>> positionAny(const Device&d){
    return d.protocol==WireProtocol::MotorController?mcPositionRaw(d):positionRaw(d);
}

bool mcStopAxis(const Device&d,int axis,QString*error=nullptr){
    QByteArray r;if(mcExchange(d,oal::skywatcher_mc::stop(axis),&r,gCommandTimeoutMs))return true;
    if(error)*error=d.session?d.session->lastExchangeDiagnostic():QString("serial session unavailable");return false;
}

bool mcWaitStopped(const Device&d,int axis,int timeoutMs,QString*error=nullptr){
    QElapsedTimer t;t.start();while(t.elapsed()<timeoutMs){double deg=0;bool running=false,g=false,i=false;if(!mcReadAxis(d,axis,deg,running,g,i,error))return false;if(!running)return true;QThread::msleep(60);}
    if(error)*error=QString("EQDrive axis %1 did not stop in time").arg(axis);return false;
}

bool mcGotoAxisDelta(const Device&d,int axis,double deltaDeg,QString*error=nullptr){
    if(std::abs(deltaDeg)<1e-5)return true;
    const double cpr=axis==1?double(d.countsPerRev1):double(d.countsPerRev2);
    if(cpr<=0){if(error)*error="Invalid motor-controller axis scale";return false;}
    double beforeDeg=0;bool rr=false,gg=false,ii=false;if(!mcReadAxis(d,axis,beforeDeg,rr,gg,ii,error))return false;
    if(!mcStopAxis(d,axis,nullptr)||!mcWaitStopped(d,axis,2500,error))return false;
    const quint32 counts=quint32(std::max(1.0,std::round(std::abs(deltaDeg)*cpr/360.0)));
    const bool forward=deltaDeg>0;
    QByteArray reply;
    const auto send=[&](const std::string&cmd){if(mcExchange(d,cmd,&reply,gCommandTimeoutMs))return true;if(error)*error=d.session->lastExchangeDiagnostic();return false;};
    // Match EQMOD/INDI's SlewTo command sequence. Step-period programming is
    // intentionally omitted here; it is for continuous slew/tracking mode.
    if(!send(oal::skywatcher_mc::setMotionMode(axis,true,false,forward)))return false;
    if(!send(oal::skywatcher_mc::setGotoIncrement(axis,counts)))return false;
    const quint32 brake=std::min<quint32>(counts,200u);
    if(!send(oal::skywatcher_mc::setBrakeIncrement(axis,brake)))return false;
    if(!send(oal::skywatcher_mc::startMotion(axis)))return false;
    QElapsedTimer verify;verify.start();while(verify.elapsed()<900){QThread::msleep(60);double now=beforeDeg;bool running=false,gotoMode=false,init=false;QString probe;if(!mcReadAxis(d,axis,now,running,gotoMode,init,&probe))continue;if((running&&gotoMode)||std::abs(now-beforeDeg)>1e-4)return true;}
    if(error)*error=QString("EQDrive axis %1 acknowledged GOTO but encoder did not move").arg(axis);return false;
}

bool mcInitializeAxes(const Device&d,QString*error=nullptr){
    for(int axis=1;axis<=2;++axis){
        double deg=0;bool running=false,gotoMode=false,initialized=false;
        QString e;
        if(!mcReadAxis(d,axis,deg,running,gotoMode,initialized,&e)){if(error)*error=e;return false;}
        if(initialized)continue;
        QByteArray reply;
        if(!mcExchange(d,oal::skywatcher_mc::initDone(axis),&reply,gCommandTimeoutMs)){if(error)*error=d.session->lastExchangeDiagnostic();return false;}
    }
    return true;
}

bool mcSetTracking(const Device&d,bool enabled,QString*error=nullptr){
    if(!enabled)return mcStopAxis(d,1,error);
    if(d.countsPerRev1==0||d.timerFreq==0){if(error)*error="Motor Controller did not report timer/axis scale required for tracking";return false;}
    if(!mcStopAxis(d,1,nullptr)||!mcWaitStopped(d,1,2500,error))return false;
    const double cpr=double(d.countsPerRev1);
    const quint32 period=quint32(std::clamp(std::round(double(d.timerFreq)*360.0/(cpr*kSiderealDegPerHour/3600.0)),6.0,double(0xFFFFFF)));
    QByteArray reply;
    const auto send=[&](const std::string&cmd){if(mcExchange(d,cmd,&reply,gCommandTimeoutMs))return true;if(error)*error=d.session->lastExchangeDiagnostic();return false;};
    const char direction=d.raSkyPerAxis<0?'1':'0';
    if(!send(oal::skywatcher_mc::command('G',1,std::string("1")+direction)))return false;
    if(!send(oal::skywatcher_mc::setStepPeriod(1,period)))return false;
    return send(oal::skywatcher_mc::startMotion(1));
}

bool identify(Device&d){
    // Canonical EQDrive probe: use the documented ASTEP *mount* section
    // first.  St/Pos/Cg are read-only and are explicitly PC>EQD commands in
    // the public EQDrive protocol.  Firmware commands are supplemental only;
    // older documentation labels them PC>FCD even though EQDrive Config may
    // expose equivalent identity data.
    bool mountSectionSeen=false;
    QByteArray reply;
    const auto read=[&](const QByteArray&cmd,int timeout){
        reply.clear();
        const bool received=exchange(d,cmd,&reply,timeout);
        if(received)logMessage(1,QString("%1 @ %2: ASTEP %3 -> '%4' (hex %5)")
                                   .arg(d.port).arg(d.baudRate).arg(QString::fromLatin1(cmd))
                                   .arg(QString::fromLatin1(oal::eqdrive::trimLine(reply)))
                                   .arg(QString::fromLatin1(reply.toHex(' '))));
        return received;
    };

    // The official examples are:
    //   St 0101 <Pos1> <Pos2> <Speed1> <Speed2> <unix-time>\r
    //   Pos <axis1-deg> <axis2-deg> OK\r
    // Either one is a strong, movement-free EQDrive mount fingerprint.
    if(read("St",gProbeTimeoutMs)&&oal::eqdrive::parseStatus(reply))mountSectionSeen=true;
    if(!mountSectionSeen&&read("Pos",gProbeTimeoutMs)&&oal::eqdrive::parsePosition(reply))mountSectionSeen=true;

    // Cg is also read-only.  Besides strengthening identification, it tells us
    // whether either motor is configured Reversed in EQDrive itself.  Those
    // signs are telemetry; OpenAstroLink does not silently apply a second
    // reversal on top of controller firmware.
    if(read("Cg",gProbeTimeoutMs)){
        const auto t=oal::eqdrive::trimLine(reply);
        if(t.startsWith("Cg ")){
            const auto dirs=oal::eqdrive::parseAxisDirections(reply);
            d.configuredAxisDirection1=dirs.first;
            d.configuredAxisDirection2=dirs.second;
        }
    }

    // Identity is useful in diagnostics but is not required to expose the
    // mount.  The public page has historically shown these general commands
    // with FCD labels, so mount-specific St/Pos remain authoritative.
    if(read("FWx",gProbeTimeoutMs)&&oal::eqdrive::trimLine(reply).toLower().contains("eqdrive"))
        d.firmware=oal::eqdrive::firmwareText(reply);
    if(d.firmware.isEmpty()&&read("FWs",gProbeTimeoutMs)&&oal::eqdrive::looksLikeEqDrive(reply))
        d.firmware=oal::eqdrive::firmwareText(reply);
    if(d.firmware.isEmpty()&&read("FW",gProbeTimeoutMs)&&oal::eqdrive::looksLikeEqDrive(reply))
        d.firmware=oal::eqdrive::firmwareText(reply);

    if(!mountSectionSeen)return false;
    d.protocol=WireProtocol::Astep;
    if(d.firmware.isEmpty())d.firmware=QStringLiteral("EQDrive ASTEP mount");
    logMessage(1,QString("%1: ASTEP mount section verified; controller axis directions RA=%2 DEC=%3")
                     .arg(d.port).arg(d.configuredAxisDirection1<0?"reversed":"normal")
                     .arg(d.configuredAxisDirection2<0?"reversed":"normal"));
    return true;
}

enum class ProbeResult { NotFound, Found, Busy };

ProbeResult probePort(const QString&port,Device&out,bool keepOpen){
    const bool focused=!qEnvironmentVariable("OAL_EQDRIVE_PORT").trimmed().isEmpty()||gConfiguredPorts.contains(port);
    struct Attempt{int baud;bool dtr;bool rts;};

    QList<Attempt> mcAttempts;
    // EQMOD-compatible EQDrive firmware is the proven mount-control path on the
    // Standard5N HIL controller. Probe the two common EQMOD rates first so a
    // missing/unsupported device cannot stall node startup for tens of seconds.
    // Extra legacy/high-rate attempts are only used for an explicitly selected
    // port; modem-line permutations are deliberately omitted unless a future HIL
    // device proves they are necessary.
    const QList<int> fastBauds=focused?QList<int>{115200,9600,19200,921600}:QList<int>{115200,9600};
    for(int baud:fastBauds)mcAttempts.append({baud,false,false});

    const auto make=[&](const Attempt&a){
        Device d;d.port=port.trimmed();d.id=makeId(d.port);d.baudRate=a.baud;d.dtr=a.dtr;d.rts=a.rts;
        d.raSkyPerAxis=gRaSkyPerAxis;d.decSkyPerAxis=gDecSkyPerAxis;fillMetadata(d);
        d.session=std::make_shared<OalBlockingSerialSession>();return d;
    };

    for(const auto&a:mcAttempts){
        Device d=make(a);QString error;
        logMessage(1,QString("probing %1 for EQMOD-compatible Motor Controller at %2 baud (DTR=%3 RTS=%4)")
                     .arg(d.port).arg(a.baud).arg(a.dtr?"on":"off").arg(a.rts?"on":"off"));
        if(!d.session->open(d.port,a.baud,std::min(gOpenSettleMs,250),&error,a.dtr,a.rts)){
            logMessage(2,QString("%1: open failed: %2").arg(d.port,error));
            if(error.contains("Access is denied",Qt::CaseInsensitive)||error.contains("Permission",Qt::CaseInsensitive)){
                logMessage(2,QString("%1 is already owned by another process (commonly EQMOD/Classic ASCOM or EQDrive Config); native EQDrive discovery will not fight for the port").arg(d.port));
                return ProbeResult::Busy;
            }
            continue;
        }
        if(identifyMotorController(d,350)){
            logMessage(1,QString("%1: native EQDrive discovered via EQMOD-compatible Motor Controller protocol (%2 baud)").arg(d.port).arg(a.baud));
            if(!keepOpen){d.session->close();d.session.reset();}out=std::move(d);return ProbeResult::Found;
        }
        d.session->close();
    }

    // ASTEP remains a supported direct protocol where firmware exposes it on
    // the control VCP, but it is now a fallback rather than the only path.
    const QList<int> astepBauds=focused?QList<int>{115200,9600}:QList<int>{115200};
    for(int baud:astepBauds){
        Device d=make({baud,false,false});QString error;
        logMessage(1,QString("probing %1 for official EQDrive ASTEP at %2 baud").arg(d.port).arg(baud));
        if(!d.session->open(d.port,baud,std::min(gOpenSettleMs,350),&error,false,false)){
            if(error.contains("Access is denied",Qt::CaseInsensitive)||error.contains("Permission",Qt::CaseInsensitive))return ProbeResult::Busy;
            continue;
        }
        if(identify(d)){
            logMessage(1,QString("%1: EQDrive ASTEP discovered (%2, %3 baud)").arg(d.port,d.firmware).arg(baud));
            if(!keepOpen){d.session->close();d.session.reset();}out=std::move(d);return ProbeResult::Found;
        }
        d.session->close();
    }
    return ProbeResult::NotFound;
}

std::optional<Device> getDevice(const QString&id){QMutexLocker l(&gMutex);auto it=gDevices.find(id);if(it==gDevices.end())return std::nullopt;return it.value();}
void updateDevice(const Device&d){QMutexLocker l(&gMutex);gDevices[d.id]=d;}
std::optional<Device> connectedForPort(const QString&p){QMutexLocker l(&gMutex);for(const auto&d:gDevices)if(d.connected&&d.port==p&&d.session&&d.session->isOpen())return d;return std::nullopt;}
void copyRuntime(Device&d,const Device&old){d.connected=old.connected;d.coordinateSynced=old.coordinateSynced;d.syncAxis1Deg=old.syncAxis1Deg;d.syncAxis2Deg=old.syncAxis2Deg;d.syncRaDeg=old.syncRaDeg;d.syncDecDeg=old.syncDecDeg;d.syncUtcMs=old.syncUtcMs;}
void refreshDevices(){
    QHash<QString,Device> old;{QMutexLocker l(&gMutex);old=gDevices;}
    QHash<QString,Device> found;
    for(const auto&p:candidatePorts()){
        if(auto e=connectedForPort(p)){found.insert(e->id,*e);continue;}
        Device d;const auto result=probePort(p,d,false);
        if(result==ProbeResult::Found){if(old.contains(d.id))copyRuntime(d,old.value(d.id));found.insert(d.id,d);}
        else if(result==ProbeResult::Busy){for(const auto&o:old)if(o.port==p)found.insert(o.id,o);}
    }
    // Preserve a previously verified descriptor while its USB serial port still
    // exists.  This keeps the native choice visible if EQMOD temporarily owns
    // COM5; connect will still return a clear BUSY/open error.
    for(const auto&o:old){if(found.contains(o.id))continue;for(const auto&i:QSerialPortInfo::availablePorts())if(i.portName()==o.port||i.systemLocation()==o.port){found.insert(o.id,o);break;}}
    QMutexLocker l(&gMutex);gDevices=found;
}

std::pair<double,double> skyFromAxes(const Device&d,double a1,double a2,qint64 nowMs){
    if(!d.coordinateSynced)return {0.0,0.0};
    const double elapsedHours=double(nowMs-d.syncUtcMs)/3600000.0;
    const double earthDeg=elapsedHours*kSiderealDegPerHour;
    const double ra=oal::eqdrive::wrap360(d.syncRaDeg+earthDeg+d.raSkyPerAxis*(a1-d.syncAxis1Deg));
    const double dec=std::clamp(d.syncDecDeg+d.decSkyPerAxis*(a2-d.syncAxis2Deg),-90.0,90.0);
    return {ra,dec};
}

bool start(void*,const char*configJson){
    const auto o=QJsonDocument::fromJson(QByteArray(configJson?configJson:"{}")).object();gConfiguredPorts.clear();for(const auto&v:o.value("ports").toArray())if(v.isString())gConfiguredPorts<<v.toString();gConfiguredPorts.removeDuplicates();
    if(o.value("baudRates").isArray()){QList<int>x;for(const auto&v:o.value("baudRates").toArray()){int b=v.toInt();if(b>=1200&&b<=1000000&&!x.contains(b))x<<b;}if(!x.isEmpty())gBaudRates=x;}
    gProbeTimeoutMs=std::clamp(o.value("probeTimeoutMs").toInt(gProbeTimeoutMs),200,5000);gCommandTimeoutMs=std::clamp(o.value("commandTimeoutMs").toInt(gCommandTimeoutMs),250,10000);gOpenSettleMs=std::clamp(o.value("openSettleMs").toInt(gOpenSettleMs),0,3000);
    gRaSkyPerAxis=o.value("raSkyPerAxis").toDouble(gRaSkyPerAxis);gDecSkyPerAxis=o.value("decSkyPerAxis").toDouble(gDecSkyPerAxis);gGuideRateDegPerHour=o.value("guideRateDegPerHour").toDouble(gGuideRateDegPerHour);gMaxNativeGotoDeg=std::clamp(o.value("maxNativeGotoDeg").toDouble(gMaxNativeGotoDeg),0.1,180.0);
    logMessage(1,QString("config: EQMOD-compatible Motor Controller first, ASTEP fallback; baudRates=%1 openSettleMs=%2 raSkyPerAxis=%3 decSkyPerAxis=%4 maxNativeGotoDeg=%5").arg([&]{QStringList x;for(int b:gBaudRates)x<<QString::number(b);return x.join(',');}()).arg(gOpenSettleMs).arg(gRaSkyPerAxis).arg(gDecSkyPerAxis).arg(gMaxNativeGotoDeg));return true;
}
void stop(void*){QMutexLocker l(&gMutex);for(auto it=gDevices.begin();it!=gDevices.end();++it){if(it.value().session)it.value().session->close();it.value().session.reset();it.value().connected=false;}}

const char*manifest(void*){return json(QJsonObject{{"driverId","oal.eqdrive"},{"name","OpenAstroLink native EQDrive dual-protocol driver"},{"version","0.2.10.25"},{"abiVersion",2},{"threadModel","per-device-serial"},{"protocol","Sky-Watcher/EQMOD Motor Controller + EQDrive ASTEP"}});}
const char*devices(void*){refreshDevices();QJsonArray a;QMutexLocker l(&gMutex);for(const auto&d:gDevices)a.append(QJsonObject{{"id",d.id},{"type","mount"},{"name",d.description},{"serialNumber",d.serial},{"firmware",d.firmware},{"transport",QJsonObject{{"kind","serial"},{"protocol",protocolName(d)},{"port",d.port},{"baudRate",d.baudRate},{"dtr",d.dtr},{"rts",d.rts},{"persistentSession",d.connected}}}});return json(a);}
const char*capabilities(void*,const char*deviceId){const auto d=getDevice(QString::fromUtf8(deviceId?deviceId:""));if(!d)return json(QJsonObject{});const bool mc=d->protocol==WireProtocol::MotorController;return json(QJsonObject{{"schemaVersion","1.0"},{"mount",QJsonObject{{"position",QJsonObject{{"supported",true},{"frame","raw-axis-plus-compat-sync-anchor"},{"requiresSync",true}}},{"rawAxes",QJsonObject{{"supported",true},{"units","degrees"},{"axis1","hour/azimuth axis"},{"axis2","declination/altitude axis"}}},{"slew",QJsonObject{{"supported",true},{"abortSupported",true},{"requiresSync",true},{"implementation",mc?"raw-axis EQMOD-compatible Motor Controller goto":"raw-axis official ASTEP Goto"}}},{"sync",QJsonObject{{"supported",true},{"implementation","compatibility sync anchor; OAL Core geometry preferred"}}},{"tracking",QJsonObject{{"supported",true},{"modes",QJsonArray{"off","equatorial"}}}},{"park",QJsonObject{{"supported",false}}},{"pulseGuide",QJsonObject{{"supported",!mc},{"implementation",mc?"not yet enabled for Motor Controller transport":"temporary ASTEP Speed offset"},{"maxDurationMs",5000}}},{"pierSide",QJsonObject{{"supported",false}}},{"telemetry",QJsonObject{{"axisDegrees",true},{"driverState",true},{"firmware",d->firmware},{"wireProtocol",protocolName(*d)}}},{"coordinateModel",QJsonObject{{"type","sync-anchor-v3"},{"meridianFlip",false},{"note","Sync once on a known sky position before native RA/DEC GOTO; no automatic pier flip until HIL qualification"}}}}}});}
const char*health(void*,const char*deviceId){const auto d=getDevice(QString::fromUtf8(deviceId?deviceId:""));if(!d)return json(QJsonObject{{"state","missing"},{"connected",false}});return json(QJsonObject{{"state",d->connected?"ok":"disconnected"},{"connected",d->connected},{"port",d->port},{"baudRate",d->baudRate},{"firmware",d->firmware},{"coordinateSynced",d->coordinateSynced}});}

const char*invoke(void*,const char*deviceId,const char*method,const char*requestJson,const OalDriverCallV2*){
    const QString id=QString::fromUtf8(deviceId?deviceId:"");const QString m=QString::fromUtf8(method?method:"");auto od=getDevice(id);if(!od)return fail("DEVICE_NOT_FOUND","EQDrive is no longer present");Device d=*od;const auto req=QJsonDocument::fromJson(QByteArray(requestJson?requestJson:"{}")).object();
    if(m=="device.connect"){
        Device c;const auto pr=probePort(d.port,c,true);if(pr!=ProbeResult::Found)return fail(pr==ProbeResult::Busy?"PORT_BUSY":"CONNECT_FAILED",pr==ProbeResult::Busy?"EQDrive serial port is already owned by another application":"No supported native EQDrive protocol responded on "+d.port);c.connected=true;if(c.protocol==WireProtocol::MotorController){QString initError;if(!mcInitializeAxes(c,&initError)){if(c.session)c.session->close();return fail("CONNECT_FAILED","EQDrive Motor Controller initialization failed: "+initError);}}c.coordinateSynced=d.coordinateSynced;c.syncAxis1Deg=d.syncAxis1Deg;c.syncAxis2Deg=d.syncAxis2Deg;c.syncRaDeg=d.syncRaDeg;c.syncDecDeg=d.syncDecDeg;c.syncUtcMs=d.syncUtcMs;updateDevice(c);emitEvent(c,"device.connected");return ok(QJsonObject{{"port",c.port},{"baudRate",c.baudRate},{"firmware",c.firmware},{"coordinateSynced",c.coordinateSynced}});
    }
    if(m=="device.disconnect"){if(d.session)d.session->close();d.session.reset();d.connected=false;updateDevice(d);emitEvent(d,"device.disconnected");return ok();}
    if(!d.connected||!d.session||!d.session->isOpen())return fail("DEVICE_DISCONNECTED","EQDrive is not connected");
    if(m=="mount.status"){
        if(d.protocol==WireProtocol::MotorController){
            double a1=0,a2=0;bool r1=false,r2=false,g1=false,g2=false,i1=false,i2=false;QString err;
            if(!mcReadAxis(d,1,a1,r1,g1,i1,&err)||!mcReadAxis(d,2,a2,r2,g2,i2,&err))return fail("TRANSPORT_ERROR",err);
            const auto sky=skyFromAxes(d,a1,a2,QDateTime::currentMSecsSinceEpoch());
            return ok(QJsonObject{{"raDeg",sky.first},{"decDeg",sky.second},{"coordinateFrame","sync-anchor-relative"},{"coordinateValid",d.coordinateSynced},{"tracking",(r1&&!g1)},{"slewing",(r1&&g1)||(r2&&g2)},{"parked",false},{"parkSupported",false},{"pierSide","unknown"},{"axis1Deg",a1},{"axis2Deg",a2},{"initialized1",i1},{"initialized2",i2},{"wireProtocol",protocolName(d)},{"coordinateSynced",d.coordinateSynced}});
        }
        const auto st=statusRaw(d);if(!st)return fail("TRANSPORT_ERROR","Could not read EQDrive St telemetry");const auto sky=skyFromAxes(d,st->axis1Deg,st->axis2Deg,QDateTime::currentMSecsSinceEpoch());
        return ok(QJsonObject{{"raDeg",sky.first},{"decDeg",sky.second},{"coordinateFrame","sync-anchor-relative"},{"coordinateValid",d.coordinateSynced},{"tracking",std::abs(st->speed1DegPerHour)>0.05&&!st->gotoActive1},{"slewing",st->gotoActive1||st->gotoActive2},{"parked",false},{"parkSupported",false},{"pierSide","unknown"},{"axis1Deg",st->axis1Deg},{"axis2Deg",st->axis2Deg},{"speed1DegPerHour",st->speed1DegPerHour},{"speed2DegPerHour",st->speed2DegPerHour},{"driverEnabled1",st->driverEnabled1},{"driverEnabled2",st->driverEnabled2},{"wireProtocol",protocolName(d)},{"coordinateSynced",d.coordinateSynced}});
    }
    if(m=="mount.axisStatus"){
        if(d.protocol==WireProtocol::MotorController){
            double a1=0,a2=0;bool r1=false,r2=false,g1=false,g2=false,i1=false,i2=false;QString err;
            if(!mcReadAxis(d,1,a1,r1,g1,i1,&err)||!mcReadAxis(d,2,a2,r2,g2,i2,&err))return fail("TRANSPORT_ERROR",err);
            return ok(QJsonObject{{"axis1Deg",a1},{"axis2Deg",a2},{"running1",r1},{"running2",r2},{"goto1",g1},{"goto2",g2},{"initialized1",i1},{"initialized2",i2},{"wireProtocol",protocolName(d)}});
        }
        const auto st=statusRaw(d);if(!st)return fail("TRANSPORT_ERROR","Could not read EQDrive St telemetry");
        return ok(QJsonObject{{"axis1Deg",st->axis1Deg},{"axis2Deg",st->axis2Deg},{"running1",std::abs(st->speed1DegPerHour)>0.05||st->gotoActive1},{"running2",std::abs(st->speed2DegPerHour)>0.05||st->gotoActive2},{"goto1",st->gotoActive1},{"goto2",st->gotoActive2},{"wireProtocol",protocolName(d)}});
    }
    if(m=="mount.gotoAxes"){
        const double targetAxis1=req.value("axis1Deg").toDouble(1e100),targetAxis2=req.value("axis2Deg").toDouble(1e100);
        if(!std::isfinite(targetAxis1)||!std::isfinite(targetAxis2)||std::abs(targetAxis1)>1e6||std::abs(targetAxis2)>1e6)return fail("INVALID_AXIS_COORDINATE","Invalid mechanical axis target");
        const auto pos=positionAny(d);if(!pos)return fail("TRANSPORT_ERROR","Could not read EQDrive axis position before raw-axis GOTO");
        auto wrap180=[](double x){x=std::fmod(x,360.0);if(x>180.0)x-=360.0;if(x<-180.0)x+=360.0;return x;};
        const double delta1=wrap180(targetAxis1-pos->first),delta2=wrap180(targetAxis2-pos->second);
        const double maxAxis=req.value("maxAxisDeltaDeg").toDouble(180.0);
        if(std::max(std::abs(delta1),std::abs(delta2))>maxAxis)return fail("AXIS_SAFETY_LIMIT",QString("Raw-axis GOTO exceeds safety limit %1 deg: d1=%2 d2=%3").arg(maxAxis).arg(delta1,0,'f',3).arg(delta2,0,'f',3));
        if(d.protocol==WireProtocol::MotorController){QString err;if(!mcGotoAxisDelta(d,1,delta1,&err))return fail("TRANSPORT_ERROR",err);if(!mcGotoAxisDelta(d,2,delta2,&err)){mcStopAxis(d,1,nullptr);return fail("TRANSPORT_ERROR",err);}}
        else {const QByteArray cmd=QString("Goto %1 %2").arg(f(pos->first+delta1),f(pos->second+delta2)).toLatin1();if(!commandOk(d,cmd,"Goto"))return fail("TRANSPORT_ERROR","EQDrive rejected raw-axis ASTEP Goto command");}
        emitEvent(d,"mount.rawAxisSlewStarted",{{"axis1Deg",targetAxis1},{"axis2Deg",targetAxis2},{"axis1DeltaDeg",delta1},{"axis2DeltaDeg",delta2},{"wireProtocol",protocolName(d)}});
        return ok(QJsonObject{{"accepted",true},{"axis1Deg",targetAxis1},{"axis2Deg",targetAxis2},{"axis1DeltaDeg",delta1},{"axis2DeltaDeg",delta2}});
    }
    if(m=="mount.sync"){
        const double ra=req.value("raDeg").toDouble(-1),dec=req.value("decDeg").toDouble(999);if(ra<0||ra>=360||dec<-90||dec>90)return fail("INVALID_COORDINATE","RA/DEC outside valid range");const auto p=positionAny(d);if(!p)return fail("TRANSPORT_ERROR","Could not read EQDrive axis position for sync");d.coordinateSynced=true;d.syncAxis1Deg=p->first;d.syncAxis2Deg=p->second;d.syncRaDeg=ra;d.syncDecDeg=dec;d.syncUtcMs=QDateTime::currentMSecsSinceEpoch();updateDevice(d);emitEvent(d,"mount.synced",{{"raDeg",ra},{"decDeg",dec},{"axis1Deg",p->first},{"axis2Deg",p->second}});return ok(QJsonObject{{"coordinateSynced",true},{"axis1Deg",p->first},{"axis2Deg",p->second}});
    }
    if(m=="mount.slew"){
        if(!d.coordinateSynced)return fail("MOUNT_NOT_SYNCED","Native EQDrive RA/DEC GOTO requires one Sync on a known sky position first");
        const double ra=req.value("raDeg").toDouble(-1),dec=req.value("decDeg").toDouble(999);if(ra<0||ra>=360||dec<-90||dec>90)return fail("INVALID_COORDINATE","RA/DEC outside valid range");
        const auto pos=positionAny(d);if(!pos)return fail("TRANSPORT_ERROR","Could not read EQDrive axis position before GOTO");
        const auto sky=skyFromAxes(d,pos->first,pos->second,QDateTime::currentMSecsSinceEpoch());
        const double dRa=oal::eqdrive::wrap180(ra-sky.first),dDec=dec-sky.second;
        if(std::max(std::abs(dRa),std::abs(dDec))>gMaxNativeGotoDeg)
            return fail("HIL_SAFETY_LIMIT",QString("Native EQDrive GOTO is temporarily limited to %1 deg per command while axis/pier direction is being qualified; requested dRA=%2 deg dDEC=%3 deg").arg(gMaxNativeGotoDeg).arg(dRa,0,'f',3).arg(dDec,0,'f',3));
        const double axisDelta1=dRa/d.raSkyPerAxis,axisDelta2=dDec/d.decSkyPerAxis;
        if(d.protocol==WireProtocol::MotorController){
            QString err;
            if(!mcGotoAxisDelta(d,1,axisDelta1,&err))return fail("TRANSPORT_ERROR",err);
            if(!mcGotoAxisDelta(d,2,axisDelta2,&err)){mcStopAxis(d,1,nullptr);return fail("TRANSPORT_ERROR",err);}
            emitEvent(d,"mount.slewStarted",{{"raDeg",ra},{"decDeg",dec},{"axis1DeltaDeg",axisDelta1},{"axis2DeltaDeg",axisDelta2},{"wireProtocol",protocolName(d)}});
            return ok(QJsonObject{{"accepted",true},{"raDeg",ra},{"decDeg",dec},{"axis1DeltaDeg",axisDelta1},{"axis2DeltaDeg",axisDelta2},{"wireProtocol",protocolName(d)}});
        }
        const double targetAxis1=pos->first+axisDelta1;
        const double targetAxis2=pos->second+axisDelta2;
        const QByteArray cmd=QString("Goto %1 %2").arg(f(targetAxis1),f(targetAxis2)).toLatin1();
        if(!commandOk(d,cmd,"Goto"))return fail("TRANSPORT_ERROR","EQDrive rejected official ASTEP Goto command");
        emitEvent(d,"mount.slewStarted",{{"raDeg",ra},{"decDeg",dec},{"axis1TargetDeg",targetAxis1},{"axis2TargetDeg",targetAxis2},{"configuredDirection1",d.configuredAxisDirection1},{"configuredDirection2",d.configuredAxisDirection2},{"wireProtocol",protocolName(d)}});
        return ok(QJsonObject{{"accepted",true},{"raDeg",ra},{"decDeg",dec},{"axis1TargetDeg",targetAxis1},{"axis2TargetDeg",targetAxis2},{"wireProtocol",protocolName(d)}});
    }
    if(m=="mount.abort"){
        if(d.protocol==WireProtocol::MotorController){
            QString e1,e2;const bool a=mcStopAxis(d,1,&e1),b=mcStopAxis(d,2,&e2);if(!a||!b)return fail("TRANSPORT_ERROR",!e1.isEmpty()?e1:e2);emitEvent(d,"mount.motionAborted");return ok(QJsonObject{{"method","motor-controller-stop"}});
        }
        const auto p=positionRaw(d);if(!p)return fail("TRANSPORT_ERROR","Could not read axis position for abort");const QByteArray cmd=QString("Goto %1 %2").arg(f(p->first),f(p->second)).toLatin1();if(!commandOk(d,cmd,"Goto"))return fail("TRANSPORT_ERROR","EQDrive best-effort abort (retarget current axis position) failed");emitEvent(d,"mount.motionAborted");return ok(QJsonObject{{"method","retarget-current-axis-position"}});
    }
    if(m=="mount.setTracking"){
        const bool enabled=req.value("enabled").toBool();
        if(d.protocol==WireProtocol::MotorController){
            Device td=d;const int axisDirection=req.value("axis1Direction").toInt(0);if(axisDirection!=0)td.raSkyPerAxis=axisDirection>0?1.0:-1.0;
            QString err;if(!mcSetTracking(td,enabled,&err))return fail("TRANSPORT_ERROR",err);emitEvent(d,"mount.tracking",{{"enabled",enabled},{"axis1Direction",axisDirection},{"wireProtocol",protocolName(d)}});return ok();}
        const QByteArray cmd=QString("Speed %1 0.000000").arg(f(enabled?kSiderealDegPerHour:0.0)).toLatin1();if(!commandOk(d,cmd,"Speed"))return fail("TRANSPORT_ERROR","EQDrive tracking Speed command failed");emitEvent(d,"mount.tracking",{{"enabled",enabled}});return ok();
    }
    if(m=="mount.pulseGuide"){
        if(d.protocol==WireProtocol::MotorController)return fail("NOT_SUPPORTED","Pulse guide is not enabled yet for native EQDrive Motor Controller transport");
        const QString dir=req.value("direction").toString();const int ms=std::clamp(req.value("durationMs").toInt(),1,5000);QByteArray rr;if(!exchange(d,"Speed",&rr,gCommandTimeoutMs))return fail("TRANSPORT_ERROR","Could not read EQDrive tracking speed");const auto sp=oal::eqdrive::parseSpeed(rr);if(!sp)return fail("TRANSPORT_ERROR","Could not parse EQDrive Speed response");double s1=sp->first,s2=sp->second;if(dir=="east")s1+=gGuideRateDegPerHour;else if(dir=="west")s1-=gGuideRateDegPerHour;else if(dir=="north")s2+=gGuideRateDegPerHour;else if(dir=="south")s2-=gGuideRateDegPerHour;else return fail("INVALID_DIRECTION","direction must be north/south/east/west");const QByteArray guide=QString("Speed %1 %2").arg(f(s1),f(s2)).toLatin1();if(!commandOk(d,guide,"Speed"))return fail("TRANSPORT_ERROR","Could not apply EQDrive guide speed");QThread::msleep(unsigned(ms));const QByteArray restore=QString("Speed %1 %2").arg(f(sp->first),f(sp->second)).toLatin1();if(!commandOk(d,restore,"Speed"))return fail("TRANSPORT_ERROR","Could not restore EQDrive tracking speed after pulse guide");return ok();
    }
    if(m=="mount.park")return fail("NOT_SUPPORTED","Native EQDrive park/meridian model is not exposed until the full pier-side model is qualified");
    if(m=="eqdrive.telemetry"){
        if(d.protocol==WireProtocol::MotorController){
            const auto p=mcPositionRaw(d);if(!p)return fail("TRANSPORT_ERROR","Could not read Motor Controller telemetry");
            return ok(QJsonObject{{"firmware",d.firmware},{"port",d.port},{"baudRate",d.baudRate},{"wireProtocol",protocolName(d)},{"axis1Deg",p->first},{"axis2Deg",p->second},{"countsPerRev1",int(d.countsPerRev1)},{"countsPerRev2",int(d.countsPerRev2)},{"timerFreq",int(d.timerFreq)}});
        }
        const auto st=statusRaw(d);if(!st)return fail("TRANSPORT_ERROR","Could not read EQDrive telemetry");QByteArray vin,vbs,cg;exchange(d,"Vin",&vin,gCommandTimeoutMs);exchange(d,"Vbs",&vbs,gCommandTimeoutMs);exchange(d,"Cg",&cg,gCommandTimeoutMs);return ok(QJsonObject{{"firmware",d.firmware},{"port",d.port},{"baudRate",d.baudRate},{"wireProtocol",protocolName(d)},{"configuredDirection1",d.configuredAxisDirection1},{"configuredDirection2",d.configuredAxisDirection2},{"axis1Deg",st->axis1Deg},{"axis2Deg",st->axis2Deg},{"speed1DegPerHour",st->speed1DegPerHour},{"speed2DegPerHour",st->speed2DegPerHour},{"stateHex",QString::number(st->state,16).rightJustified(4,'0')},{"vin",QString::fromLatin1(oal::eqdrive::trimLine(vin))},{"vbs",QString::fromLatin1(oal::eqdrive::trimLine(vbs))},{"config",QString::fromLatin1(oal::eqdrive::trimLine(cg))}});
    }
    return fail("METHOD_NOT_SUPPORTED","Unsupported EQDrive method: "+m);
}

bool cancel(void*,const char*deviceId,const char*){
    const auto d=getDevice(QString::fromUtf8(deviceId?deviceId:""));
    if(!d||!d->connected||!d->session)return false;
    if(d->protocol==WireProtocol::MotorController)return mcStopAxis(*d,1,nullptr)&&mcStopAxis(*d,2,nullptr);
    const auto p=positionRaw(*d);if(!p)return false;
    const QByteArray cmd=QString("Goto %1 %2").arg(f(p->first),f(p->second)).toLatin1();
    return commandOk(*d,cmd,"Goto");
}
void release(void*,const char*value){if(value)gHost.deallocate(gHost.hostContext,const_cast<char*>(value));}
OalDriverV2 api{OAL_DRIVER_ABI_V2,sizeof(OalDriverV2),
                OAL_DRIVER_FEATURE_EVENTS|OAL_DRIVER_FEATURE_CANCELLATION|OAL_DRIVER_FEATURE_HEALTH,
                "oal.eqdrive","OpenAstroLink native EQDrive","0.2.10.25",nullptr,
                &manifest,&start,&stop,&devices,&capabilities,&health,&invoke,&cancel,&release};
}

extern "C" OAL_DRIVER_EXPORT const OalDriverV2 *oalCreateDriverV2(const OalDriverHostV2 *host){
    if(!host||host->abiVersion!=OAL_DRIVER_ABI_V2||!host->allocate||!host->deallocate)return nullptr;
    gHost=*host;return &api;
}
