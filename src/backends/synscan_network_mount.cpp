#include "backends/synscan_network_mount.h"
#include "../../drivers/skywatcher/motor_controller_protocol.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QHostInfo>
#include <QNetworkDatagram>
#include <QThread>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace oas {
namespace {
constexpr double kSiderealDegPerHour=360.0/(86164.0905/3600.0);
QByteArray ba(const std::string&s){return QByteArray(s.data(),int(s.size()));}
QString replyText(const QByteArray&r){return QString::fromLatin1(r).trimmed();}
double wrap360(double x){x=std::fmod(x,360.0);return x<0?x+360.0:x;}
double wrap180(double x){x=wrap360(x);return x>180.0?x-360.0:x;}
}

SynScanNetworkMount::SynScanNetworkMount(QString e):endpoint_(std::move(e)){if(endpoint_.trimmed().isEmpty())endpoint_="auto";}

bool SynScanNetworkMount::resolveEndpoint(QHostAddress&host,quint16&port,QString*error){
    const QString text=endpoint_.trimmed();
    if(text.isEmpty()||text.compare("auto",Qt::CaseInsensitive)==0){
        // The mount/Wi-Fi adapter itself listens on UDP/11880. Broadcast a
        // harmless firmware-version inquiry and accept only a syntactically
        // valid Motor Controller response.
        QUdpSocket probe;if(!probe.bind(QHostAddress::AnyIPv4,0,QUdpSocket::ShareAddress)){if(error)*error="Could not bind direct SynScan Wi-Fi discovery socket: "+probe.errorString();return false;}
        const QByteArray q=ba(oal::skywatcher_mc::getVersion(1));probe.writeDatagram(q,QHostAddress::Broadcast,11880);
        // A few EQDrive/SynScan adapters filter broadcast replies; send the
        // same read-only query to the two common AP gateway addresses too.
        probe.writeDatagram(q,QHostAddress("192.168.4.1"),11880);probe.writeDatagram(q,QHostAddress("192.168.0.1"),11880);
        QElapsedTimer timer;timer.start();while(timer.elapsed()<2200){if(!probe.waitForReadyRead(180))continue;while(probe.hasPendingDatagrams()){const auto dg=probe.receiveDatagram();const std::string r=dg.data().toStdString();if(oal::skywatcher_mc::isNormalResponse(r)&&oal::skywatcher_mc::responsePayload(r).size()>=4){host=dg.senderAddress();port=11880;return true;}}}
        if(error)*error="No direct SynScan/EQDrive Wi-Fi Motor Controller found on UDP 11880. Connect this computer to the mount/EQDrive Wi-Fi network or enter the adapter IP explicitly (for example 192.168.4.1).";return false;
    }
    QUrl u=text.contains("://")?QUrl(text):QUrl("udp://"+text);if(!u.isValid()||u.host().isEmpty()){if(error)*error="Direct SynScan Wi-Fi endpoint must be auto or the mount/EQDrive Wi-Fi adapter IP[:11880]";return false;}port=quint16(u.port(11880));host=QHostAddress(u.host());if(host.isNull()){const auto info=QHostInfo::fromName(u.host());for(const auto&a:info.addresses())if(a.protocol()==QAbstractSocket::IPv4Protocol){host=a;break;}}if(host.isNull()){if(error)*error="Could not resolve mount/EQDrive Wi-Fi adapter host: "+u.host();return false;}return true;
}

bool SynScanNetworkMount::exchange(const QByteArray&command,QByteArray&reply,int timeoutMs,QString*error){
    if(state_==ConnectionState::Disconnected){if(error)*error="Direct SynScan Wi-Fi mount is not connected";return false;}
    while(socket_.hasPendingDatagrams())socket_.receiveDatagram();
    if(socket_.writeDatagram(command,host_,port_)!=command.size()){if(error)*error="Direct SynScan Wi-Fi UDP send failed: "+socket_.errorString();return false;}
    QElapsedTimer timer;timer.start();reply.clear();while(timer.elapsed()<timeoutMs){if(!socket_.waitForReadyRead(std::min(120,std::max(1,timeoutMs-int(timer.elapsed())))))continue;while(socket_.hasPendingDatagrams()){const auto dg=socket_.receiveDatagram();if(dg.senderAddress()!=host_)continue;reply=dg.data();if(reply.contains('\r')||reply.startsWith('=')||reply.startsWith('!'))break;}if(!reply.isEmpty())break;}
    if(reply.isEmpty()){if(error)*error=QString("Direct SynScan Wi-Fi UDP timeout to %1:%2 for %3").arg(host_.toString()).arg(port_).arg(QString::fromLatin1(command.toHex(' ')));return false;}
    const std::string r=reply.toStdString();if(oal::skywatcher_mc::isErrorResponse(r)){if(error)*error=QString("Motor Controller rejected command %1: %2").arg(QString::fromLatin1(command).trimmed(),replyText(reply));return false;}if(!oal::skywatcher_mc::isNormalResponse(r)){if(error)*error="Invalid Motor Controller reply: "+replyText(reply);return false;}return true;
}

bool SynScanNetworkMount::axisQuery(char opcode,int axis,QByteArray&payload,QString*error){QByteArray r;if(!exchange(ba(oal::skywatcher_mc::command(opcode,axis)),r,1800,error))return false;payload=QByteArray::fromStdString(oal::skywatcher_mc::responsePayload(r.toStdString()));return true;}
bool SynScanNetworkMount::axisCommand(char opcode,int axis,const QByteArray&payload,QString*error){QByteArray r;return exchange(ba(oal::skywatcher_mc::command(opcode,axis,payload.toStdString())),r,1800,error);}

bool SynScanNetworkMount::connectDevice(QString*error){
    state_=ConnectionState::Connecting;if(!resolveEndpoint(host_,port_,error)){state_=ConnectionState::Error;return false;}if(!socket_.bind(QHostAddress::AnyIPv4,0,QUdpSocket::ShareAddress)){if(error)*error="Could not bind direct SynScan Wi-Fi UDP client: "+socket_.errorString();state_=ConnectionState::Error;return false;}state_=ConnectionState::Connected;
    QByteArray v1,v2,a1,a2,b;if(!axisQuery('e',1,v1,error)||!axisQuery('e',2,v2,error)||!axisQuery('a',1,a1,error)||!axisQuery('a',2,a2,error)){disconnectDevice();state_=ConnectionState::Error;return false;}
    auto c1=oal::skywatcher_mc::decodeU24(a1.toStdString()),c2=oal::skywatcher_mc::decodeU24(a2.toStdString());if(!c1||!c2||*c1==0||*c2==0){if(error)*error="Direct Wi-Fi mount returned invalid counts-per-revolution";disconnectDevice();state_=ConnectionState::Error;return false;}countsPerRev1_=*c1;countsPerRev2_=*c2;if(axisQuery('b',1,b,nullptr)){auto t=oal::skywatcher_mc::decodeU24(b.toStdString());if(t)timerFreq_=*t;}firmware1_=QString::fromLatin1(v1);firmware2_=QString::fromLatin1(v2);
    // Initialization changes controller state, so do it only after an explicit
    // user connect, never during discovery.
    for(int axis=1;axis<=2;++axis){QByteArray fs;if(axisQuery('f',axis,fs,nullptr)){auto st=oal::skywatcher_mc::parseStatus((QByteArray("=")+fs+"\r").toStdString());if(st.valid&&!st.initialized)axisCommand('F',axis,{},nullptr);}}
    parked_=false;
    return true;
}
void SynScanNetworkMount::disconnectDevice(){socket_.close();state_=ConnectionState::Disconnected;parked_=false;trackingRequested_=false;}

bool SynScanNetworkMount::readAxis(int axis,qint32&position,bool&running,bool&gotoMode,bool&initialized,QString*error){QByteArray p,s;if(!axisQuery('j',axis,p,error)||!axisQuery('f',axis,s,error))return false;auto pos=oal::skywatcher_mc::decodePosition((QByteArray("=")+p+"\r").toStdString());auto st=oal::skywatcher_mc::parseStatus((QByteArray("=")+s+"\r").toStdString());if(!pos||!st.valid){if(error)*error="Could not parse direct Motor Controller axis state";return false;}position=*pos;running=st.running;gotoMode=st.gotoMode;initialized=st.initialized;return true;}
double SynScanNetworkMount::axisDeltaDeg(int axis,qint32 from,qint32 to)const{const double cpr=axis==1?countsPerRev1_:countsPerRev2_;if(cpr<=0)return 0;return double(qint64(to)-qint64(from))*360.0/cpr;}
std::pair<double,double> SynScanNetworkMount::skyFromEncoder(qint32 p1,qint32 p2,qint64 nowMs)const{if(!coordinateSynced_)return{0,0};const double elapsedH=double(nowMs-syncUtcMs_)/3600000.0;const double ra=wrap360(syncRaDeg_+elapsedH*kSiderealDegPerHour+raSkyPerAxis_*axisDeltaDeg(1,syncPos1_,p1));const double dec=std::clamp(syncDecDeg_+decSkyPerAxis_*axisDeltaDeg(2,syncPos2_,p2),-90.0,90.0);return{ra,dec};}

bool SynScanNetworkMount::status(MountStatus&s,QString*error){qint32 p1=0,p2=0;bool r1=false,r2=false,g1=false,g2=false,i1=false,i2=false;if(!readAxis(1,p1,r1,g1,i1,error)||!readAxis(2,p2,r2,g2,i2,error))return false;const auto sky=skyFromEncoder(p1,p2,QDateTime::currentMSecsSinceEpoch());s.connection=state_;s.coordinate={sky.first,sky.second};s.coordinateValid=coordinateSynced_;s.slewing=(r1&&g1)||(r2&&g2);s.tracking=trackingRequested_||(r1&&!g1);s.parked=parked_;s.pierSide="unknown";return true;}

bool SynScanNetworkMount::stopAxis(int axis,QString*error){return axisCommand('K',axis,{},error);}
bool SynScanNetworkMount::waitStopped(int axis,int timeoutMs,QString*error){QElapsedTimer t;t.start();while(t.elapsed()<timeoutMs){qint32 p=0;bool running=false,g=false,i=false;if(!readAxis(axis,p,running,g,i,error))return false;if(!running)return true;QThread::msleep(60);}if(error)*error=QString("Axis %1 did not stop in time").arg(axis);return false;}

bool SynScanNetworkMount::gotoAxisDelta(int axis,double deltaDeg,QString*error){
    if(std::abs(deltaDeg)<1e-5)return true;
    const double cpr=axis==1?countsPerRev1_:countsPerRev2_;
    if(cpr<=0){if(error)*error="Invalid axis scale";return false;}
    qint32 before=0;bool wasRunning=false,wasGoto=false,initialized=false;
    if(!readAxis(axis,before,wasRunning,wasGoto,initialized,error))return false;
    const quint32 counts=quint32(std::max(1.0,std::round(std::abs(deltaDeg)*cpr/360.0)));
    const bool clockwise=deltaDeg>0;
    if(!stopAxis(axis,nullptr)||!waitStopped(axis,2500,error))return false;
    QByteArray r;
    const auto send=[&](const std::string&cmd){return exchange(ba(cmd),r,1800,error);};
    // Follow the proven EQMOD/INDI SlewTo sequence: mode -> target increment ->
    // deceleration breakpoint -> start.  SetStepPeriod belongs to continuous
    // slew/tracking; forcing it during SlewTo can make compatible controllers
    // acknowledge every command while never starting the GOTO.
    if(!send(oal::skywatcher_mc::setMotionMode(axis,true,false,clockwise)))return false;
    if(!send(oal::skywatcher_mc::setGotoIncrement(axis,counts)))return false;
    const quint32 brake=std::min<quint32>(counts,200u);
    if(!send(oal::skywatcher_mc::setBrakeIncrement(axis,brake)))return false;
    if(!send(oal::skywatcher_mc::startMotion(axis)))return false;

    // Do not report a GOTO as accepted merely because all command replies were
    // syntactically valid. Some EQDrive/SynScan-compatible controllers can
    // acknowledge a sequence while the axis remains stopped. Require either a
    // running GOTO state or a real encoder change shortly after :J.
    QElapsedTimer verify;verify.start();
    while(verify.elapsed()<900){
        QThread::msleep(60);
        qint32 now=before;bool running=false,gotoMode=false,init=false;
        QString probeError;
        if(!readAxis(axis,now,running,gotoMode,init,&probeError))continue;
        if((running&&gotoMode)||std::abs(qint64(now)-qint64(before))>=2)return true;
    }
    if(error)*error=QString("Axis %1 accepted the direct Wi-Fi GOTO command sequence but did not start moving; check controller mode/park state and native motion encoding").arg(axis);
    return false;
}
bool SynScanNetworkMount::slewTo(const EquatorialCoord&t,QString*error){if(parked_){if(error)*error="Direct Wi-Fi mount is parked in OpenAstroLink; unpark it first";return false;}if(!coordinateSynced_){if(error)*error="Direct SynScan/EQDrive Wi-Fi GOTO requires one Sync on a known sky position first";return false;}MountStatus st;if(!status(st,error))return false;const double dRa=wrap180(t.raDeg-st.coordinate.raDeg),dDec=t.decDeg-st.coordinate.decDeg;if(std::max(std::abs(dRa),std::abs(dDec))>safetyGotoLimitDeg_){if(error)*error=QString("Direct Wi-Fi native GOTO is HIL-limited to %1° per command; requested dRA=%2° dDEC=%3°. Use Sync + a smaller test slew or Classic ASCOM until the direct axis direction is qualified.").arg(safetyGotoLimitDeg_).arg(dRa,0,'f',3).arg(dDec,0,'f',3);return false;}trackingRequested_=false;const double a1=dRa/raSkyPerAxis_,a2=dDec/decSkyPerAxis_;if(!gotoAxisDelta(1,a1,error)){abortMotion(nullptr);return false;}if(!gotoAxisDelta(2,a2,error)){abortMotion(nullptr);return false;}return true;}
bool SynScanNetworkMount::abortMotion(QString*error){QString e1,e2;const bool a=stopAxis(1,&e1),b=stopAxis(2,&e2);trackingRequested_=false;if(!a||!b){if(error)*error=!e1.isEmpty()?e1:e2;return false;}return true;}
bool SynScanNetworkMount::syncTo(const EquatorialCoord&t,QString*error){qint32 p1=0,p2=0;bool r=false,g=false,i=false;if(!readAxis(1,p1,r,g,i,error)||!readAxis(2,p2,r,g,i,error))return false;coordinateSynced_=true;syncPos1_=p1;syncPos2_=p2;syncRaDeg_=wrap360(t.raDeg);syncDecDeg_=std::clamp(t.decDeg,-90.0,90.0);syncUtcMs_=QDateTime::currentMSecsSinceEpoch();return true;}
bool SynScanNetworkMount::setTracking(bool enabled,QString*error){if(!enabled){trackingRequested_=false;return stopAxis(1,error);}if(countsPerRev1_==0||timerFreq_==0){if(error)*error="Direct Wi-Fi mount did not report the timer/axis scale required for tracking";return false;}if(!stopAxis(1,nullptr)||!waitStopped(1,2500,error))return false;const double cpr=countsPerRev1_;const quint32 period=quint32(std::clamp(std::round(double(timerFreq_)*360.0/(cpr*kSiderealDegPerHour/3600.0)),6.0,double(0xFFFFFF)));QByteArray r;auto send=[&](const std::string&cmd){return exchange(ba(cmd),r,1800,error);}; // low-speed continuous slew mode: payload 10/11
    const std::string mode=oal::skywatcher_mc::command('G',1,std::string("1")+(raSkyPerAxis_<0?"1":"0"));if(!send(mode)||!send(oal::skywatcher_mc::setStepPeriod(1,period))||!send(oal::skywatcher_mc::startMotion(1)))return false;trackingRequested_=true;return true;}
bool SynScanNetworkMount::park(bool enabled,QString*error){
    if(enabled){
        if(!abortMotion(error))return false;
        parked_=true;
        return true;
    }
    // The low-level Motor Controller protocol has no celestial "park" state;
    // unpark is therefore a safe local software gate. Physical home/park
    // coordinates remain the responsibility of the future qualified pier model.
    parked_=false;
    return true;
}
bool SynScanNetworkMount::pulseGuide(GuideDirection,int,QString*error){if(error)*error="Direct SynScan/EQDrive Wi-Fi pulse guide is not enabled until native tracking direction is HIL-qualified";return false;}
}
