#include "core/mount_geometry.h"
#include "core/equatorial_frames.h"
#include <algorithm>
#include <cmath>

namespace oas { namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDeg = kPi / 180.0;

double wrap360(double x){x=std::fmod(x,360.0);if(x<0)x+=360.0;return x;}
double wrap180(double x){x=wrap360(x);if(x>180.0)x-=360.0;return x;}
double julianDateUtc(const QDateTime &utc){return 2440587.5 + double(utc.toUTC().toMSecsSinceEpoch())/86400000.0;}
double signedNearest(double delta){return wrap180(delta);}

MechanicalAxes equatorialAxes(const EquatorialCoord &jnow, double lstDeg, bool gem, const QString &pier){
    const double ha=wrap180(lstDeg-jnow.raDeg);
    if(!gem)return {90.0+ha,jnow.decDeg,true};
    if(pier.compare("west",Qt::CaseInsensitive)==0)
        return {270.0+ha,180.0-jnow.decDeg,true};
    return {90.0+ha,jnow.decDeg,true};
}

EquatorialCoord equatorialFromAxes(MechanicalAxes a,double lstDeg,bool gem,const QString &pier){
    double dec=a.axis2Deg;
    double ha=a.axis1Deg-90.0;
    if(gem&&pier.compare("west",Qt::CaseInsensitive)==0){ha=a.axis1Deg-270.0;dec=180.0-a.axis2Deg;}
    while(dec>180.0)dec-=360.0;while(dec<-180.0)dec+=360.0;
    if(dec>90.0)dec=180.0-dec;if(dec<-90.0)dec=-180.0-dec;
    return {wrap360(lstDeg-wrap180(ha)),std::clamp(dec,-90.0,90.0),EquatorialFrame::JNow};
}
}

QString mountGeometryTypeName(MountGeometryType type){
    switch(type){
    case MountGeometryType::GermanEquatorial:return "german-equatorial";
    case MountGeometryType::ForkEquatorial:return "fork-equatorial";
    case MountGeometryType::AltAzimuth:return "alt-azimuth";
    case MountGeometryType::AltAzimuthDerotator:return "alt-azimuth-derotator";
    case MountGeometryType::EquatorialPlatform:return "equatorial-platform";
    case MountGeometryType::CustomTwoAxis:return "custom-two-axis";
    }return "german-equatorial";
}
MountGeometryType mountGeometryTypeFromString(const QString&t,MountGeometryType fallback){
    const auto s=t.trimmed().toLower();
    if(s=="german-equatorial"||s=="gem")return MountGeometryType::GermanEquatorial;
    if(s=="fork-equatorial"||s=="fork")return MountGeometryType::ForkEquatorial;
    if(s=="alt-azimuth"||s=="altaz"||s=="alt-az")return MountGeometryType::AltAzimuth;
    if(s=="alt-azimuth-derotator"||s=="altaz-derotator")return MountGeometryType::AltAzimuthDerotator;
    if(s=="equatorial-platform")return MountGeometryType::EquatorialPlatform;
    if(s=="custom-two-axis")return MountGeometryType::CustomTwoAxis;
    return fallback;
}

double localSiderealTimeDeg(const QDateTime &utc,double longitudeDeg){
    const double jd=julianDateUtc(utc);const double T=(jd-2451545.0)/36525.0;
    const double gmst=280.46061837+360.98564736629*(jd-2451545.0)+0.000387933*T*T-(T*T*T)/38710000.0;
    return wrap360(gmst+longitudeDeg);
}

void MountGeometryModel::configure(MountGeometryConfig c,ObserverLocation o){config_=std::move(c);observer_=o;synced_=false;pierSide_="unknown";}

MechanicalAxes MountGeometryModel::canonicalAxesForSky(const EquatorialCoord &skyJNow,const QDateTime&utc,QString*pier,QString*error)const{
    const double lst=localSiderealTimeDeg(utc,observer_.longitudeDeg);
    switch(config_.type){
    case MountGeometryType::GermanEquatorial:{
        QString side=config_.preferredPierSide.toLower();if(side!="east"&&side!="west")side="east";if(pier)*pier=side;
        return equatorialAxes(skyJNow,lst,true,side);
    }
    case MountGeometryType::ForkEquatorial:
    case MountGeometryType::EquatorialPlatform:{if(pier)*pier="none";return equatorialAxes(skyJNow,lst,false,{});}
    case MountGeometryType::AltAzimuth:
    case MountGeometryType::AltAzimuthDerotator:{
        const double lat=observer_.latitudeDeg*kDeg,dec=skyJNow.decDeg*kDeg,ha=wrap180(lst-skyJNow.raDeg)*kDeg;
        const double sinAlt=std::sin(lat)*std::sin(dec)+std::cos(lat)*std::cos(dec)*std::cos(ha);
        const double alt=std::asin(std::clamp(sinAlt,-1.0,1.0));
        const double y=-std::sin(ha)*std::cos(dec);
        const double x=std::sin(dec)*std::cos(lat)-std::cos(dec)*std::sin(lat)*std::cos(ha);
        if(pier)*pier="none";return {wrap360(std::atan2(y,x)/kDeg),alt/kDeg,true};
    }
    case MountGeometryType::CustomTwoAxis:{if(pier)*pier="none";return {skyJNow.raDeg,skyJNow.decDeg,true};}
    }
    if(error)*error="Unsupported mount geometry";return {};
}

EquatorialCoord MountGeometryModel::skyJNowFromCanonicalAxes(const MechanicalAxes&a,const QDateTime&utc,QString*error)const{
    const double lst=localSiderealTimeDeg(utc,observer_.longitudeDeg);
    switch(config_.type){
    case MountGeometryType::GermanEquatorial:return equatorialFromAxes(a,lst,true,pierSide_);
    case MountGeometryType::ForkEquatorial:
    case MountGeometryType::EquatorialPlatform:return equatorialFromAxes(a,lst,false,{});
    case MountGeometryType::AltAzimuth:
    case MountGeometryType::AltAzimuthDerotator:{
        const double az=a.axis1Deg*kDeg,alt=a.axis2Deg*kDeg,lat=observer_.latitudeDeg*kDeg;
        const double sinDec=std::sin(alt)*std::sin(lat)+std::cos(alt)*std::cos(lat)*std::cos(az);
        const double dec=std::asin(std::clamp(sinDec,-1.0,1.0));
        const double y=-std::sin(az)*std::cos(alt);
        const double x=std::sin(alt)*std::cos(lat)-std::cos(alt)*std::sin(lat)*std::cos(az);
        const double ha=std::atan2(y,x)/kDeg;
        return {wrap360(lst-ha),dec/kDeg,EquatorialFrame::JNow};
    }
    case MountGeometryType::CustomTwoAxis:return {wrap360(a.axis1Deg),std::clamp(a.axis2Deg,-90.0,90.0),EquatorialFrame::JNow};
    }
    if(error)*error="Unsupported mount geometry";return {};
}

bool MountGeometryModel::sync(const EquatorialCoord&sky,const MechanicalAxes&actual,const QDateTime&utc,QString*error){
    if(!actual.valid){if(error)*error="Mechanical axis position is not available";return false;}
    const auto jnow=convertEquatorialFrame(sky,EquatorialFrame::JNow,utc);QString side;
    const auto canonical=canonicalAxesForSky(jnow,utc,&side,error);if(!canonical.valid)return false;
    syncActual_=actual;syncCanonical_=canonical;synced_=true;pierSide_=side;return true;
}

bool MountGeometryModel::skyFromAxes(const MechanicalAxes&actual,EquatorialCoord&skyJ2000,const QDateTime&utc,QString*error)const{
    if(!synced_){if(error)*error="Mount geometry has not been synced";return false;}if(!actual.valid){if(error)*error="Mechanical axis position is not available";return false;}
    MechanicalAxes c{syncCanonical_.axis1Deg + (actual.axis1Deg-syncActual_.axis1Deg)/double(config_.axis1Sign>=0?1:-1),
                     syncCanonical_.axis2Deg + (actual.axis2Deg-syncActual_.axis2Deg)/double(config_.axis2Sign>=0?1:-1),true};
    auto jnow=skyJNowFromCanonicalAxes(c,utc,error);skyJ2000=convertEquatorialFrame(jnow,EquatorialFrame::J2000,utc);return true;
}

bool MountGeometryModel::axesForSky(const EquatorialCoord&sky,const MechanicalAxes&current,MechanicalAxes&target,const QDateTime&utc,QString*error)const{
    if(!synced_){if(error)*error="Mount geometry requires one Sync on a known sky position first";return false;}if(!current.valid){if(error)*error="Mechanical axis position is not available";return false;}
    const auto jnow=convertEquatorialFrame(sky,EquatorialFrame::JNow,utc);QString side=pierSide_;
    auto canonical=canonicalAxesForSky(jnow,utc,&side,error);if(!canonical.valid)return false;
    // Keep the current pier branch until a qualified meridian-flip planner is enabled.
    if(config_.type==MountGeometryType::GermanEquatorial&&side!=pierSide_)
        canonical=equatorialAxes(jnow,localSiderealTimeDeg(utc,observer_.longitudeDeg),true,pierSide_);
    const double d1=signedNearest(canonical.axis1Deg-syncCanonical_.axis1Deg);
    double d2=canonical.axis2Deg-syncCanonical_.axis2Deg;
    if(config_.type==MountGeometryType::AltAzimuth||config_.type==MountGeometryType::AltAzimuthDerotator||config_.type==MountGeometryType::CustomTwoAxis)d2=signedNearest(d2);
    target={syncActual_.axis1Deg+(config_.axis1Sign>=0?1.0:-1.0)*d1,
            syncActual_.axis2Deg+(config_.axis2Sign>=0?1.0:-1.0)*d2,true};
    // Unwrap target near the current controller coordinate for shortest safe motion.
    target.axis1Deg=current.axis1Deg+signedNearest(target.axis1Deg-current.axis1Deg);
    if(config_.type!=MountGeometryType::GermanEquatorial&&config_.type!=MountGeometryType::ForkEquatorial)
        target.axis2Deg=current.axis2Deg+signedNearest(target.axis2Deg-current.axis2Deg);
    return true;
}

int MountGeometryModel::trackingAxis1Direction()const{
    // For equatorial geometries hour angle increases with sidereal time. Alt-Az
    // tracking is two-axis and is handled as future rate-vector work.
    if(config_.type==MountGeometryType::GermanEquatorial||config_.type==MountGeometryType::ForkEquatorial||config_.type==MountGeometryType::EquatorialPlatform)
        return config_.axis1Sign>=0?1:-1;
    return 0;
}

} // namespace oas
