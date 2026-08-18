#pragma once
#include "algorithms/star_detector.h"
#include "core/interfaces.h"
#include <functional>

namespace oas {
class AutofocusEngine {
public:
    using Progress = std::function<void(const FocusSample&)>;
    AutofocusResult run(ICamera &camera,IFocuser &focuser,const AutofocusRequest &request,const Progress &progress={});
private:
    static double score(const cv::Mat&,AutofocusMode,bool autoPlanetRoi);
    static cv::Rect planetRoi(const cv::Mat&);
    static double median(std::vector<double> values);
    std::vector<FocusSample> scan(ICamera&,IFocuser&,AutofocusMode,int start,int end,int step,int frames,int settle,bool autoRoi,const Progress&);
};
}
