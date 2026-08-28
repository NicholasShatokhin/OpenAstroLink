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
bool AscomClassicMount::connectDevice(QString *error){state_=ConnectionState::Connecting;if(progId_.trimmed().isEmpty()){if(error)*error="Choose or enter an ASCOM Telescope ProgID (for example EQMOD.Telescope)";state_=ConnectionState::Error;return false;}if(!ensureHost(error)){state_=ConnectionState::Error;return false;}QJsonObject d;if(!request({{"cmd","connect"},{"progId",progId_}},&d,error,15000)){state_=ConnectionState::Error;return false;}name_=d.value("name").toString(progId_);state_=ConnectionState::Connected;return true;}
void AscomClassicMount::disconnectDevice(){if(host_.state()!=QProcess::NotRunning){QJsonObject d;request({{"cmd","disconnect"}},&d,nullptr,5000);host_.write("{\"cmd\":\"quit\"}\n");host_.waitForFinished(2000);if(host_.state()!=QProcess::NotRunning)host_.kill();}state_=ConnectionState::Disconnected;}
bool AscomClassicMount::status(MountStatus&s,QString*e){QJsonObject d;if(!request({{"cmd","status"}},&d,e))return false;s.connection=state_;s.coordinate={d.value("raHours").toDouble()*15.0,d.value("decDeg").toDouble()};s.tracking=d.value("tracking").toBool();s.slewing=d.value("slewing").toBool();s.parked=d.value("parked").toBool();const int side=d.value("sideOfPier").toInt(-1);s.pierSide=side==0?"east":side==1?"west":"unknown";return true;}
bool AscomClassicMount::slewTo(const EquatorialCoord&t,QString*e){return request({{"cmd","slew"},{"raHours",t.raDeg/15.0},{"decDeg",t.decDeg}},nullptr,e,15000);}
bool AscomClassicMount::abortMotion(QString*e){return request({{"cmd","abort"}},nullptr,e);}
bool AscomClassicMount::syncTo(const EquatorialCoord&t,QString*e){return request({{"cmd","sync"},{"raHours",t.raDeg/15.0},{"decDeg",t.decDeg}},nullptr,e);}
bool AscomClassicMount::setTracking(bool v,QString*e){return request({{"cmd","tracking"},{"enabled",v}},nullptr,e);}
bool AscomClassicMount::park(bool v,QString*e){return request({{"cmd","park"},{"parked",v}},nullptr,e,30000);}
bool AscomClassicMount::pulseGuide(GuideDirection dir,int ms,QString*e){int d=0;switch(dir){case GuideDirection::North:d=0;break;case GuideDirection::South:d=1;break;case GuideDirection::East:d=2;break;case GuideDirection::West:d=3;break;}return request({{"cmd","pulseGuide"},{"direction",d},{"durationMs",ms}},nullptr,e,std::max(5000,ms+3000));}
QString AscomClassicMount::destinationPierSide(const EquatorialCoord&t,QString*e){QJsonObject d;if(!request({{"cmd","destinationPierSide"},{"raHours",t.raDeg/15.0},{"decDeg",t.decDeg}},&d,e,5000))return{};const int side=d.value("sideOfPier").toInt(-1);return side==0?"east":side==1?"west":"unknown";}
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
