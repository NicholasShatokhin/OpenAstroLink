#include "backends/synscan_network_mount.h"
#include "core/equatorial_frames.h"
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

SynScanNetworkMount::SynScanNetworkMount(QString e,MountGeometryConfig geometry,ObserverLocation observer)
    :endpoint_(std::move(e)),geometry_(std::move(geometry),observer){
    if(endpoint_.trimmed().isEmpty())endpoint_="auto";
}

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
    if(state_!=ConnectionState::Connected||socket_.state()!=QAbstractSocket::BoundState){if(error)*error="Direct SynScan Wi-Fi mount is not connected";return false;}
    while(socket_.state()==QAbstractSocket::BoundState&&socket_.hasPendingDatagrams())socket_.receiveDatagram();
    if(socket_.writeDatagram(command,host_,port_)!=command.size()){if(error)*error="Direct SynScan Wi-Fi UDP send failed: "+socket_.errorString();return false;}
    QElapsedTimer timer;timer.start();reply.clear();while(timer.elapsed()<timeoutMs){if(!socket_.waitForReadyRead(std::min(120,std::max(1,timeoutMs-int(timer.elapsed())))))continue;while(socket_.state()==QAbstractSocket::BoundState&&socket_.hasPendingDatagrams()){const auto dg=socket_.receiveDatagram();if(dg.senderAddress()!=host_)continue;reply=dg.data();if(reply.contains('\r')||reply.startsWith('=')||reply.startsWith('!'))break;}if(!reply.isEmpty())break;}
    if(reply.isEmpty()){if(error)*error=QString("Direct SynScan Wi-Fi UDP timeout to %1:%2 for %3").arg(host_.toString()).arg(port_).arg(QString::fromLatin1(command.toHex(' ')));return false;}
    const std::string r=reply.toStdString();if(oal::skywatcher_mc::isErrorResponse(r)){if(error)*error=QString("Motor Controller rejected command %1: %2").arg(QString::fromLatin1(command).trimmed(),replyText(reply));return false;}if(!oal::skywatcher_mc::isNormalResponse(r)){if(error)*error="Invalid Motor Controller reply: "+replyText(reply);return false;}return true;
}

bool SynScanNetworkMount::axisQuery(char opcode,int axis,QByteArray&payload,QString*error){QByteArray r;if(!exchange(ba(oal::skywatcher_mc::command(opcode,axis)),r,1800,error))return false;payload=QByteArray::fromStdString(oal::skywatcher_mc::responsePayload(r.toStdString()));return true;}
bool SynScanNetworkMount::axisCommand(char opcode,int axis,const QByteArray&payload,QString*error){QByteArray r;return exchange(ba(oal::skywatcher_mc::command(opcode,axis,payload.toStdString())),r,1800,error);}

bool SynScanNetworkMount::connectDevice(QString*error){
    state_=ConnectionState::Connecting;if(!resolveEndpoint(host_,port_,error)){state_=ConnectionState::Error;return false;}if(!socket_.bind(QHostAddress::AnyIPv4,0,QUdpSocket::ShareAddress)){if(error)*error="Could not bind direct SynScan Wi-Fi UDP client: "+socket_.errorString();state_=ConnectionState::Error;return false;}state_=ConnectionState::Connected;
    QByteArray v1,v2,a1,a2,b;if(!axisQuery('e',1,v1,error)||!axisQuery('e',2,v2,error)||!axisQuery('a',1,a1,error)||!axisQuery('a',2,a2,error)){disconnectDevice();state_=ConnectionState::Error;return false;}
    auto c1=oal::skywatcher_mc::decodeU24(a1.toStdString()),c2=oal::skywatcher_mc::decodeU24(a2.toStdString());if(!c1||!c2||*c1==0||*c2==0){if(error)*error="Direct Wi-Fi mount returned invalid counts-per-revolution";disconnectDevice();state_=ConnectionState::Error;return false;}countsPerRev1_=*c1;countsPerRev2_=*c2;if(axisQuery('b',1,b,nullptr)){auto t=oal::skywatcher_mc::decodeU24(b.toStdString());if(t)timerFreq_=*t;}firmware1_=QString::fromLatin1(v1);firmware2_=QString::fromLatin1(v2);
    // UDP/11880 is a transport for the same Sky-Watcher Motor Controller
    // command set used by native EQDrive serial.  Do not reinterpret axis
    // polarity here: the same controller counts must represent the same physical
    // mechanical angles on both transports.
    // Initialization changes controller state, so do it only after an explicit
    // user connect, never during discovery.
    for(int axis=1;axis<=2;++axis){QByteArray fs;if(axisQuery('f',axis,fs,nullptr)){auto st=oal::skywatcher_mc::parseStatus((QByteArray("=")+fs+"\r").toStdString());if(st.valid&&!st.initialized)axisCommand('F',axis,{},nullptr);}}
    // Direct UDP/11880 is the same Motor Controller protocol as native EQDrive.
    // Capture the user's actual startup polar-Home pose instead of guessing a
    // firmware-specific DEC offset. This pose becomes mechanical 0°,0°.
    qint32 h1=0,h2=0;bool rr=false,gg=false,ii=false;
    if(!readAxis(1,h1,rr,gg,ii,error)||!readAxis(2,h2,rr,gg,ii,error)){disconnectDevice();state_=ConnectionState::Error;return false;}
    sessionHomeCounts1_=h1;sessionHomeCounts2_=h2;sessionHomeValid_=true;
    parked_=false;alignmentSource_.clear();homeAlignmentNote_.clear();
    tryAutoHomeSync(nullptr);
    return true;
}
void SynScanNetworkMount::disconnectDevice(){state_=ConnectionState::Disconnected;socket_.close();parked_=false;trackingRequested_=false;sessionHomeValid_=false;}

bool SynScanNetworkMount::readAxis(int axis,qint32&position,bool&running,bool&gotoMode,bool&initialized,QString*error){QByteArray p,s;if(!axisQuery('j',axis,p,error)||!axisQuery('f',axis,s,error))return false;auto pos=oal::skywatcher_mc::decodePosition((QByteArray("=")+p+"\r").toStdString());auto st=oal::skywatcher_mc::parseStatus((QByteArray("=")+s+"\r").toStdString());if(!pos||!st.valid){if(error)*error="Could not parse direct Motor Controller axis state";return false;}position=*pos;running=st.running;gotoMode=st.gotoMode;initialized=st.initialized;return true;}
double SynScanNetworkMount::axisDeltaDeg(int axis,qint32 from,qint32 to)const{const double cpr=axis==1?countsPerRev1_:countsPerRev2_;if(cpr<=0)return 0;return double(qint64(to)-qint64(from))*360.0/cpr;}
MechanicalAxes SynScanNetworkMount::axesFromEncoder(qint32 p1,qint32 p2)const{
    if(countsPerRev1_==0||countsPerRev2_==0||!sessionHomeValid_)return {};
    // One public mechanical coordinate system only: the controller counts
    // captured at direct-MC connect are the user's physical polar Home/Park and
    // therefore map to Axis1=0°, Axis2=0°. No guessed quarter-turn offset.
    const double a1=(double(qint64(p1)-qint64(sessionHomeCounts1_))*360.0)/double(countsPerRev1_);
    const double a2=(double(qint64(p2)-qint64(sessionHomeCounts2_))*360.0)/double(countsPerRev2_);
    return {wrap180(a1),wrap180(a2),true};
}

bool SynScanNetworkMount::tryAutoHomeSync(QString *error){
    if(!geometry_.config().autoHomeSync)return false;
    qint32 p1=0,p2=0;bool r=false,g=false,i=false;QString e;
    if(!readAxis(1,p1,r,g,i,&e)||!readAxis(2,p2,r,g,i,&e)){if(error)*error=e;return false;}
    const auto axes=axesFromEncoder(p1,p2);
    const double h1=geometry_.config().customHome?geometry_.config().homeAxis1Deg:0.0;
    const double h2=geometry_.config().customHome?geometry_.config().homeAxis2Deg:0.0;
    const double tol=std::clamp(geometry_.config().homeToleranceDeg,0.1,15.0);
    const double d1=std::abs(wrap180(axes.axis1Deg-h1)),d2=std::abs(wrap180(axes.axis2Deg-h2));
    if(d1>tol||d2>tol){homeAlignmentNote_=QString("Direct Wi-Fi Home not applied: dAxis1=%1deg dAxis2=%2deg tolerance=%3deg").arg(d1,0,'f',4).arg(d2,0,'f',4).arg(tol,0,'f',2);if(error)*error=homeAlignmentNote_;return false;}
    if(!geometry_.syncHome(axes,QDateTime::currentDateTimeUtc(),&e)){if(error)*error=e;return false;}
    alignmentSource_=geometry_.config().customHome?"synscan-wifi-saved-home":"synscan-wifi-zero-home";
    homeAlignmentNote_=QString("Direct Wi-Fi Home accepted: axes=(%1,%2)").arg(axes.axis1Deg,0,'f',4).arg(axes.axis2Deg,0,'f',4);
    if(error)error->clear();return true;
}

bool SynScanNetworkMount::status(MountStatus&s,QString*error){qint32 p1=0,p2=0;bool r1=false,r2=false,g1=false,g2=false,i1=false,i2=false;if(!readAxis(1,p1,r1,g1,i1,error)||!readAxis(2,p2,r2,g2,i2,error))return false;const auto axes=axesFromEncoder(p1,p2);s.connection=state_;s.axes=axes;s.geometryType=mountGeometryTypeName(geometry_.config().type);s.slewing=(r1&&g1)||(r2&&g2);s.tracking=trackingRequested_||(r1&&!g1);s.parked=parked_;s.diagnostics["alignmentSource"]=alignmentSource_.isEmpty()?(geometry_.synced()?"restored":"unsynced"):alignmentSource_;s.diagnostics["axis1ControllerCounts"]=int(p1);s.diagnostics["axis2ControllerCounts"]=int(p2);s.diagnostics["axis1HomeZeroCounts"]=int(sessionHomeCounts1_);s.diagnostics["axis2HomeZeroCounts"]=int(sessionHomeCounts2_);s.diagnostics["startupHomeCaptured"]=sessionHomeValid_;s.diagnostics["nativeCoordinateModelVersion"]=geometry_.config().nativeCoordinateModelVersion;s.diagnostics["motorControllerTransport"]="udp-11880-wire-identical-to-native-eqdrive";s.diagnostics["countsPerRev1"]=int(countsPerRev1_);s.diagnostics["countsPerRev2"]=int(countsPerRev2_);s.diagnostics["timerFreq"]=int(timerFreq_);s.diagnostics["lastGotoAxis1DeltaDeg"]=lastGotoDelta1Deg_;s.diagnostics["lastGotoAxis2DeltaDeg"]=lastGotoDelta2Deg_;s.diagnostics["lastGotoAxis1Counts"]=int(lastGotoCounts1_);s.diagnostics["lastGotoAxis2Counts"]=int(lastGotoCounts2_);s.diagnostics["lastGotoAxis1Forward"]=lastGotoForward1_;s.diagnostics["lastGotoAxis2Forward"]=lastGotoForward2_;s.diagnostics["firmwareAxis1"]=firmware1_;s.diagnostics["firmwareAxis2"]=firmware2_;if(!homeAlignmentNote_.isEmpty())s.diagnostics["homeAlignmentNote"]=homeAlignmentNote_;EquatorialCoord sky;if(geometry_.skyFromAxes(axes,sky,QDateTime::currentDateTimeUtc(),nullptr)){s.coordinate=sky;s.coordinateValid=true;}else{s.coordinate={0,0,EquatorialFrame::J2000};s.coordinateValid=false;}s.pierSide=geometry_.pierSide();s.diagnostics["pierSide"]=s.pierSide;return true;}

bool SynScanNetworkMount::stopAxis(int axis,QString*error){return axisCommand('K',axis,{},error);}
bool SynScanNetworkMount::waitStopped(int axis,int timeoutMs,QString*error){QElapsedTimer t;t.start();while(t.elapsed()<timeoutMs){qint32 p=0;bool running=false,g=false,i=false;if(!readAxis(axis,p,running,g,i,error))return false;if(!running)return true;QThread::msleep(60);}if(error)*error=QString("Axis %1 did not stop in time").arg(axis);return false;}

bool SynScanNetworkMount::gotoAxisDelta(int axis,double deltaDeg,QString*error){
    if(std::abs(deltaDeg)<1e-5)return true;
    const double cpr=axis==1?countsPerRev1_:countsPerRev2_;
    if(cpr<=0){if(error)*error="Invalid axis scale";return false;}
    qint32 before=0;bool wasRunning=false,wasGoto=false,initialized=false;
    if(!readAxis(axis,before,wasRunning,wasGoto,initialized,error))return false;
    const auto plan=oal::skywatcher_mc::makeGotoPlan(deltaDeg,cpr);
    if(!plan){if(error)*error="Could not construct Motor Controller GOTO plan";return false;}
    if(axis==1){lastGotoDelta1Deg_=deltaDeg;lastGotoCounts1_=plan->counts;lastGotoForward1_=plan->forward;}
    else{lastGotoDelta2Deg_=deltaDeg;lastGotoCounts2_=plan->counts;lastGotoForward2_=plan->forward;}
    if(!stopAxis(axis,nullptr)||!waitStopped(axis,2500,error))return false;
    QByteArray r;
    const auto send=[&](const std::string&cmd){return exchange(ba(cmd),r,1800,error);};
    // Use the exact same shared Motor Controller GOTO plan as native EQDrive:
    // mode -> target increment -> deceleration breakpoint -> start.
    if(!send(oal::skywatcher_mc::setMotionMode(axis,true,false,plan->forward)))return false;
    if(!send(oal::skywatcher_mc::setGotoIncrement(axis,plan->counts)))return false;
    if(!send(oal::skywatcher_mc::setBrakeIncrement(axis,plan->brakeCounts)))return false;
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
bool SynScanNetworkMount::setManualRate(int axis,int direction,int rateLevel,QString*error){
    direction=std::clamp(direction,-1,1);rateLevel=std::clamp(rateLevel,0,9);
    if(direction==0||rateLevel==0)return stopAxis(axis,error);
    const double cpr=axis==1?double(countsPerRev1_):double(countsPerRev2_);
    if(cpr<=0||timerFreq_==0){if(error)*error="Direct Wi-Fi mount did not report timer/axis scale required for manual slew";return false;}
    static const double mult[10]={0,1,8,16,32,64,128,400,600,800};
    const double rateDegPerHour=mult[rateLevel]*kSiderealDegPerHour;
    const quint32 period=quint32(std::clamp(std::round(double(timerFreq_)*360.0/(cpr*rateDegPerHour/3600.0)),6.0,double(0xFFFFFF)));
    if(!stopAxis(axis,nullptr)||!waitStopped(axis,2500,error))return false;QByteArray r;auto send=[&](const std::string&cmd){return exchange(ba(cmd),r,1800,error);};
    if(!send(oal::skywatcher_mc::setMotionMode(axis,false,rateLevel>=7,direction>0)))return false;
    if(!send(oal::skywatcher_mc::setStepPeriod(axis,period)))return false;
    return send(oal::skywatcher_mc::startMotion(axis));
}
bool SynScanNetworkMount::slewTo(const EquatorialCoord&t,QString*error){if(parked_){if(error)*error="Direct Wi-Fi mount is parked in OpenAstroLink; unpark it first";return false;}qint32 p1=0,p2=0;bool r=false,g=false,i=false;if(!readAxis(1,p1,r,g,i,error)||!readAxis(2,p2,r,g,i,error))return false;const auto current=axesFromEncoder(p1,p2);MechanicalAxes target;if(!geometry_.axesForSky(t,current,target,QDateTime::currentDateTimeUtc(),error))return false;const double d1=wrap180(target.axis1Deg-current.axis1Deg),d2=wrap180(target.axis2Deg-current.axis2Deg);trackingRequested_=false;if(!gotoAxisDelta(1,d1,error)){abortMotion(nullptr);return false;}if(!gotoAxisDelta(2,d2,error)){abortMotion(nullptr);return false;}return true;}
bool SynScanNetworkMount::abortMotion(QString*error){QString e1,e2;const bool a=stopAxis(1,&e1),b=stopAxis(2,&e2);trackingRequested_=false;if(!a||!b){if(error)*error=!e1.isEmpty()?e1:e2;return false;}return true;}
bool SynScanNetworkMount::syncTo(const EquatorialCoord&t,QString*error){qint32 p1=0,p2=0;bool r=false,g=false,i=false;if(!readAxis(1,p1,r,g,i,error)||!readAxis(2,p2,r,g,i,error))return false;const auto axes=axesFromEncoder(p1,p2);const auto utc=QDateTime::currentDateTimeUtc();const auto jnow=convertEquatorialFrame(t,EquatorialFrame::JNow,utc);if(geometry_.config().nativeCoordinateModelVersion>=4&&geometry_.config().type==MountGeometryType::GermanEquatorial&&std::abs(jnow.decDeg)>80.0){const double h1=geometry_.config().customHome?geometry_.config().homeAxis1Deg:0.0,h2=geometry_.config().customHome?geometry_.config().homeAxis2Deg:0.0;const double tol=std::clamp(geometry_.config().homeToleranceDeg,0.1,15.0);if(std::abs(wrap180(axes.axis1Deg-h1))<=tol&&std::abs(wrap180(axes.axis2Deg-h2))<=tol){const bool ok=geometry_.syncHome(axes,utc,error);if(ok)alignmentSource_="synscan-wifi-polar-home-sync";return ok;}}const bool ok=geometry_.sync(t,axes,utc,error);if(ok)alignmentSource_="synscan-wifi-manual-sync";return ok;}
bool SynScanNetworkMount::setTracking(bool enabled,TrackingRate rate,QString*error){if(!enabled){trackingRequested_=false;return stopAxis(1,error);}if(countsPerRev1_==0||timerFreq_==0){if(error)*error="Direct Wi-Fi mount did not report the timer/axis scale required for tracking";return false;}if(!stopAxis(1,nullptr)||!waitStopped(1,2500,error))return false;const double cpr=countsPerRev1_;const double trackingDegPerHour=rate==TrackingRate::Lunar?14.492052:rate==TrackingRate::Solar?15.0:kSiderealDegPerHour;const quint32 period=quint32(std::clamp(std::round(double(timerFreq_)*360.0/(cpr*trackingDegPerHour/3600.0)),6.0,double(0xFFFFFF)));QByteArray r;auto send=[&](const std::string&cmd){return exchange(ba(cmd),r,1800,error);}; // low-speed continuous slew mode: payload 10/11
    const int axisDirection=geometry_.trackingAxis1Direction();if(axisDirection==0){if(error)*error="This geometry requires two-axis tracking; direct rate-vector tracking is not implemented yet";return false;}const std::string mode=oal::skywatcher_mc::setMotionMode(1,false,false,axisDirection>0);if(!send(mode)||!send(oal::skywatcher_mc::setStepPeriod(1,period))||!send(oal::skywatcher_mc::startMotion(1)))return false;trackingRequested_=true;return true;}
bool SynScanNetworkMount::park(bool enabled,QString*error){
    if(!enabled){parked_=false;return true;}
    qint32 p1=0,p2=0;bool r=false,g=false,i=false;if(!readAxis(1,p1,r,g,i,error)||!readAxis(2,p2,r,g,i,error))return false;const auto current=axesFromEncoder(p1,p2);const auto target=geometry_.parkAxes();const double d1=wrap180(target.axis1Deg-current.axis1Deg),d2=wrap180(target.axis2Deg-current.axis2Deg);trackingRequested_=false;if(!gotoAxisDelta(1,d1,error)){abortMotion(nullptr);return false;}if(!gotoAxisDelta(2,d2,error)){abortMotion(nullptr);return false;}parked_=true;return true;
}
bool SynScanNetworkMount::pulseGuide(GuideDirection,int,QString*error){if(error)*error="Direct SynScan/EQDrive Wi-Fi pulse guide is not enabled until native tracking direction is HIL-qualified";return false;}
bool SynScanNetworkMount::manualSlew(int a1,int a2,int rate,QString*error){trackingRequested_=false;QString e;if(!setManualRate(1,a1,rate,&e)){if(error)*error=e;return false;}if(!setManualRate(2,a2,rate,&e)){stopAxis(1,nullptr);if(error)*error=e;return false;}return true;}
}
