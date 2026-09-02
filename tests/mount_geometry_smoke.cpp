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


    // v9 direct Motor Controller GEM model: v7 EQMOD HA/Dec branch geometry
    // is retained. The 2026-09-02 follow-up HIL measured an exact east/west
    // sky mirror after the Axis1 hypothesis, so all four physical sign mappings
    // were recomputed from the recorded raw controller deltas. The only mapping
    // that sends the recorded target to its ASCOM-correct Az/Alt is
    // Axis1=+1, Axis2=-1. Home is still raw 0,0.
    MountGeometryConfig eqv9;eqv9.type=MountGeometryType::GermanEquatorial;eqv9.axis1Sign=1;eqv9.axis2Sign=-1;
    eqv9.nativeCoordinateModelVersion=9;eqv9.autoHomeSync=true;
    ObserverLocation hilSite{54.476389,30.496667,200.0};
    const MechanicalAxes mechanicalHome{0.0,0.0,true};
    const auto moonUtc=QDateTime::fromString("2026-08-31T21:39:31.666Z",Qt::ISODate);
    MountGeometryModel v9(eqv9,hilSite);
    if(!v9.syncHome(mechanicalHome,moonUtc,&err))return fail("EQDrive v9 Home sync failed");
    if(v9.pierSide()!="west")return fail("EQDrive v9 northern Home must use EQMOD west branch");
    EquatorialCoord homeJ2000;if(!v9.skyFromAxes(mechanicalHome,homeJ2000,moonUtc,&err))return fail("EQDrive v9 Home sky transform failed");
    const auto homeJNow=convertEquatorialFrame(homeJ2000,EquatorialFrame::JNow,moonUtc);
    if(std::abs(homeJNow.decDeg-90.0)>2e-3)return fail("EQDrive v9 Home must point at north celestial pole");

    // Geometry sanity check on the earlier Moon target. With v9 physical signs,
    // canonical (+44.3077,+77.2996) maps to raw (+44.3077,-77.2996).
    const EquatorialCoord moonJNow{21.147893,12.700359,EquatorialFrame::JNow};MechanicalAxes moonAxes;
    if(!v9.axesForSky(moonJNow,mechanicalHome,moonAxes,moonUtc,&err))return fail("EQDrive v9 Moon transform failed");
    if(std::abs(moonAxes.axis1Deg-44.307735)>0.02||std::abs(moonAxes.axis2Deg+77.299641)>0.02)return fail("EQDrive v9 Moon physical-axis mapping mismatch");
    EquatorialCoord moonBack;if(!v9.skyFromAxes(moonAxes,moonBack,moonUtc,&err))return fail("EQDrive v9 Moon reverse transform failed");
    const auto moonBackJNow=convertEquatorialFrame(moonBack,EquatorialFrame::JNow,moonUtc);
    if(std::abs(wrap180(moonBackJNow.raDeg-moonJNow.raDeg))>0.01||std::abs(moonBackJNow.decDeg-moonJNow.decDeg)>0.01)return fail("EQDrive v9 Moon round-trip mismatch");

    // Exact follow-up HIL from 2026-09-02 08:33:47 UTC. The requested JNow
    // target was RA=61.569978, Dec=22.152640 at LST~=140.453559 deg, so
    // HA~=+78.883581 deg and the EQMOD east pointing state is canonical
    // Axis1~= -11.116419, Axis2~= -67.847360. The qualified physical mapping
    // must command raw Axis1~= -11.116419, Axis2~= +67.847360.
    const auto mirrorUtc=QDateTime::fromString("2026-09-02T08:33:47.136Z",Qt::ISODate);
    MountGeometryModel mirrorV9(eqv9,hilSite);
    if(!mirrorV9.syncHome(mechanicalHome,mirrorUtc,&err))return fail("EQDrive v9 follow-up HIL Home sync failed");
    const EquatorialCoord westTargetJNow{61.569978,22.152640,EquatorialFrame::JNow};MechanicalAxes westTargetAxes;
    if(!mirrorV9.axesForSky(westTargetJNow,mechanicalHome,westTargetAxes,mirrorUtc,&err))return fail("EQDrive v9 follow-up HIL transform failed");
    if(mirrorV9.pierSide()!="east")return fail("EQDrive v9 west-sky target must select EQMOD east pointing state");
    if(std::abs(westTargetAxes.axis1Deg+11.116419)>0.04||std::abs(westTargetAxes.axis2Deg-67.847360)>0.04)return fail("EQDrive v9 follow-up HIL does not command the non-mirrored raw axes");
    EquatorialCoord westTargetBack;if(!mirrorV9.skyFromAxes(westTargetAxes,westTargetBack,mirrorUtc,&err))return fail("EQDrive v9 follow-up HIL reverse transform failed");
    const auto westTargetBackJNow=convertEquatorialFrame(westTargetBack,EquatorialFrame::JNow,mirrorUtc);
    if(std::abs(wrap180(westTargetBackJNow.raDeg-westTargetJNow.raDeg))>0.01||std::abs(westTargetBackJNow.decDeg-westTargetJNow.decDeg)>0.01)return fail("EQDrive v9 follow-up HIL round-trip mismatch");

    // Generic positive-HA coverage: the canonical east pointing state remains
    // selected; v9 changes only the controller-to-canonical physical signs.
    const double lst=localSiderealTimeDeg(moonUtc,hilSite.longitudeDeg);
    const EquatorialCoord westSky{std::fmod(lst-30.0+360.0,360.0),40.0,EquatorialFrame::JNow};MechanicalAxes westAxes;
    if(!v9.axesForSky(westSky,moonAxes,westAxes,moonUtc,&err))return fail("EQDrive v9 west-sky transform failed");
    if(!(westAxes.axis2Deg>0.0))return fail("EQDrive v9 positive HA must command positive raw Axis2 with Axis2Sign=-1");
    EquatorialCoord westBack;if(!v9.skyFromAxes(westAxes,westBack,moonUtc,&err))return fail("EQDrive v9 west-sky reverse transform failed");
    const auto westBackJNow=convertEquatorialFrame(westBack,EquatorialFrame::JNow,moonUtc);
    if(std::abs(wrap180(westBackJNow.raDeg-westSky.raDeg))>0.01||std::abs(westBackJNow.decDeg-westSky.decDeg)>0.01)return fail("EQDrive v9 west-sky round-trip mismatch");

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
