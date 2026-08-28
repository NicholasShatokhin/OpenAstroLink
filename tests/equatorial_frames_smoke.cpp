#include "core/equatorial_frames.h"
#include <QCoreApplication>
#include <QDateTime>
#include <cmath>
#include <iostream>

namespace {
double wrap180(double x){while(x>180.0)x-=360.0;while(x<-180.0)x+=360.0;return x;}
bool near(double a,double b,double eps){return std::abs(a-b)<=eps;}
}

int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    using namespace oas;
    const QDateTime date(QDate(2026,8,28),QTime(0,0),Qt::UTC);
    const EquatorialCoord vega{279.23473479,38.78368896,EquatorialFrame::J2000};
    const auto now=convertEquatorialFrame(vega,EquatorialFrame::JNow,date);
    // Sanity values from the same standard precession equations; broad enough
    // to catch sign/order mistakes without turning this into an ephemeris test.
    if(!near(now.raDeg,279.458,0.02)||!near(now.decDeg,38.808,0.02)){
        std::cerr<<"unexpected J2000->JNow precession: "<<now.raDeg<<" "<<now.decDeg<<"\n";return 1;
    }
    const auto back=convertEquatorialFrame(now,EquatorialFrame::J2000,date);
    if(std::abs(wrap180(back.raDeg-vega.raDeg))>1e-8||std::abs(back.decDeg-vega.decDeg)>1e-8){
        std::cerr<<"J2000 round-trip failed\n";return 2;
    }
    if(equatorialFrameFromString("ICRS")!=EquatorialFrame::J2000||
       equatorialFrameFromString("of-date")!=EquatorialFrame::JNow){
        std::cerr<<"frame parser failed\n";return 3;
    }
    return 0;
}
