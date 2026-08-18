#include "algorithms/motion_estimator.h"
#include <opencv2/calib3d.hpp>
#include <cmath>

namespace oas {
FrameMotion MotionEstimator::estimate(const std::vector<DetectedStar>&a,const std::vector<DetectedStar>&b,double maxd)const{FrameMotion r;if(a.size()<3||b.size()<3)return r;std::vector<cv::Point2f> p,q;std::vector<bool> used(b.size(),false);for(const auto&s:a){double bd=maxd;int bi=-1;for(int i=0;i<int(b.size());++i)if(!used[i]){double d=cv::norm(s.positionPx-b[i].positionPx);if(d<bd){bd=d;bi=i;}}if(bi>=0){p.emplace_back(s.positionPx);q.emplace_back(b[bi].positionPx);used[bi]=true;}}if(p.size()<3)return r;cv::Mat in;cv::Mat A=cv::estimateAffinePartial2D(p,q,in,cv::RANSAC,3.0,2000,0.99,10);if(A.empty())return r;double a00=A.at<double>(0,0),a10=A.at<double>(1,0);r.valid=true;r.dxPx=A.at<double>(0,2);r.dyPx=A.at<double>(1,2);r.scale=std::hypot(a00,a10);r.rotationDeg=std::atan2(a10,a00)*180.0/3.141592653589793;for(int i=0;i<in.rows;++i)r.inliers+=in.at<uchar>(i)!=0;return r;}
}
