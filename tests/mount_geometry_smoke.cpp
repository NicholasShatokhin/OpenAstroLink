#include "core/mount_geometry.h"
#include "core/equatorial_frames.h"
#include <QDateTime>
#include <cmath>
#include <iostream>

using namespace oas;

namespace {
double wrap180(double v){while(v>180.0)v-=360.0;while(v<-180.0)v+=360.0;return v;}
bool near(double a,double b,double eps=1e-4){return std::abs(a-b)<=eps;}
int fail(const char *m){std::cerr<<m<<'\n';return 1;}
}

int main(){
    const auto utc=QDateTime::fromString("2026-08-28T18:00:00Z",Qt::ISODate);
    ObserverLocation site{50.45,30.52,180.0};
    MountGeometryConfig gem;
    gem.type=MountGeometryType::GermanEquatorial;
    gem.axis1Sign=1;gem.axis2Sign=1;gem.preferredPierSide="east";
    gem.parkAxis1Deg=90.0;gem.parkAxis2Deg=0.0;
    MountGeometryModel m(gem,site);
    const MechanicalAxes actual0{90.0,0.0,true};
    const EquatorialCoord syncSky{90.0,0.0,EquatorialFrame::J2000};
    QString err;
    if(!m.sync(syncSky,actual0,utc,&err))return fail("GEM sync failed");

    MechanicalAxes raPlus;
    if(!m.axesForSky({91.0,0.0,EquatorialFrame::J2000},actual0,raPlus,utc,&err))return fail("GEM RA+ transform failed");
    // For an east-branch GEM at fixed UTC, increasing celestial RA reduces hour angle.
    if(!(raPlus.axis1Deg<actual0.axis1Deg))return fail("GEM RA+ should reduce mechanical axis1 for +1 sign");

    MechanicalAxes decPlus;
    if(!m.axesForSky({90.0,1.0,EquatorialFrame::J2000},actual0,decPlus,utc,&err))return fail("GEM DEC+ transform failed");
    if(!(decPlus.axis2Deg>actual0.axis2Deg))return fail("GEM DEC+ should increase mechanical axis2 for +1 sign");

    EquatorialCoord back;
    if(!m.skyFromAxes(raPlus,back,utc,&err))return fail("GEM reverse transform failed");
    if(std::abs(wrap180(back.raDeg-91.0))>2e-3 || std::abs(back.decDeg)>2e-3)return fail("GEM round-trip mismatch");
    const auto park=m.parkAxes();if(!near(park.axis1Deg,90.0)||!near(park.axis2Deg,0.0))return fail("Mechanical park mismatch");

    MountGeometryConfig alt;alt.type=MountGeometryType::AltAzimuth;
    MountGeometryModel a(alt,site);
    // A sync absorbs the installation encoder-zero offset, so arbitrary actual axes are valid.
    const EquatorialCoord star{279.2347,38.7837,EquatorialFrame::J2000};
    const MechanicalAxes altActual{123.0,45.0,true};
    if(!a.sync(star,altActual,utc,&err))return fail("AltAz sync failed");
    EquatorialCoord altBack;
    if(!a.skyFromAxes(altActual,altBack,utc,&err))return fail("AltAz reverse transform failed");
    if(std::abs(wrap180(altBack.raDeg-star.raDeg))>2e-3 || std::abs(altBack.decDeg-star.decDeg)>2e-3)return fail("AltAz round-trip mismatch");

    if(mountGeometryTypeFromString("fork-equatorial")!=MountGeometryType::ForkEquatorial)return fail("Geometry parser failed");
    std::cout<<"mount geometry smoke PASS\n";
    return 0;
}
