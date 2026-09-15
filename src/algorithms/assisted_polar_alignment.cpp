#include "algorithms/assisted_polar_alignment.h"
#include "core/equatorial_frames.h"
#include <opencv2/core.hpp>
#include <algorithm>
#include <cmath>

namespace oas { namespace {
constexpr double D=3.14159265358979323846/180.0,R=180.0/3.14159265358979323846;
double wrap360(double x){x=std::fmod(x,360.0);if(x<0)x+=360.0;return x;}
double wrap180(double x){x=std::fmod(x+180.0,360.0);if(x<0)x+=360.0;return x-180.0;}
cv::Vec3d horizontalUnit(const HorizontalCoord &h){const double a=h.azDeg*D,e=h.altDeg*D,c=std::cos(e);return {c*std::sin(a),c*std::cos(a),std::sin(e)};}
HorizontalCoord unitHorizontal(const cv::Vec3d &v){const double n=cv::norm(v);if(n<1e-12)return{};const cv::Vec3d u=v*(1.0/n);return {wrap360(std::atan2(u[0],u[1])*R),std::asin(std::clamp(u[2],-1.0,1.0))*R};}
double angleDeg(const cv::Vec3d&a,const cv::Vec3d&b){const double na=cv::norm(a),nb=cv::norm(b);if(na<1e-12||nb<1e-12)return 0;return std::acos(std::clamp(a.dot(b)/(na*nb),-1.0,1.0))*R;}
}
AssistedPolarResult AssistedPolarAlignmentEstimator::estimate(const std::vector<AssistedPolarSample>&samples,const ObserverLocation&observer)const{
    AssistedPolarResult out;out.sampleCount=int(samples.size());
    if(samples.size()<2){out.message="At least two centred targets are required; three or more separated targets are recommended";return out;}
    cv::Matx33d H=cv::Matx33d::zeros();std::vector<cv::Vec3d> mountVec,targetVec;mountVec.reserve(samples.size());targetVec.reserve(samples.size());
    for(const auto&s:samples){const QDateTime utc=s.utc.isValid()?s.utc:QDateTime::currentDateTimeUtc();const auto mh=equatorialToHorizontal(s.mountReported,observer,utc),th=equatorialToHorizontal(s.target,observer,utc);auto m=horizontalUnit(mh),t=horizontalUnit(th);mountVec.push_back(m);targetVec.push_back(t);for(int r=0;r<3;++r)for(int c=0;c<3;++c)H(r,c)+=t[r]*m[c];}
    cv::SVD svd(cv::Mat(H),cv::SVD::FULL_UV);cv::Mat Rm=svd.u*svd.vt;if(cv::determinant(Rm)<0){cv::Mat U=svd.u.clone();U.col(2)*=-1;Rm=U*svd.vt;}
    cv::Matx33d rot;for(int r=0;r<3;++r)for(int c=0;c<3;++c)rot(r,c)=Rm.at<double>(r,c);
    double rss=0,span=0;for(size_t i=0;i<samples.size();++i){rss+=std::pow(angleDeg(rot*mountVec[i],targetVec[i])*60.0,2);for(size_t j=i+1;j<samples.size();++j)span=std::max(span,angleDeg(targetVec[i],targetVec[j]));}
    const double idealAz=observer.latitudeDeg>=0?0.0:180.0,idealAlt=std::abs(observer.latitudeDeg);const cv::Vec3d ideal=horizontalUnit({idealAz,idealAlt});const HorizontalCoord axis=unitHorizontal(rot*ideal);
    out.axisAzDeg=axis.azDeg;out.axisAltDeg=axis.altDeg;out.idealAzDeg=idealAz;out.idealAltDeg=idealAlt;out.altitudeAdjustmentArcmin=(idealAlt-axis.altDeg)*60.0;out.azimuthAdjustmentArcmin=wrap180(idealAz-axis.azDeg)*60.0;out.totalErrorArcmin=angleDeg(horizontalUnit(axis),ideal)*60.0;out.rmsResidualArcmin=std::sqrt(rss/double(samples.size()));out.skySpanDeg=span;
    if(samples.size()>=3&&span>=15.0&&out.rmsResidualArcmin<=5.0)out.confidence="good";else if(samples.size()>=3&&span>=5.0&&out.rmsResidualArcmin<=15.0)out.confidence="limited";else out.confidence="poor";
    out.success=true;out.message=QString("Adjust mount altitude by %1 arcmin and azimuth by %2 arcmin; confidence=%3, fit RMS=%4 arcmin, sample span=%5 deg").arg(out.altitudeAdjustmentArcmin,0,'f',2).arg(out.azimuthAdjustmentArcmin,0,'f',2).arg(out.confidence).arg(out.rmsResidualArcmin,0,'f',2).arg(out.skySpanDeg,0,'f',1);return out;
}
}
