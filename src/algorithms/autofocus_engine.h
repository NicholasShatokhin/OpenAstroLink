#pragma once
#include "algorithms/star_detector.h"
#include "core/interfaces.h"
#include <functional>

namespace oas {
class AutofocusEngine {
public:
    using Progress = std::function<void(const FocusSample&)>;
    using FrameProgress = std::function<void(const CameraFrame&,int focusPosition)>;
    using Cancellation = std::function<bool()>;
    AutofocusResult run(ICamera &camera,IFocuser &focuser,const AutofocusRequest &request,const Progress &progress={},const Cancellation &cancel={},const FrameProgress &frameProgress={});
private:
    static double score(const cv::Mat&,AutofocusMode,bool autoPlanetRoi,int *detectedStars=nullptr);
    static cv::Rect planetRoi(const cv::Mat&);
    static double median(std::vector<double> values);
    std::vector<FocusSample> scan(ICamera&,IFocuser&,const AutofocusRequest&,int start,int end,int step,const Progress&,const Cancellation&,const FrameProgress&);
    static bool interruptibleSleep(int milliseconds,const Cancellation&);
};
}
