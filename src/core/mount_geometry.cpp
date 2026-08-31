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

struct Vec3 { double x{0.0}, y{0.0}, z{0.0}; };

// Sky-Watcher/EQDrive raw encoder axes are best treated as a spherical
// telescope frame whose pole is the mount's polar axis.  This is the same
// kinematic representation used by mature Sky-Watcher implementations: build
// a horizon direction vector, rotate it into the polar-aligned mount frame,
// and convert that vector back to two spherical motor angles.
Vec3 telescopeVectorFromAzAlt(double azDeg,double altDeg){
    const double a=wrap360(azDeg)*kDeg,h=std::clamp(altDeg,-90.0,90.0)*kDeg;
    const double ch=std::cos(h);
    // Azimuth is north-through-east.  The telescope-vector convention keeps Y
    // negative toward increasing azimuth, matching the Sky-Watcher raw-axis
    // support functions used by INDI.
    return {ch*std::cos(a),-ch*std::sin(a),std::sin(h)};
}
Vec3 rotateAroundY(const Vec3&v,double angleDeg){
    const double a=angleDeg*kDeg,c=std::cos(a),s=std::sin(a);
    return {c*v.x+s*v.z,v.y,-s*v.x+c*v.z};
}
MechanicalAxes sphericalAxesFromVector(const Vec3&v){
    const double r=std::hypot(v.x,v.y);
    return {wrap360(std::atan2(-v.y,v.x)/kDeg),std::atan2(v.z,r)/kDeg,true};
}
MechanicalAxes polarAlignedAxesForSky(const EquatorialCoord &jnow,const ObserverLocation&observer,const QDateTime&utc){
    const auto hor=equatorialToHorizontal(jnow,observer,utc);
    const auto horizon=telescopeVectorFromAzAlt(hor.azDeg,hor.altDeg);
    // North: horizon -> polar frame by latitude-90 deg.  South uses the
    // symmetric latitude+90 deg rotation.
    const double rotation=observer.latitudeDeg>=0.0?observer.latitudeDeg-90.0:observer.latitudeDeg+90.0;
    return sphericalAxesFromVector(rotateAroundY(horizon,rotation));
}
EquatorialCoord skyFromPolarAlignedAxes(const MechanicalAxes&a,const ObserverLocation&observer,const QDateTime&utc){
    const auto mountVector=telescopeVectorFromAzAlt(a.axis1Deg,a.axis2Deg);
    const double rotation=observer.latitudeDeg>=0.0?90.0-observer.latitudeDeg:-90.0-observer.latitudeDeg;
    const auto horizon=sphericalAxesFromVector(rotateAroundY(mountVector,rotation));
    return horizontalToEquatorial({horizon.axis1Deg,horizon.axis2Deg},observer,EquatorialFrame::JNow,utc);
}



// Native/direct Sky-Watcher Motor Controller GEM model v6 (OpenAstroLink 0.2.10.44).
//
// The controller startup counts are still the user-defined physical Home/Park
// and are exposed as Axis1=0deg, Axis2=0deg.  The sky transform, however, is
// no longer inferred from an arbitrary HA phase.  It follows the same
// telescope-direction-vector construction used by the mature INDI SkyWatcher
// driver for a mount approximately aligned on the celestial pole:
//
//   sky JNow -> horizontal direction vector -> rotate horizon frame into the
//   polar-aligned mount frame -> spherical mount azimuth/altitude.
//
// The public mechanical coordinates are then
//   Axis1 = mount-frame azimuth
//   Axis2 = 90deg - mount-frame altitude (signed on the flipped GEM branch).
//
// Consequently physical Home is exactly 0,0, while an eastern target is not
// forced through the erroneous v5 H+90deg phase.  A GEM has two equivalent
// representations of the same telescope vector: (A,+P) and (A+180deg,-P).
// axesForSky() chooses the branch requiring the shortest controller motion.
MechanicalAxes gemTelescopeFrameAxesForSky(const EquatorialCoord &jnow,
                                             const ObserverLocation &observer,
                                             const QDateTime &utc){
    const auto spherical=polarAlignedAxesForSky(jnow,observer,utc);
    if(!spherical.valid)return {};
    return {wrap180(spherical.axis1Deg),
            std::clamp(90.0-spherical.axis2Deg,0.0,180.0),true};
}

EquatorialCoord skyFromGemTelescopeFrameAxes(MechanicalAxes a,
                                               const ObserverLocation &observer,
                                               const QDateTime &utc){
    // Fold the alternate GEM branch back into the primary spherical telescope
    // direction before using the proven polar-frame inverse transform.
    double p=wrap180(a.axis2Deg);
    double az=wrap180(a.axis1Deg);
    if(p<0.0){p=-p;az=wrap180(az-180.0);}
    p=std::clamp(p,0.0,180.0);
    const double mountAltitude=90.0-p;
    return skyFromPolarAlignedAxes({wrap360(az),mountAltitude,true},observer,utc);
}

// Native direct-MC GEM model v5 (OpenAstroLink 0.2.10.44).
//
// Mechanical Home is defined as Axis1=0, Axis2=0 with the counterweight
// shaft down and the telescope/DEC axis aimed along the local celestial pole.
// EQMOD's own standard Home/Park convention reports RA=LST+6h at Dec=+90°,
// therefore Axis1=0 corresponds to hour angle H=-90°.  Away from the pole:
//     Axis1 = H + 90°
//     Axis2 = signed polar distance
// This phase is fixed by the physical counterweight-down pose, so no near-pole
// manual Sync is required at startup.
//
// A GEM has two equivalent mechanical representations of the same sky vector:
//   branch A: (H,       +P)
//   branch B: (H+180°,  -P)
// where H=LST-RA and P=90°-DEC in the northern hemisphere (mirrored around
// the south celestial pole in the southern hemisphere).  axesForSky() chooses
// the branch requiring the shortest physical motion from the current pose.
MechanicalAxes gemHomeZeroAxesForSky(const EquatorialCoord &jnow,
                                      const ObserverLocation &observer,
                                      const QDateTime &utc){
    const double poleSign=observer.latitudeDeg>=0.0?1.0:-1.0;
    const double lst=localSiderealTimeDeg(utc,observer.longitudeDeg);
    const double hourAngle=wrap180(lst-jnow.raDeg);
    const double polarDistance=std::clamp(90.0-poleSign*jnow.decDeg,0.0,180.0);
    // Standard counterweight-down Home phase: H=-90° maps to Axis1=0°.
    return {wrap180(hourAngle+90.0),polarDistance,true};
}

EquatorialCoord skyFromGemHomeZeroAxes(MechanicalAxes a,
                                        const ObserverLocation &observer,
                                        const QDateTime &utc){
    // Normalize an arbitrary unwrapped controller representation to one of the
    // two equivalent GEM branches.  A negative Axis2 means the flipped branch.
    double p=wrap180(a.axis2Deg);
    double axis1=wrap180(a.axis1Deg);
    if(p<0.0){p=-p;axis1=wrap180(axis1-180.0);}
    p=std::clamp(p,0.0,180.0);
    const double hourAngle=wrap180(axis1-90.0);
    const double poleSign=observer.latitudeDeg>=0.0?1.0:-1.0;
    const double dec=std::clamp(poleSign*(90.0-p),-90.0,90.0);
    const double ra=wrap360(localSiderealTimeDeg(utc,observer.longitudeDeg)-hourAngle);
    return {ra,dec,EquatorialFrame::JNow};
}

// Legacy v1 direct HA/DEC mapping, retained for non-migrated/raw backends.
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

void MountGeometryModel::configure(MountGeometryConfig c,ObserverLocation o){
    // Preserve a valid alignment when only operational/safety settings change
    // (Park/Home coordinates, GOTO envelope, auto-home preference, pier-flip
    // policy).  Only parameters that alter the encoder<->sky transform must
    // invalidate Sync.
    const bool transformUnchanged =
        c.type==config_.type && c.axis1Sign==config_.axis1Sign && c.axis2Sign==config_.axis2Sign &&
        c.nativeCoordinateModelVersion==config_.nativeCoordinateModelVersion &&
        c.preferredPierSide.compare(config_.preferredPierSide,Qt::CaseInsensitive)==0 &&
        std::abs(o.latitudeDeg-observer_.latitudeDeg)<1e-10 &&
        std::abs(o.longitudeDeg-observer_.longitudeDeg)<1e-10;
    config_=std::move(c);observer_=o;
    if(!transformUnchanged){synced_=false;pierSide_="unknown";}
}

MechanicalAxes MountGeometryModel::canonicalAxesForSky(const EquatorialCoord &skyJNow,const QDateTime&utc,QString*pier,QString*error)const{
    const double lst=localSiderealTimeDeg(utc,observer_.longitudeDeg);
    switch(config_.type){
    case MountGeometryType::GermanEquatorial:{
        QString side=config_.preferredPierSide.toLower();if(side!="east"&&side!="west")side="east";if(pier)*pier=side;
        if(config_.nativeCoordinateModelVersion>=6)
            return gemTelescopeFrameAxesForSky(skyJNow,observer_,utc);
        if(config_.nativeCoordinateModelVersion>=4)
            return gemHomeZeroAxesForSky(skyJNow,observer_,utc);
        if(config_.nativeCoordinateModelVersion>=2)
            return polarAlignedAxesForSky(skyJNow,observer_,utc);
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
    case MountGeometryType::GermanEquatorial:
        if(config_.nativeCoordinateModelVersion>=6)return skyFromGemTelescopeFrameAxes(a,observer_,utc);
        if(config_.nativeCoordinateModelVersion>=4)return skyFromGemHomeZeroAxes(a,observer_,utc);
        if(config_.nativeCoordinateModelVersion>=2)return skyFromPolarAlignedAxes(a,observer_,utc);
        return equatorialFromAxes(a,lst,true,pierSide_);
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

bool MountGeometryModel::syncHome(const MechanicalAxes&actual,const QDateTime&utc,QString*error){
    if(!actual.valid){if(error)*error="Mechanical axis position is not available";return false;}
    if(config_.type!=MountGeometryType::GermanEquatorial &&
       config_.type!=MountGeometryType::ForkEquatorial &&
       config_.type!=MountGeometryType::EquatorialPlatform){
        if(error)*error="Automatic Home alignment is currently defined only for equatorial mount geometries";
        return false;
    }
    const double poleDec=observer_.latitudeDeg>=0.0?90.0:-90.0;
    const double lst=localSiderealTimeDeg(utc,observer_.longitudeDeg);
    const EquatorialCoord poleOnMeridian{lst,poleDec,EquatorialFrame::JNow};
    QString side=config_.preferredPierSide.toLower();
    if(config_.type!=MountGeometryType::GermanEquatorial)side="none";
    else if(side!="east"&&side!="west")side="east";
    MechanicalAxes canonical;
    if(config_.type==MountGeometryType::GermanEquatorial&&config_.nativeCoordinateModelVersion>=4){
        // v4 uses one mechanical-axis system directly: standard Home is 0,0.
        // RA at the celestial pole is singular, so Home defines the hour-axis
        // phase explicitly rather than deriving it from a pole vector.
        canonical={0.0,0.0,true};
    }else if(config_.type==MountGeometryType::GermanEquatorial&&config_.nativeCoordinateModelVersion>=2){
        canonical={90.0,poleDec,true};
    }else if(config_.type==MountGeometryType::GermanEquatorial)canonical=equatorialAxes(poleOnMeridian,lst,true,side);
    else canonical=equatorialAxes(poleOnMeridian,lst,false,{});
    if(!canonical.valid){if(error)*error="Could not construct canonical equatorial Home reference";return false;}
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

    if(config_.type==MountGeometryType::GermanEquatorial&&config_.nativeCoordinateModelVersion>=4){
        // Evaluate both physically equivalent GEM branches and choose the one
        // with the shortest controller motion from the CURRENT pose.  This is
        // what prevents a target a few degrees from the pole from taking an
        // unnecessary ~150° hour-axis route.
        const MechanicalAxes candidates[2]={
            canonical,
            {wrap180(canonical.axis1Deg+180.0),-canonical.axis2Deg,true}
        };
        bool have=false;double bestCost=1e100;MechanicalAxes best;
        for(const auto &candidate:candidates){
            const double d1=signedNearest(candidate.axis1Deg-syncCanonical_.axis1Deg);
            const double d2=candidate.axis2Deg-syncCanonical_.axis2Deg;
            MechanicalAxes t{
                syncActual_.axis1Deg+(config_.axis1Sign>=0?1.0:-1.0)*d1,
                syncActual_.axis2Deg+(config_.axis2Sign>=0?1.0:-1.0)*d2,true};
            t.axis1Deg=current.axis1Deg+signedNearest(t.axis1Deg-current.axis1Deg);
            t.axis2Deg=current.axis2Deg+signedNearest(t.axis2Deg-current.axis2Deg);
            const double move1=std::abs(t.axis1Deg-current.axis1Deg);
            const double move2=std::abs(t.axis2Deg-current.axis2Deg);
            const double cost=std::max(move1,move2)+0.05*(move1+move2);
            if(!have||cost<bestCost){have=true;bestCost=cost;best=t;}
        }
        if(!have){if(error)*error="Could not construct a valid GEM branch";return false;}
        target=best;return true;
    }

    // Keep the current pier branch until a qualified meridian-flip planner is enabled.
    if(config_.type==MountGeometryType::GermanEquatorial&&config_.nativeCoordinateModelVersion<2&&side!=pierSide_)
        canonical=equatorialAxes(jnow,localSiderealTimeDeg(utc,observer_.longitudeDeg),true,pierSide_);
    const double d1=signedNearest(canonical.axis1Deg-syncCanonical_.axis1Deg);
    double d2=canonical.axis2Deg-syncCanonical_.axis2Deg;
    if(config_.type==MountGeometryType::AltAzimuth||config_.type==MountGeometryType::AltAzimuthDerotator||config_.type==MountGeometryType::CustomTwoAxis)d2=signedNearest(d2);
    target={syncActual_.axis1Deg+(config_.axis1Sign>=0?1.0:-1.0)*d1,
            syncActual_.axis2Deg+(config_.axis2Sign>=0?1.0:-1.0)*d2,true};
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
