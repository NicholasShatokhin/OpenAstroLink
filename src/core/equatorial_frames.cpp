#include "core/equatorial_frames.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace oas { namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDeg = kPi / 180.0;
constexpr double kArcsec = kDeg / 3600.0;

double wrap360(double x) {
    x = std::fmod(x, 360.0);
    if (x < 0.0) x += 360.0;
    return x;
}

double julianDateUtc(const QDateTime &utc) {
    const QDateTime u = utc.toUTC();
    return 2440587.5 + double(u.toMSecsSinceEpoch()) / 86400000.0;
}

using Mat3 = std::array<std::array<double,3>,3>;
using Vec3 = std::array<double,3>;

Mat3 mul(const Mat3&a,const Mat3&b){
    Mat3 r{};
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)r[i][j]+=a[i][k]*b[k][j];
    return r;
}
Mat3 transpose(const Mat3&m){Mat3 r{};for(int i=0;i<3;++i)for(int j=0;j<3;++j)r[i][j]=m[j][i];return r;}
Vec3 mul(const Mat3&m,const Vec3&v){Vec3 r{};for(int i=0;i<3;++i)for(int k=0;k<3;++k)r[i]+=m[i][k]*v[k];return r;}
Mat3 rz(double a){const double c=std::cos(a),s=std::sin(a);return {{{c,-s,0},{s,c,0},{0,0,1}}};}
Mat3 ry(double a){const double c=std::cos(a),s=std::sin(a);return {{{c,0,s},{0,1,0},{-s,0,c}}};}

// J2000 -> mean equator/equinox of date. Matrix form equivalent to the Meeus
// zeta/z/theta formula used for stellar precession from epoch J2000.0.
Mat3 j2000ToDateMatrix(const QDateTime&utc){
    const double T=(julianDateUtc(utc)-2451545.0)/36525.0;
    const double zeta=(2306.2181*T + 0.30188*T*T + 0.017998*T*T*T)*kArcsec;
    const double z   =(2306.2181*T + 1.09468*T*T + 0.018203*T*T*T)*kArcsec;
    const double th  =(2004.3109*T - 0.42665*T*T - 0.041833*T*T*T)*kArcsec;
    return mul(rz(z),mul(ry(-th),rz(zeta)));
}

Vec3 vectorFromCoord(const EquatorialCoord&c){
    const double a=c.raDeg*kDeg,d=c.decDeg*kDeg,cd=std::cos(d);
    return {cd*std::cos(a),cd*std::sin(a),std::sin(d)};
}
EquatorialCoord coordFromVector(const Vec3&v,EquatorialFrame frame){
    const double r=std::hypot(v[0],v[1]);
    EquatorialCoord c;
    c.raDeg=wrap360(std::atan2(v[1],v[0])/kDeg);
    c.decDeg=std::atan2(v[2],r)/kDeg;
    c.frame=frame;
    return c;
}
} // namespace

QString equatorialFrameName(EquatorialFrame frame){
    switch(frame){case EquatorialFrame::JNow:return "JNOW";default:return "J2000";}
}
EquatorialFrame equatorialFrameFromString(const QString&text,EquatorialFrame fallback){
    const QString s=text.trimmed().toUpper();
    if(s=="J2000"||s=="ICRS"||s=="FK5-J2000")return EquatorialFrame::J2000;
    if(s=="JNOW"||s=="OF-DATE"||s=="OFDATE"||s=="DATE")return EquatorialFrame::JNow;
    return fallback;
}
EquatorialCoord convertEquatorialFrame(const EquatorialCoord&coord,EquatorialFrame target,const QDateTime&utc){
    if(coord.frame==target){auto c=coord;c.raDeg=wrap360(c.raDeg);c.decDeg=std::clamp(c.decDeg,-90.0,90.0);return c;}
    const Mat3 f=j2000ToDateMatrix(utc);
    const Mat3 m=(coord.frame==EquatorialFrame::J2000&&target==EquatorialFrame::JNow)?f:transpose(f);
    return coordFromVector(mul(m,vectorFromCoord(coord)),target);
}

} // namespace oas
