#include "oal/driver_api.h"
#include "astep_protocol.h"
#include "../common_blocking_serial_session.h"

#include <QByteArray>
#include <QDateTime>
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
constexpr double kSiderealDegPerHour = 360.0 / (86164.0905 / 3600.0);

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
int gProbeTimeoutMs{900};
int gCommandTimeoutMs{2500};
int gOpenSettleMs{350};
double gRaSkyPerAxis{-1.0};
double gDecSkyPerAxis{1.0};
double gGuideRateDegPerHour{kSiderealDegPerHour*0.5};

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

bool identify(Device&d){
    // Probe the read-only motor status first. It is both cheap and specific to
    // the public ASTEP mount section; this keeps multi-baud focused discovery
    // reasonably fast even when the configured controller rate is unknown.
    QByteArray statusReply;if(!exchange(d,"St",&statusReply,gProbeTimeoutMs)||!oal::eqdrive::parseStatus(statusReply))return false;
    QByteArray fw;if(exchange(d,"FWs",&fw,gProbeTimeoutMs)&&oal::eqdrive::looksLikeEqDrive(fw))d.firmware=oal::eqdrive::firmwareText(fw);
    if(d.firmware.isEmpty()){fw.clear();if(exchange(d,"FW",&fw,gProbeTimeoutMs)&&oal::eqdrive::looksLikeEqDrive(fw))d.firmware=oal::eqdrive::firmwareText(fw);}
    if(d.firmware.isEmpty()){fw.clear();if(exchange(d,"FWx",&fw,gProbeTimeoutMs)&&oal::eqdrive::trimLine(fw).toLower().contains("eqdrive"))d.firmware=oal::eqdrive::firmwareText(fw);}
    if(d.firmware.isEmpty())d.firmware=QStringLiteral("EQDrive ASTEP (firmware string unavailable)");
    QByteArray cg;if(exchange(d,"Cg",&cg,gCommandTimeoutMs)){auto dirs=oal::eqdrive::parseAxisDirections(cg);d.configuredAxisDirection1=dirs.first;d.configuredAxisDirection2=dirs.second;}
    return true;
}

bool probePort(const QString&port,Device&out,bool keepOpen){
    const bool focused=!qEnvironmentVariable("OAL_EQDRIVE_PORT").trimmed().isEmpty()||gConfiguredPorts.contains(port);
    struct Lines{bool dtr;bool rts;};
    const QList<Lines> lineModes=focused?QList<Lines>{{false,false},{true,false},{true,true}}:QList<Lines>{{false,false}};
    for(int baud:gBaudRates){for(const auto lines:lineModes){
        Device d;d.port=port.trimmed();d.id=makeId(d.port);d.baudRate=baud;d.dtr=lines.dtr;d.rts=lines.rts;d.raSkyPerAxis=gRaSkyPerAxis;d.decSkyPerAxis=gDecSkyPerAxis;fillMetadata(d);d.session=std::make_shared<OalBlockingSerialSession>();QString error;
        logMessage(1,QString("probing %1 for EQDrive ASTEP at %2 baud (DTR=%3 RTS=%4)").arg(d.port).arg(baud).arg(lines.dtr?"on":"off").arg(lines.rts?"on":"off"));
        if(!d.session->open(d.port,baud,gOpenSettleMs,&error,lines.dtr,lines.rts)){logMessage(2,QString("%1: open failed: %2").arg(d.port,error));continue;}
        if(identify(d)){
            logMessage(1,QString("%1: EQDrive discovered (%2, %3 baud)").arg(d.port,d.firmware).arg(baud));
            if(!keepOpen){d.session->close();d.session.reset();}out=std::move(d);return true;
        }
        logMessage(2,QString("%1 @ %2: ASTEP probe failed: %3").arg(d.port).arg(baud).arg(d.session->lastExchangeDiagnostic()));d.session->close();
    }}
    return false;
}

std::optional<Device> getDevice(const QString&id){QMutexLocker l(&gMutex);auto it=gDevices.find(id);if(it==gDevices.end())return std::nullopt;return it.value();}
void updateDevice(const Device&d){QMutexLocker l(&gMutex);gDevices[d.id]=d;}
std::optional<Device> connectedForPort(const QString&p){QMutexLocker l(&gMutex);for(const auto&d:gDevices)if(d.connected&&d.port==p&&d.session&&d.session->isOpen())return d;return std::nullopt;}
void refreshDevices(){QHash<QString,Device> found;for(const auto&p:candidatePorts()){if(auto e=connectedForPort(p)){found.insert(e->id,*e);continue;}Device d;if(probePort(p,d,false)){QMutexLocker l(&gMutex);if(gDevices.contains(d.id)){const auto old=gDevices.value(d.id);d.connected=old.connected;d.coordinateSynced=old.coordinateSynced;d.syncAxis1Deg=old.syncAxis1Deg;d.syncAxis2Deg=old.syncAxis2Deg;d.syncRaDeg=old.syncRaDeg;d.syncDecDeg=old.syncDecDeg;d.syncUtcMs=old.syncUtcMs;}found.insert(d.id,d);}}QMutexLocker l(&gMutex);for(auto it=gDevices.begin();it!=gDevices.end();++it)if(it.value().connected&&!found.contains(it.key()))found.insert(it.key(),it.value());gDevices=found;}

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
    gRaSkyPerAxis=o.value("raSkyPerAxis").toDouble(gRaSkyPerAxis);gDecSkyPerAxis=o.value("decSkyPerAxis").toDouble(gDecSkyPerAxis);gGuideRateDegPerHour=o.value("guideRateDegPerHour").toDouble(gGuideRateDegPerHour);
    logMessage(1,QString("config: baudRates=%1 openSettleMs=%2 raSkyPerAxis=%3 decSkyPerAxis=%4").arg([&]{QStringList x;for(int b:gBaudRates)x<<QString::number(b);return x.join(',');}()).arg(gOpenSettleMs).arg(gRaSkyPerAxis).arg(gDecSkyPerAxis));return true;
}
void stop(void*){QMutexLocker l(&gMutex);for(auto it=gDevices.begin();it!=gDevices.end();++it){if(it.value().session)it.value().session->close();it.value().session.reset();it.value().connected=false;}}

const char*manifest(void*){return json(QJsonObject{{"driverId","oal.eqdrive"},{"name","OpenAstroLink native EQDrive driver"},{"version","0.2.10.16"},{"abiVersion",2},{"threadModel","per-device-serial"},{"protocol","EQDrive ASTEP"}});}
const char*devices(void*){refreshDevices();QJsonArray a;QMutexLocker l(&gMutex);for(const auto&d:gDevices)a.append(QJsonObject{{"id",d.id},{"type","mount"},{"name",d.description},{"serialNumber",d.serial},{"firmware",d.firmware},{"transport",QJsonObject{{"kind","serial"},{"protocol","eqdrive-astep"},{"port",d.port},{"baudRate",d.baudRate},{"dtr",d.dtr},{"rts",d.rts},{"persistentSession",d.connected}}}});return json(a);}
const char*capabilities(void*,const char*deviceId){const auto d=getDevice(QString::fromUtf8(deviceId?deviceId:""));if(!d)return json(QJsonObject{});return json(QJsonObject{{"schemaVersion","1.0"},{"mount",QJsonObject{{"position",QJsonObject{{"supported",true},{"frame","sync-anchor-relative"},{"requiresSync",true}}},{"slew",QJsonObject{{"supported",true},{"abortSupported",true},{"requiresSync",true},{"implementation","ASTEP relative-axis slew"}}},{"sync",QJsonObject{{"supported",true},{"implementation","non-destructive sky/axis anchor"}}},{"tracking",QJsonObject{{"supported",true},{"modes",QJsonArray{"off","equatorial"}}}},{"park",QJsonObject{{"supported",false}}},{"pulseGuide",QJsonObject{{"supported",true},{"implementation","temporary ASTEP Speed offset"},{"maxDurationMs",5000}}},{"pierSide",QJsonObject{{"supported",false}}},{"telemetry",QJsonObject{{"axisDegrees",true},{"axisSpeedDegPerHour",true},{"driverState",true},{"firmware",d->firmware}}},{"coordinateModel",QJsonObject{{"type","sync-anchor-v1"},{"meridianFlip",false},{"note","Sync once on a known sky position before native RA/DEC GOTO; no automatic pier flip in v0.2.10.16"}}}}}});}
const char*health(void*,const char*deviceId){const auto d=getDevice(QString::fromUtf8(deviceId?deviceId:""));if(!d)return json(QJsonObject{{"state","missing"},{"connected",false}});return json(QJsonObject{{"state",d->connected?"ok":"disconnected"},{"connected",d->connected},{"port",d->port},{"baudRate",d->baudRate},{"firmware",d->firmware},{"coordinateSynced",d->coordinateSynced}});}

const char*invoke(void*,const char*deviceId,const char*method,const char*requestJson,const OalDriverCallV2*){
    const QString id=QString::fromUtf8(deviceId?deviceId:"");const QString m=QString::fromUtf8(method?method:"");auto od=getDevice(id);if(!od)return fail("DEVICE_NOT_FOUND","EQDrive is no longer present");Device d=*od;const auto req=QJsonDocument::fromJson(QByteArray(requestJson?requestJson:"{}")).object();
    if(m=="device.connect"){
        Device c;if(!probePort(d.port,c,true))return fail("CONNECT_FAILED","EQDrive ASTEP handshake failed on "+d.port);c.connected=true;c.coordinateSynced=d.coordinateSynced;c.syncAxis1Deg=d.syncAxis1Deg;c.syncAxis2Deg=d.syncAxis2Deg;c.syncRaDeg=d.syncRaDeg;c.syncDecDeg=d.syncDecDeg;c.syncUtcMs=d.syncUtcMs;updateDevice(c);emitEvent(c,"device.connected");return ok(QJsonObject{{"port",c.port},{"baudRate",c.baudRate},{"firmware",c.firmware},{"coordinateSynced",c.coordinateSynced}});
    }
    if(m=="device.disconnect"){if(d.session)d.session->close();d.session.reset();d.connected=false;updateDevice(d);emitEvent(d,"device.disconnected");return ok();}
    if(!d.connected||!d.session||!d.session->isOpen())return fail("DEVICE_DISCONNECTED","EQDrive is not connected");
    if(m=="mount.status"){
        const auto st=statusRaw(d);if(!st)return fail("TRANSPORT_ERROR","Could not read EQDrive St telemetry");const auto sky=skyFromAxes(d,st->axis1Deg,st->axis2Deg,QDateTime::currentMSecsSinceEpoch());
        return ok(QJsonObject{{"raDeg",sky.first},{"decDeg",sky.second},{"coordinateFrame","sync-anchor-relative"},{"coordinateValid",d.coordinateSynced},{"tracking",std::abs(st->speed1DegPerHour)>0.05&&!st->gotoActive1},{"slewing",st->gotoActive1||st->gotoActive2},{"parked",false},{"parkSupported",false},{"pierSide","unknown"},{"axis1Deg",st->axis1Deg},{"axis2Deg",st->axis2Deg},{"speed1DegPerHour",st->speed1DegPerHour},{"speed2DegPerHour",st->speed2DegPerHour},{"driverEnabled1",st->driverEnabled1},{"driverEnabled2",st->driverEnabled2},{"coordinateSynced",d.coordinateSynced}});
    }
    if(m=="mount.sync"){
        const double ra=req.value("raDeg").toDouble(-1),dec=req.value("decDeg").toDouble(999);if(ra<0||ra>=360||dec<-90||dec>90)return fail("INVALID_COORDINATE","RA/DEC outside valid range");const auto p=positionRaw(d);if(!p)return fail("TRANSPORT_ERROR","Could not read EQDrive axis position for sync");d.coordinateSynced=true;d.syncAxis1Deg=p->first;d.syncAxis2Deg=p->second;d.syncRaDeg=ra;d.syncDecDeg=dec;d.syncUtcMs=QDateTime::currentMSecsSinceEpoch();updateDevice(d);emitEvent(d,"mount.synced",{{"raDeg",ra},{"decDeg",dec},{"axis1Deg",p->first},{"axis2Deg",p->second}});return ok(QJsonObject{{"coordinateSynced",true},{"axis1Deg",p->first},{"axis2Deg",p->second}});
    }
    if(m=="mount.slew"){
        if(!d.coordinateSynced)return fail("MOUNT_NOT_SYNCED","Native EQDrive RA/DEC GOTO requires one Sync on a known sky position first");const double ra=req.value("raDeg").toDouble(-1),dec=req.value("decDeg").toDouble(999);if(ra<0||ra>=360||dec<-90||dec>90)return fail("INVALID_COORDINATE","RA/DEC outside valid range");const auto st=statusRaw(d);if(!st)return fail("TRANSPORT_ERROR","Could not read EQDrive status before slew");const auto sky=skyFromAxes(d,st->axis1Deg,st->axis2Deg,QDateTime::currentMSecsSinceEpoch());const double dRa=oal::eqdrive::wrap180(ra-sky.first),dDec=dec-sky.second;const double axis1Delta=dRa/d.raSkyPerAxis,axis2Delta=dDec/d.decSkyPerAxis;
        const QByteArray cmd=QString("Slew %1 %2").arg(f(axis1Delta),f(axis2Delta)).toLatin1();if(!commandOk(d,cmd,"Slew"))return fail("TRANSPORT_ERROR","EQDrive rejected ASTEP Slew command");emitEvent(d,"mount.slewStarted",{{"raDeg",ra},{"decDeg",dec},{"axis1DeltaDeg",axis1Delta},{"axis2DeltaDeg",axis2Delta}});return ok(QJsonObject{{"accepted",true},{"raDeg",ra},{"decDeg",dec},{"axis1DeltaDeg",axis1Delta},{"axis2DeltaDeg",axis2Delta}});
    }
    if(m=="mount.abort"){
        const auto p=positionRaw(d);if(!p)return fail("TRANSPORT_ERROR","Could not read axis position for abort");const QByteArray cmd=QString("Goto %1 %2").arg(f(p->first),f(p->second)).toLatin1();if(!commandOk(d,cmd,"Goto"))return fail("TRANSPORT_ERROR","EQDrive best-effort abort (retarget current axis position) failed");emitEvent(d,"mount.motionAborted");return ok(QJsonObject{{"method","retarget-current-axis-position"}});
    }
    if(m=="mount.setTracking"){
        const bool enabled=req.value("enabled").toBool();const QByteArray cmd=QString("Speed %1 0.000000").arg(f(enabled?kSiderealDegPerHour:0.0)).toLatin1();if(!commandOk(d,cmd,"Speed"))return fail("TRANSPORT_ERROR","EQDrive tracking Speed command failed");emitEvent(d,"mount.tracking",{{"enabled",enabled}});return ok();
    }
    if(m=="mount.pulseGuide"){
        const QString dir=req.value("direction").toString();const int ms=std::clamp(req.value("durationMs").toInt(),1,5000);QByteArray rr;if(!exchange(d,"Speed",&rr,gCommandTimeoutMs))return fail("TRANSPORT_ERROR","Could not read EQDrive tracking speed");const auto sp=oal::eqdrive::parseSpeed(rr);if(!sp)return fail("TRANSPORT_ERROR","Could not parse EQDrive Speed response");double s1=sp->first,s2=sp->second;if(dir=="east")s1+=gGuideRateDegPerHour;else if(dir=="west")s1-=gGuideRateDegPerHour;else if(dir=="north")s2+=gGuideRateDegPerHour;else if(dir=="south")s2-=gGuideRateDegPerHour;else return fail("INVALID_DIRECTION","direction must be north/south/east/west");const QByteArray guide=QString("Speed %1 %2").arg(f(s1),f(s2)).toLatin1();if(!commandOk(d,guide,"Speed"))return fail("TRANSPORT_ERROR","Could not apply EQDrive guide speed");QThread::msleep(unsigned(ms));const QByteArray restore=QString("Speed %1 %2").arg(f(sp->first),f(sp->second)).toLatin1();if(!commandOk(d,restore,"Speed"))return fail("TRANSPORT_ERROR","Could not restore EQDrive tracking speed after pulse guide");return ok();
    }
    if(m=="mount.park")return fail("NOT_SUPPORTED","Native EQDrive park/meridian model is not exposed until the full pier-side model is qualified");
    if(m=="eqdrive.telemetry"){
        const auto st=statusRaw(d);if(!st)return fail("TRANSPORT_ERROR","Could not read EQDrive telemetry");QByteArray vin,vbs,cg;exchange(d,"Vin",&vin,gCommandTimeoutMs);exchange(d,"Vbs",&vbs,gCommandTimeoutMs);exchange(d,"Cg",&cg,gCommandTimeoutMs);return ok(QJsonObject{{"firmware",d.firmware},{"port",d.port},{"baudRate",d.baudRate},{"axis1Deg",st->axis1Deg},{"axis2Deg",st->axis2Deg},{"speed1DegPerHour",st->speed1DegPerHour},{"speed2DegPerHour",st->speed2DegPerHour},{"stateHex",QString::number(st->state,16).rightJustified(4,'0')},{"vin",QString::fromLatin1(oal::eqdrive::trimLine(vin))},{"vbs",QString::fromLatin1(oal::eqdrive::trimLine(vbs))},{"config",QString::fromLatin1(oal::eqdrive::trimLine(cg))}});
    }
    return fail("METHOD_NOT_SUPPORTED","Unsupported EQDrive method: "+m);
}

bool cancel(void*,const char*deviceId,const char*){
    const auto d=getDevice(QString::fromUtf8(deviceId?deviceId:""));
    if(!d||!d->connected||!d->session)return false;
    const auto p=positionRaw(*d);if(!p)return false;
    const QByteArray cmd=QString("Goto %1 %2").arg(f(p->first),f(p->second)).toLatin1();
    return commandOk(*d,cmd,"Goto");
}
void release(void*,const char*value){if(value)gHost.deallocate(gHost.hostContext,const_cast<char*>(value));}
OalDriverV2 api{OAL_DRIVER_ABI_V2,sizeof(OalDriverV2),
                OAL_DRIVER_FEATURE_EVENTS|OAL_DRIVER_FEATURE_CANCELLATION|OAL_DRIVER_FEATURE_HEALTH,
                "oal.eqdrive","OpenAstroLink native EQDrive","0.2.10.16",nullptr,
                &manifest,&start,&stop,&devices,&capabilities,&health,&invoke,&cancel,&release};
}

extern "C" OAL_DRIVER_EXPORT const OalDriverV2 *oalCreateDriverV2(const OalDriverHostV2 *host){
    if(!host||host->abiVersion!=OAL_DRIVER_ABI_V2||!host->allocate||!host->deallocate)return nullptr;
    gHost=*host;return &api;
}
