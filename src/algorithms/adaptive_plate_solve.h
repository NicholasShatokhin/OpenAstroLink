#pragma once

#include "algorithms/motion_estimator.h"
#include "algorithms/star_detector.h"
#include "core/astro_types.h"

#include <QString>
#include <vector>

namespace oas {

// Lightweight quality metrics used to decide whether invoking an external
// plate solver is worthwhile. These values are diagnostics, not hard ASTAP
// requirements; the final adaptive attempt always invokes the solver.
struct SolveFrameQuality {
    int detectedStars{0};
    double background{0.0};
    double noiseSigma{0.0};
    double p99{0.0};
    double saturationFraction{0.0};
    double medianHfrPx{0.0};
};

struct AdaptivePreparedFrame {
    CameraFrame frame;
    SolveFrameQuality quality;
    int inputFrames{0};
    int registeredFrames{0};
};

// Prepares short, drift-limited exposures for urban plate solving. The class
// deliberately performs only image-domain work; capture/retry policy lives in
// ApplicationController so it can remain cancellable and resource-locked.
class AdaptivePlateSolvePreprocessor {
public:
    SolveFrameQuality assess(const cv::Mat &image) const;
    AdaptivePreparedFrame prepare(const std::vector<CameraFrame> &frames,
                                  bool registerFrames,
                                  bool equalizeBackground,
                                  QString *warning = nullptr) const;

private:
    cv::Mat equalized16(const cv::Mat &input) const;
    StarDetector detector_{StarDetectorOptions{3.0, 2, 600, 250}};
    MotionEstimator motion_;
};

} // namespace oas
