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

    // A repeatable mechanical Home can restore an equatorial model without a
    // manual near-pole Sync.  Operational-only changes must preserve it.
    MountGeometryConfig homeCfg=gem;homeCfg.axis1Sign=-1;homeCfg.customHome=true;homeCfg.autoHomeSync=true;homeCfg.homeAxis1Deg=12.5;homeCfg.homeAxis2Deg=-3.0;
    MountGeometryModel home(homeCfg,site);const MechanicalAxes homeActual{12.5,-3.0,true};
    if(!home.syncHome(homeActual,utc,&err))return fail("GEM automatic Home sync failed");
    EquatorialCoord homeSky;if(!home.skyFromAxes(homeActual,homeSky,utc,&err))return fail("Home reverse transform failed");
    if(std::abs(std::abs(homeSky.decDeg)-90.0)>2e-3)return fail("Home must map to the local celestial pole");
    auto operational=homeCfg;operational.maxGotoAxisDeltaDeg=90.0;operational.parkAxis1Deg=1.0;home.configure(operational,site);
    if(!home.synced())return fail("Operational mount profile change incorrectly invalidated Sync");
    auto transformChange=operational;transformChange.axis1Sign=1;home.configure(transformChange,site);
    if(home.synced())return fail("Axis-sign change must invalidate Sync");


    // v7 direct Motor Controller GEM model: standard EQMOD mechanical Home is
    // Axis1=0, Axis2=0 with counterweights down, OTA on the north celestial
    // pole, i.e. sky HA=-6h and Dec=+90deg.  Unlike v6, GEM pointing states
    // are physical branches and must not be replaced by whichever spherical
    // representation happens to have the shortest motor delta.
    MountGeometryConfig eqv7;eqv7.type=MountGeometryType::GermanEquatorial;eqv7.axis1Sign=1;eqv7.axis2Sign=1;
    eqv7.nativeCoordinateModelVersion=7;eqv7.autoHomeSync=true;
    ObserverLocation hilSite{54.476389,30.496667,200.0};
    const MechanicalAxes mechanicalHome{0.0,0.0,true};
    const auto moonUtc=QDateTime::fromString("2026-08-31T21:39:31.666Z",Qt::ISODate);
    MountGeometryModel v7(eqv7,hilSite);
    if(!v7.syncHome(mechanicalHome,moonUtc,&err))return fail("EQDrive v7 Home sync failed");
    if(v7.pierSide()!="west")return fail("EQDrive v7 northern Home must use EQMOD west branch");
    EquatorialCoord homeJ2000;if(!v7.skyFromAxes(mechanicalHome,homeJ2000,moonUtc,&err))return fail("EQDrive v7 Home sky transform failed");
    const auto homeJNow=convertEquatorialFrame(homeJ2000,EquatorialFrame::JNow,moonUtc);
    if(std::abs(homeJNow.decDeg-90.0)>2e-3)return fail("EQDrive v7 Home must point at north celestial pole");

    // 2026-08-31 real HIL: OAL v6 sent this Moon target to about
    // (-45.69,-77.30), while EQMOD/ASCOM pointed correctly on pier=west.
    // Standard EQMOD HA/Dec mechanics require (+44.3077,+77.2996).
    const EquatorialCoord moonJNow{21.147893,12.700359,EquatorialFrame::JNow};MechanicalAxes moonAxes;
    if(!v7.axesForSky(moonJNow,mechanicalHome,moonAxes,moonUtc,&err))return fail("EQDrive v7 Moon HIL transform failed");
    if(std::abs(moonAxes.axis1Deg-44.307735)>0.02||std::abs(moonAxes.axis2Deg-77.299641)>0.02)return fail("EQDrive v7 Moon target does not match EQMOD mechanical branch");
    EquatorialCoord moonBack;if(!v7.skyFromAxes(moonAxes,moonBack,moonUtc,&err))return fail("EQDrive v7 Moon reverse transform failed");
    const auto moonBackJNow=convertEquatorialFrame(moonBack,EquatorialFrame::JNow,moonUtc);
    if(std::abs(wrap180(moonBackJNow.raDeg-moonJNow.raDeg))>0.01||std::abs(moonBackJNow.decDeg-moonJNow.decDeg)>0.01)return fail("EQDrive v7 Moon round-trip mismatch");

    // Western-hour-angle target must use the opposite physical GEM branch.
    // This verifies that v7 changes pointing state at the meridian instead of
    // free-form shortest-branch selection.
    const double lst=localSiderealTimeDeg(moonUtc,hilSite.longitudeDeg);
    const EquatorialCoord westSky{std::fmod(lst-30.0+360.0,360.0),40.0,EquatorialFrame::JNow};MechanicalAxes westAxes;
    if(!v7.axesForSky(westSky,moonAxes,westAxes,moonUtc,&err))return fail("EQDrive v7 west-sky transform failed");
    if(!(westAxes.axis2Deg<0.0))return fail("EQDrive v7 positive HA must select opposite GEM pointing state");
    EquatorialCoord westBack;if(!v7.skyFromAxes(westAxes,westBack,moonUtc,&err))return fail("EQDrive v7 west-sky reverse transform failed");
    const auto westBackJNow=convertEquatorialFrame(westBack,EquatorialFrame::JNow,moonUtc);
    if(std::abs(wrap180(westBackJNow.raDeg-westSky.raDeg))>0.01||std::abs(westBackJNow.decDeg-westSky.decDeg)>0.01)return fail("EQDrive v7 west-sky round-trip mismatch");

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
