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


    // v6 direct Motor Controller GEM model: startup controller counts define
    // physical Home/Park as 0,0, while sky mapping follows the polar-aligned
    // telescope direction-vector transform used by mature SkyWatcher drivers.
    MountGeometryConfig eqv6;eqv6.type=MountGeometryType::GermanEquatorial;eqv6.axis1Sign=1;eqv6.axis2Sign=1;
    eqv6.nativeCoordinateModelVersion=6;eqv6.autoHomeSync=true;
    ObserverLocation kyiv{50.476481,30.496884,0.0};
    const auto hilUtc=QDateTime::fromString("2026-08-31T13:54:49.448Z",Qt::ISODate);
    MountGeometryModel v6(eqv6,kyiv);const MechanicalAxes mechanicalHome{0.0,0.0,true};
    if(!v6.syncHome(mechanicalHome,hilUtc,&err))return fail("EQDrive v6 Home sync failed");
    EquatorialCoord homeJ2000;if(!v6.skyFromAxes(mechanicalHome,homeJ2000,hilUtc,&err))return fail("EQDrive v6 Home sky transform failed");
    const auto homeJNow=convertEquatorialFrame(homeJ2000,EquatorialFrame::JNow,hilUtc);
    if(std::abs(homeJNow.decDeg-90.0)>2e-3)return fail("EQDrive v6 Home must point at north celestial pole");

    // Regression from the 2026-08-31 HIL.  The target was only ~17deg from
    // the pole.  The polar telescope-frame model produces the same sky vector
    // on two GEM branches and must choose the shorter branch from Home.
    const EquatorialCoord hilTarget{275.285624,72.735277,EquatorialFrame::J2000};MechanicalAxes hilAxes;
    if(!v6.axesForSky(hilTarget,mechanicalHome,hilAxes,hilUtc,&err))return fail("EQDrive v6 HIL target transform failed");
    if(std::abs(hilAxes.axis1Deg+51.9348)>0.25||std::abs(hilAxes.axis2Deg+15.9484)>0.25)return fail("EQDrive v6 polar-frame target mismatch");
    EquatorialCoord hilBack;if(!v6.skyFromAxes(hilAxes,hilBack,hilUtc,&err))return fail("EQDrive v6 reverse transform failed");
    if(std::abs(wrap180(hilBack.raDeg-hilTarget.raDeg))>0.01||std::abs(hilBack.decDeg-hilTarget.decDeg)>0.01)return fail("EQDrive v6 HIL round-trip mismatch");

    // Latest HIL regression: from startup Home the first target was near
    // Az=15deg/Alt=64deg, then Deneb region near Az=92deg/Alt=62.6deg.  v5
    // sent the second move along the wrong hour-axis phase.  v6 must map the
    // first target to the nearby flipped branch and the second target to a
    // small continuation on that same physical branch.
    ObserverLocation hil2Site{54.476389,30.496667,200.0};
    MountGeometryModel v6Hil2(eqv6,hil2Site);const auto hil2HomeUtc=QDateTime::fromString("2026-08-31T17:16:01.739Z",Qt::ISODate);
    if(!v6Hil2.syncHome(mechanicalHome,hil2HomeUtc,&err))return fail("EQDrive v6 HIL2 Home sync failed");
    const EquatorialCoord firstTargetJNow{302.005296,77.793909,EquatorialFrame::JNow};MechanicalAxes firstAxes;
    if(!v6Hil2.axesForSky(firstTargetJNow,mechanicalHome,firstAxes,hil2HomeUtc,&err))return fail("EQDrive v6 first east-side HIL target transform failed");
    if(std::abs(firstAxes.axis1Deg+32.6047)>0.25||std::abs(firstAxes.axis2Deg+12.2061)>0.25)return fail("EQDrive v6 first HIL target chose wrong GEM branch");
    const auto hil2Utc=QDateTime::fromString("2026-08-31T17:17:01.469Z",Qt::ISODate);
    const EquatorialCoord hil2JNow{310.592084,45.379860,EquatorialFrame::JNow};MechanicalAxes hil2Axes;
    if(!v6Hil2.axesForSky(hil2JNow,firstAxes,hil2Axes,hil2Utc,&err))return fail("EQDrive v6 Deneb-region HIL target transform failed");
    if(std::abs(hil2Axes.axis1Deg+40.9420)>0.25||std::abs(hil2Axes.axis2Deg+44.6201)>0.25)return fail("EQDrive v6 Deneb-region target chose wrong polar-frame branch");
    if(std::abs(hil2Axes.axis1Deg-firstAxes.axis1Deg)>15.0||std::abs(hil2Axes.axis2Deg-firstAxes.axis2Deg)>40.0)return fail("EQDrive v6 Deneb-region continuation is not the short physical route");
    EquatorialCoord hil2Back;if(!v6Hil2.skyFromAxes(hil2Axes,hil2Back,hil2Utc,&err))return fail("EQDrive v6 HIL2 reverse transform failed");
    const auto hil2BackJNow=convertEquatorialFrame(hil2Back,EquatorialFrame::JNow,hil2Utc);
    if(std::abs(wrap180(hil2BackJNow.raDeg-hil2JNow.raDeg))>0.01||std::abs(hil2BackJNow.decDeg-hil2JNow.decDeg)>0.01)return fail("EQDrive v6 HIL2 round-trip mismatch");

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
