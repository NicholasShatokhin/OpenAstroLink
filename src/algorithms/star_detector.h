#pragma once
#include "core/astro_types.h"

namespace oas {
struct StarDetectorOptions {
    double thresholdSigma{3.5};
    int minAreaPx{2};
    int maxAreaPx{300};
    int maxStars{100};
};
class StarDetector {
public:
    explicit StarDetector(StarDetectorOptions options = {}) : options_(options) {}
    std::vector<DetectedStar> detect(const cv::Mat &image) const;
private:
    StarDetectorOptions options_;
};
}
