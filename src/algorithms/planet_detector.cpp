#include "algorithms/planet_detector.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <vector>

namespace oas {
namespace {
cv::Mat gray32(const cv::Mat &src) {
    cv::Mat g;
    if (src.channels() == 1) g = src;
    else cv::cvtColor(src, g, cv::COLOR_BGR2GRAY);
    cv::Mat f; g.convertTo(f, CV_32F); return f;
}

double percentile(std::vector<float> values, double q) {
    if (values.empty()) return 0.0;
    const size_t k = std::min(values.size()-1, size_t(std::floor(q * double(values.size()-1))));
    std::nth_element(values.begin(), values.begin()+ptrdiff_t(k), values.end());
    return values[k];
}
}

PlanetDetection PlanetDetector::detect(const cv::Mat &image) const {
    PlanetDetection out;
    if (image.empty() || image.cols < 8 || image.rows < 8) return out;
    cv::Mat f = gray32(image), smooth;
    cv::GaussianBlur(f, smooth, cv::Size(5,5), 0.0);

    const int sx = std::max(1, smooth.cols / 256), sy = std::max(1, smooth.rows / 256);
    std::vector<float> sample; sample.reserve(size_t((smooth.cols+sx-1)/sx) * size_t((smooth.rows+sy-1)/sy));
    for (int y=0;y<smooth.rows;y+=sy) {
        const float *row=smooth.ptr<float>(y);
        for (int x=0;x<smooth.cols;x+=sx) sample.push_back(row[x]);
    }
    const double bg = percentile(sample,0.50), p995 = percentile(sample,0.995);
    double mn=0,mx=0;cv::minMaxLoc(smooth,&mn,&mx);
    if (!(mx > bg)) return out;
    const double threshold = std::max(bg + 0.18*(mx-bg), bg + 0.55*(p995-bg));
    cv::Mat mask;cv::threshold(smooth,mask,threshold,255,cv::THRESH_BINARY);mask.convertTo(mask,CV_8U);
    cv::morphologyEx(mask,mask,cv::MORPH_OPEN,cv::getStructuringElement(cv::MORPH_ELLIPSE,{3,3}));
    cv::morphologyEx(mask,mask,cv::MORPH_CLOSE,cv::getStructuringElement(cv::MORPH_ELLIPSE,{5,5}));

    cv::Mat labels,stats,centroids;const int n=cv::connectedComponentsWithStats(mask,labels,stats,centroids,8,CV_32S);
    double bestScore=0.0;int best=-1;
    const double maxArea=0.35*double(image.total());
    for(int i=1;i<n;++i){
        const int area=stats.at<int>(i,cv::CC_STAT_AREA);if(area<4||area>maxArea)continue;
        const int x=stats.at<int>(i,cv::CC_STAT_LEFT),y=stats.at<int>(i,cv::CC_STAT_TOP),w=stats.at<int>(i,cv::CC_STAT_WIDTH),h=stats.at<int>(i,cv::CC_STAT_HEIGHT);
        const cv::Rect r(x,y,w,h);double flux=0,peak=0;cv::Point2d weighted{};
        for(int yy=r.y;yy<r.y+r.height;++yy){const int *lr=labels.ptr<int>(yy);const float *fr=smooth.ptr<float>(yy);for(int xx=r.x;xx<r.x+r.width;++xx)if(lr[xx]==i){const double v=std::max(0.0,double(fr[xx])-bg);flux+=v;peak=std::max(peak,double(fr[xx]));weighted.x+=v*xx;weighted.y+=v*yy;}}
        if(flux<=0)continue;const double score=flux*std::sqrt(double(area));if(score>bestScore){bestScore=score;best=i;out.centroidPx={weighted.x/flux,weighted.y/flux};out.bounds=r;out.peak=peak;}
    }
    if(best<0)return out;
    out.found=true;out.background=bg;out.confidence=std::clamp((out.peak-bg)/std::max(1.0,mx-bg),0.0,1.0);return out;
}

} // namespace oas
