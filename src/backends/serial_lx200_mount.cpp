#include "backends/serial_lx200_mount.h"
#include <cmath>
#include <cstdio>

namespace oas {
bool SerialLx200Mount::connectDevice(QString *error){serial_.setPortName(portName_);serial_.setBaudRate(baud_);serial_.setDataBits(QSerialPort::Data8);serial_.setParity(QSerialPort::NoParity);serial_.setStopBits(QSerialPort::OneStop);serial_.setFlowControl(QSerialPort::NoFlowControl);if(!serial_.open(QIODevice::ReadWrite)){state_=ConnectionState::Error;if(error)*error=serial_.errorString();return false;}state_=ConnectionState::Connected;return true;}
void SerialLx200Mount::disconnectDevice(){serial_.close();slewActive_=false;state_=ConnectionState::Disconnected;}
QByteArray SerialLx200Mount::command(const QByteArray &cmd,int timeout,QString *error){serial_.clear();if(serial_.write(cmd)!=cmd.size()||!serial_.waitForBytesWritten(timeout)){if(error)*error=serial_.errorString();return{};}QByteArray out;while(serial_.waitForReadyRead(timeout)){out+=serial_.readAll();if(out.contains('#')||out.size()>128)break;}return out.trimmed();}
bool SerialLx200Mount::parseRa(const QByteArray&s,double&d){int h=0,m=0,sec=0;if(std::sscanf(s.constData(),"%d:%d:%d",&h,&m,&sec)<2)return false;d=15.0*(h+m/60.0+sec/3600.0);return true;}
bool SerialLx200Mount::parseDec(const QByteArray&s,double&d){char sign='+';int deg=0,m=0,sec=0;int n=std::sscanf(s.constData(),"%c%d*%d:%d",&sign,&deg,&m,&sec);if(n<3)n=std::sscanf(s.constData(),"%c%d:%d:%d",&sign,&deg,&m,&sec);if(n<3)return false;d=(deg+m/60.0+sec/3600.0)*(sign=='-'?-1.0:1.0);return true;}
QByteArray SerialLx200Mount::raString(double d){
    d=std::fmod(d+360.0,360.0);
    int totalSec=int(std::lround((d/15.0)*3600.0))%(24*3600);
    int hh=totalSec/3600,mm=(totalSec%3600)/60,ss=totalSec%60;
    return QStringLiteral("%1:%2:%3")
        .arg(hh,2,10,QLatin1Char('0'))
        .arg(mm,2,10,QLatin1Char('0'))
        .arg(ss,2,10,QLatin1Char('0'))
        .toLatin1();
}
QByteArray SerialLx200Mount::decString(double d){
    const char sign=d<0?'-':'+';
    d=std::abs(d);
    int totalSec=int(std::lround(d*3600.0));
    int dd=totalSec/3600,mm=(totalSec%3600)/60,ss=totalSec%60;
    return QStringLiteral("%1%2*%3:%4")
        .arg(QLatin1Char(sign))
        .arg(dd,2,10,QLatin1Char('0'))
        .arg(mm,2,10,QLatin1Char('0'))
        .arg(ss,2,10,QLatin1Char('0'))
        .toLatin1();
}
bool SerialLx200Mount::status(MountStatus&s,QString*e){if(state_!=ConnectionState::Connected){if(e)*e="Mount disconnected";return false;}double ra=0,dec=0;auto r=command(":GR#",1000,e);auto d=command(":GD#",1000,e);if(!parseRa(r,ra)||!parseDec(d,dec)){if(e&&e->isEmpty())*e="Cannot parse mount coordinates";return false;}s.connection=state_;s.coordinate={ra,dec};s.tracking=true;if(slewActive_){double dra=std::abs(ra-slewTarget_.raDeg);dra=std::min(dra,360.0-dra);const double ddec=std::abs(dec-slewTarget_.decDeg);if(dra<=0.03&&ddec<=0.03)slewActive_=false;}s.slewing=slewActive_;return true;}
bool SerialLx200Mount::slewTo(const EquatorialCoord&t,QString*e){if(command(QByteArray(":Sr")+raString(t.raDeg)+"#",1000,e).isEmpty())return false;if(command(QByteArray(":Sd")+decString(t.decDeg)+"#",1000,e).isEmpty())return false;auto r=command(":MS#",2000,e);const bool ok=!r.isEmpty()&&r[0]=='0';if(ok){slewTarget_=t;slewActive_=true;}return ok;}
bool SerialLx200Mount::abortMotion(QString*e){QString local;command(":Q#",1000,&local);slewActive_=false;if(e)*e=local;return local.isEmpty();}
bool SerialLx200Mount::syncTo(const EquatorialCoord&t,QString*e){if(command(QByteArray(":Sr")+raString(t.raDeg)+"#",1000,e).isEmpty())return false;if(command(QByteArray(":Sd")+decString(t.decDeg)+"#",1000,e).isEmpty())return false;return !command(":CM#",2000,e).isEmpty();}
bool SerialLx200Mount::setTracking(bool,QString*e){if(e)*e="Tracking control is not standardized by LX200 backend";return false;}
bool SerialLx200Mount::park(bool,QString*e){if(e)*e="Park is not standardized by this LX200 backend";return false;}
bool SerialLx200Mount::pulseGuide(GuideDirection,int,QString*e){if(e)*e="PulseGuide is not implemented for this serial profile";return false;}
}
