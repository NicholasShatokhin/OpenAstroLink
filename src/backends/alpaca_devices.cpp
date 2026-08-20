#include "backends/alpaca_devices.h"

namespace oas {
QUrl AlpacaDeviceBase::endpoint(const QString&n)const{return appendPath(base_,n);}
QUrlQuery AlpacaDeviceBase::transactionForm()const{QUrlQuery q;q.addQueryItem("ClientID",QString::number(clientId_));q.addQueryItem("ClientTransactionID",QString::number(transactionId_++));return q;}
bool AlpacaDeviceBase::check(const HttpJsonClient::Reply&r,QString*e)const{if(!r.ok()){if(e)*e=r.error.isEmpty()?QString("HTTP %1").arg(r.httpStatus):r.error;return false;}const int n=r.json.value("ErrorNumber").toInt(0);if(n!=0){if(e)*e=r.json.value("ErrorMessage").toString(QString("Alpaca error %1").arg(n));return false;}return true;}

bool AlpacaMount::connectDevice(QString*e){auto q=transactionForm();q.addQueryItem("Connected","true");auto r=http_.putForm(endpoint("connected"),q);if(!check(r,e)){state_=ConnectionState::Error;return false;}state_=ConnectionState::Connected;return true;}
void AlpacaMount::disconnectDevice(){auto q=transactionForm();q.addQueryItem("Connected","false");http_.putForm(endpoint("connected"),q);state_=ConnectionState::Disconnected;}
bool AlpacaMount::status(MountStatus&s,QString*e){auto ra=http_.get(QUrl(endpoint("rightascension").toString()+"?"+transactionForm().toString()));if(!check(ra,e))return false;auto de=http_.get(QUrl(endpoint("declination").toString()+"?"+transactionForm().toString()));if(!check(de,e))return false;s.connection=state_;s.coordinate.raDeg=ra.json.value("Value").toDouble()*15.0;s.coordinate.decDeg=de.json.value("Value").toDouble();auto tr=http_.get(QUrl(endpoint("tracking").toString()+"?"+transactionForm().toString()));if(check(tr,nullptr))s.tracking=tr.json.value("Value").toBool();auto sl=http_.get(QUrl(endpoint("slewing").toString()+"?"+transactionForm().toString()));if(check(sl,nullptr))s.slewing=sl.json.value("Value").toBool();auto pa=http_.get(QUrl(endpoint("atpark").toString()+"?"+transactionForm().toString()));if(check(pa,nullptr))s.parked=pa.json.value("Value").toBool();return true;}
bool AlpacaMount::slewTo(const EquatorialCoord&t,QString*e){auto q=transactionForm();q.addQueryItem("RightAscension",QString::number(t.raDeg/15.0,'g',14));q.addQueryItem("Declination",QString::number(t.decDeg,'g',14));return check(http_.putForm(endpoint("slewtocoordinatesasync"),q,30000),e);}
bool AlpacaMount::abortMotion(QString*e){return check(http_.putForm(endpoint("abortslew"),transactionForm()),e);}
bool AlpacaMount::syncTo(const EquatorialCoord&t,QString*e){auto q=transactionForm();q.addQueryItem("RightAscension",QString::number(t.raDeg/15.0,'g',14));q.addQueryItem("Declination",QString::number(t.decDeg,'g',14));return check(http_.putForm(endpoint("synctocoordinates"),q),e);}
bool AlpacaMount::setTracking(bool v,QString*e){auto q=transactionForm();q.addQueryItem("Tracking",v?"true":"false");return check(http_.putForm(endpoint("tracking"),q),e);}
bool AlpacaMount::park(bool v,QString*e){return check(http_.putForm(endpoint(v?"park":"unpark"),transactionForm(),30000),e);}
bool AlpacaMount::pulseGuide(GuideDirection d,int ms,QString*e){int dir=0;if(d==GuideDirection::North)dir=0;else if(d==GuideDirection::South)dir=1;else if(d==GuideDirection::East)dir=2;else dir=3;auto q=transactionForm();q.addQueryItem("Direction",QString::number(dir));q.addQueryItem("Duration",QString::number(ms));return check(http_.putForm(endpoint("pulseguide"),q),e);}

bool AlpacaFocuser::connectDevice(QString*e){auto q=transactionForm();q.addQueryItem("Connected","true");if(!check(http_.putForm(endpoint("connected"),q),e)){state_=ConnectionState::Error;return false;}state_=ConnectionState::Connected;return true;}
void AlpacaFocuser::disconnectDevice(){auto q=transactionForm();q.addQueryItem("Connected","false");http_.putForm(endpoint("connected"),q);state_=ConnectionState::Disconnected;}
bool AlpacaFocuser::status(FocuserStatus&s,QString*e){auto p=http_.get(QUrl(endpoint("position").toString()+"?"+transactionForm().toString()));if(!check(p,e))return false;s.connection=state_;s.position=p.json.value("Value").toInt();auto m=http_.get(QUrl(endpoint("ismoving").toString()+"?"+transactionForm().toString()));if(check(m,nullptr))s.moving=m.json.value("Value").toBool();auto t=http_.get(QUrl(endpoint("temperature").toString()+"?"+transactionForm().toString()));if(check(t,nullptr))s.temperatureC=t.json.value("Value").toDouble();return true;}
bool AlpacaFocuser::moveAbsolute(int p,QString*e){auto q=transactionForm();q.addQueryItem("Position",QString::number(p));return check(http_.putForm(endpoint("move"),q),e);}
bool AlpacaFocuser::moveRelative(int d,QString*e){FocuserStatus s;if(!status(s,e))return false;return moveAbsolute(s.position+d,e);}
bool AlpacaFocuser::halt(QString*e){return check(http_.putForm(endpoint("halt"),transactionForm()),e);}
}
