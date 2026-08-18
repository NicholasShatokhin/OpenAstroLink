#include "algorithms/star_detector.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace oas {
static cv::Mat grayFloat(const cv::Mat &src){cv::Mat g;if(src.channels()==3)cv::cvtColor(src,g,cv::COLOR_BGR2GRAY);else if(src.channels()==4)cv::cvtColor(src,g,cv::COLOR_BGRA2GRAY);else g=src;cv::Mat f;double scale=src.depth()==CV_16U?1.0/65535.0:src.depth()==CV_8U?1.0/255.0:1.0;g.convertTo(f,CV_32F,scale);return f;}
std::vector<DetectedStar> StarDetector::detect(const cv::Mat &image) const {
    std::vector<DetectedStar> out;if(image.empty())return out;cv::Mat gray=grayFloat(image),smooth;cv::GaussianBlur(gray,smooth,{3,3},0.8);
    cv::Scalar mean,stddev;cv::meanStdDev(smooth,mean,stddev);const double bg=mean[0],thr=bg+options_.thresholdSigma*stddev[0];cv::Mat mask;smooth.convertTo(mask,CV_8U,255.0,-thr*255.0);cv::threshold(mask,mask,0,255,cv::THRESH_BINARY);
    cv::Mat labels,stats,cent;int n=cv::connectedComponentsWithStats(mask,labels,stats,cent,8,CV_32S);
    for(int i=1;i<n;++i){int area=stats.at<int>(i,cv::CC_STAT_AREA);if(area<options_.minAreaPx||area>options_.maxAreaPx)continue;cv::Rect r(stats.at<int>(i,cv::CC_STAT_LEFT),stats.at<int>(i,cv::CC_STAT_TOP),stats.at<int>(i,cv::CC_STAT_WIDTH),stats.at<int>(i,cv::CC_STAT_HEIGHT));double flux=0,peak=0,wx=0,wy=0;std::vector<std::pair<double,double>> radii;for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)if(labels.at<int>(y,x)==i){double v=std::max(0.0,double(smooth.at<float>(y,x))-bg);flux+=v;peak=std::max(peak,double(smooth.at<float>(y,x)));wx+=v*x;wy+=v*y;}if(flux<=1e-8)continue;double cx=wx/flux,cy=wy/flux;for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)if(labels.at<int>(y,x)==i){double v=std::max(0.0,double(smooth.at<float>(y,x))-bg);radii.push_back({std::hypot(x-cx,y-cy),v});}std::sort(radii.begin(),radii.end(),[](auto&a,auto&b){return a.first<b.first;});double acc=0,hfr=0;for(auto [rad,v]:radii){acc+=v;if(acc>=flux*0.5){hfr=rad;break;}}out.push_back({{cx,cy},flux,peak,hfr});}
    std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.flux>b.flux;});if(int(out.size())>options_.maxStars)out.resize(options_.maxStars);return out;
}
}
