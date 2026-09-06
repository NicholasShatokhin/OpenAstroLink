#include "gui/stellarium_remote_control_client.h"
#include "core/equatorial_frames.h"
#include <QUrlQuery>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace oas {
namespace {
QString number(double value,int precision=8){return QString::number(value,'f',precision);}
QString replyError(const HttpJsonClient::Reply &r){return !r.error.isEmpty()?r.error:QString("HTTP %1: %2").arg(r.httpStatus).arg(QString::fromUtf8(r.bytes.left(240)));}
QString suffixKey(const QString &propertyId){const int dot=propertyId.lastIndexOf('.');return dot>=0?propertyId.mid(dot+1):propertyId;}
}

bool StellariumRemoteControlClient::propertyMap(const QUrl &baseUrl,QHash<QString,QString> &ids,QString *error){
    const auto r=http_.get(appendPath(baseUrl,"api/stelproperty/list"),5000);if(!r.ok()){if(error)*error=replyError(r);return false;}
    for(auto it=r.json.begin();it!=r.json.end();++it){const QString suffix=suffixKey(it.key());if(suffix.startsWith("fovRectangularMarker")||suffix=="fovCenterMarkerDisplayed")ids.insert(suffix,it.key());}
    const QStringList required{"fovRectangularMarkerWidth","fovRectangularMarkerHeight","fovRectangularMarkerRotationAngle","fovRectangularMarkerDisplayed","fovCenterMarkerDisplayed"};
    for(const auto &k:required)if(!ids.contains(k)){if(error)*error="Stellarium Remote Control does not expose required SpecialMarkersMgr property: "+k;return false;}
    return true;
}

bool StellariumRemoteControlClient::setProperty(const QUrl &baseUrl,const QString &id,const QString &value,QString *error){
    QUrlQuery form;form.addQueryItem("id",id);form.addQueryItem("value",value);const auto r=http_.postForm(appendPath(baseUrl,"api/stelproperty/set"),form,5000);
    if(!r.ok()){if(error)*error=replyError(r);return false;}return true;
}

bool StellariumRemoteControlClient::setView(const QUrl &baseUrl,const EquatorialCoord &center,double fovDeg,QString *error){
    constexpr double D=3.14159265358979323846/180.0;const auto j=convertEquatorialFrame(center,EquatorialFrame::J2000);const double ra=j.raDeg*D,dec=j.decDeg*D;
    const double x=std::cos(dec)*std::cos(ra),y=std::cos(dec)*std::sin(ra),z=std::sin(dec);
    QUrlQuery view;view.addQueryItem("j2000",QString("[%1,%2,%3]").arg(number(x,12)).arg(number(y,12)).arg(number(z,12)));
    auto r=http_.postForm(appendPath(baseUrl,"api/main/view"),view,5000);if(!r.ok()){if(error)*error=replyError(r);return false;}
    QUrlQuery fov;fov.addQueryItem("fov",number(std::clamp(fovDeg,0.05,120.0),4));r=http_.postForm(appendPath(baseUrl,"api/main/fov"),fov,5000);
    if(!r.ok()){if(error)*error=replyError(r);return false;}return true;
}

bool StellariumRemoteControlClient::sendFrame(const QUrl &baseUrl,const SkyFrame &frame,QString *error){
    if(!baseUrl.isValid()||baseUrl.host().isEmpty()){if(error)*error="Stellarium Remote Control URL is invalid";return false;}
    if(!frame.valid||frame.widthDeg<=0.0||frame.heightDeg<=0.0){if(error)*error="Selected OAL frame is not available";return false;}
    QHash<QString,QString> ids;if(!propertyMap(baseUrl,ids,error))return false;
    const struct { const char *suffix; QString value; } props[] = {
        {"fovRectangularMarkerWidth",number(frame.widthDeg,8)},
        {"fovRectangularMarkerHeight",number(frame.heightDeg,8)},
        {"fovRectangularMarkerRotationAngle",number(frame.rotationDeg,6)},
        {"fovRectangularMarkerDisplayed","true"},
        {"fovCenterMarkerDisplayed","true"}
    };
    for(const auto &p:props)if(!setProperty(baseUrl,ids.value(QString::fromLatin1(p.suffix)),p.value,error))return false;
    return setView(baseUrl,frame.center,1.8*std::max(frame.widthDeg,frame.heightDeg),error);
}

bool StellariumRemoteControlClient::hideFrame(const QUrl &baseUrl,QString *error){
    if(!baseUrl.isValid()||baseUrl.host().isEmpty()){if(error)*error="Stellarium Remote Control URL is invalid";return false;}
    QHash<QString,QString> ids;if(!propertyMap(baseUrl,ids,error))return false;
    if(!setProperty(baseUrl,ids.value("fovRectangularMarkerDisplayed"),"false",error))return false;
    return setProperty(baseUrl,ids.value("fovCenterMarkerDisplayed"),"false",error);
}

} // namespace oas
