#include "backends/synscan_app_mount.h"
#include <QElapsedTimer>
#include <QHostInfo>
#include <QNetworkDatagram>
#include <QUrl>
#include <algorithm>

namespace oas {
SynScanAppMount::SynScanAppMount(QString e):endpoint_(std::move(e)){if(endpoint_.trimmed().isEmpty())endpoint_="auto";}
bool SynScanAppMount::parseEndpoint(QHostAddress&host,quint16&port,QString*error)const{
    QString text=endpoint_.trimmed();
    if(text.isEmpty()||text.compare("auto",Qt::CaseInsensitive)==0){
        // SynScan App Protocol is served by the phone/PC running SynScan Pro,
        // not by the Wi-Fi adapter itself. Discover it with a harmless UDP
        // ServerVersion broadcast on the documented 11881 port.
        QUdpSocket probe;probe.setSocketOption(QAbstractSocket::MulticastTtlOption,1);
        if(!probe.bind(QHostAddress::AnyIPv4,0,QUdpSocket::ShareAddress)){if(error)*error="Could not bind SynScan discovery socket: "+probe.errorString();return false;}
        const QByteArray q("ServerVersion");probe.writeDatagram(q,QHostAddress::Broadcast,11881);
        QElapsedTimer timer;timer.start();while(timer.elapsed()<1800){if(!probe.waitForReadyRead(150))continue;while(probe.hasPendingDatagrams()){const auto dg=probe.receiveDatagram();const auto r=QString::fromLatin1(dg.data()).trimmed();if(r.startsWith("Ok,ServerVersion")){host=dg.senderAddress();port=11881;return true;}}}
        // Same-machine SynScan Pro is common on Windows; try loopback after the
        // broadcast so Auto still works when broadcast is filtered.
        host=QHostAddress::LocalHost;port=11881;return true;
    }
    QUrl u=text.contains("://")?QUrl(text):QUrl("udp://"+text);
    if(!u.isValid()||u.host().isEmpty()){if(error)*error="SynScan App endpoint must be auto or the IP/host of the phone/PC running SynScan Pro[:11881] (not the mount Wi-Fi adapter IP)";return false;}
    port=quint16(u.port(11881));host=QHostAddress(u.host());
    if(host.isNull()){const auto info=QHostInfo::fromName(u.host());for(const auto&a:info.addresses())if(a.protocol()==QAbstractSocket::IPv4Protocol){host=a;break;}}
    if(host.isNull()){if(error)*error="Could not resolve SynScan App host: "+u.host();return false;}return true;
}
bool SynScanAppMount::command(const QString&cmd,QStringList*fields,QString*error,int timeoutMs){
    if(state_!=ConnectionState::Connected&&cmd!="ServerVersion"){if(error)*error="SynScan App network telescope is not connected";return false;}
    while(socket_.hasPendingDatagrams())socket_.receiveDatagram();
    const QByteArray bytes=cmd.toLatin1();if(socket_.writeDatagram(bytes,host_,port_)!=bytes.size()){if(error)*error="SynScan App UDP send failed: "+socket_.errorString();return false;}
    QElapsedTimer timer;timer.start();QByteArray response;
    while(timer.elapsed()<timeoutMs){if(!socket_.waitForReadyRead(std::min(100,std::max(1,timeoutMs-int(timer.elapsed())))))continue;auto dg=socket_.receiveDatagram();if(dg.senderAddress()!=host_)continue;response=dg.data();break;}
    if(response.isEmpty()){if(error)*error="SynScan App UDP response timeout to "+host_.toString()+":"+QString::number(port_);return false;}
    const QStringList parts=QString::fromLatin1(response).trimmed().split(',',Qt::KeepEmptyParts);
    if(parts.size()<2){if(error)*error="Invalid SynScan App response: "+QString::fromLatin1(response);return false;}
    if(parts[0]!="Ok"){if(error)*error=QString("SynScan App rejected %1: %2").arg(cmd.section(',',0,0),QString::fromLatin1(response));return false;}
    const QString expected=cmd.section(',',0,0);if(parts[1]!=expected){if(error)*error=QString("SynScan App response mismatch: expected %1, got %2").arg(expected,parts[1]);return false;}
    if(fields)*fields=parts.mid(2);return true;
}
bool SynScanAppMount::connectDevice(QString*error){state_=ConnectionState::Connecting;if(!parseEndpoint(host_,port_,error)){state_=ConnectionState::Error;return false;}if(!socket_.bind(QHostAddress::AnyIPv4,0,QUdpSocket::ShareAddress)){if(error)*error="Could not bind SynScan App UDP client: "+socket_.errorString();state_=ConnectionState::Error;return false;}state_=ConnectionState::Connected;QStringList f;if(!command("ServerVersion",&f,error,4000)){socket_.close();state_=ConnectionState::Error;return false;}return true;}
void SynScanAppMount::disconnectDevice(){socket_.close();state_=ConnectionState::Disconnected;}
bool SynScanAppMount::boolGet(const QString&p,bool&v,QString*e){QStringList f;if(!command(p+"Get",&f,e)||f.isEmpty())return false;bool ok=false;const int x=f[0].toInt(&ok);if(!ok){if(e)*e="Invalid SynScan App boolean response for "+p;return false;}v=x!=0;return true;}
bool SynScanAppMount::status(MountStatus&s,QString*error){
    QStringList f;if(!command("RightAscensionDeclinationGet",&f,error)||f.size()<2)return false;bool a=false,b=false;const double raHours=f[0].toDouble(&a),dec=f[1].toDouble(&b);if(!a||!b){if(error)*error="Invalid SynScan App RA/DEC response";return false;}
    s.connection=state_;s.coordinate={raHours*15.0,dec};bool x=false;if(boolGet("Tracking",x,nullptr))s.tracking=x;if(boolGet("Slewing",x,nullptr))s.slewing=x;if(boolGet("AtPark",x,nullptr))s.parked=x;s.pierSide="unknown";
    QStringList side;if(command("SideOfPierGet",&side,nullptr,1200)&&!side.isEmpty()){bool ok=false;const int p=side[0].toInt(&ok);if(ok)s.pierSide=p==0?"east":p==1?"west":"unknown";}return true;
}
bool SynScanAppMount::slewTo(const EquatorialCoord&t,QString*e){return command(QString("SlewToCoordinatesAsync,%1,%2").arg(t.raDeg/15.0,0,'f',9).arg(t.decDeg,0,'f',9),nullptr,e,4000);}
bool SynScanAppMount::abortMotion(QString*e){return command("AbortSlew",nullptr,e);}
bool SynScanAppMount::syncTo(const EquatorialCoord&t,QString*e){return command(QString("SyncToCoordinates,%1,%2").arg(t.raDeg/15.0,0,'f',9).arg(t.decDeg,0,'f',9),nullptr,e,4000);}
bool SynScanAppMount::setTracking(bool enabled,QString*e){return command(QString("TrackingSet,%1").arg(enabled?1:0),nullptr,e);}
bool SynScanAppMount::park(bool enabled,QString*e){return command(enabled?"Park":"Unpark",nullptr,e,5000);}
bool SynScanAppMount::pulseGuide(GuideDirection dir,int ms,QString*e){int d=0;switch(dir){case GuideDirection::North:d=0;break;case GuideDirection::South:d=1;break;case GuideDirection::East:d=2;break;case GuideDirection::West:d=3;break;}return command(QString("PulseGuide,%1,%2").arg(d).arg(std::clamp(ms,1,5000)),nullptr,e,std::max(3000,ms+1500));}
}
