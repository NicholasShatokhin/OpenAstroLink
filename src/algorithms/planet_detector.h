#pragma once

#include <opencv2/core.hpp>

namespace oas {

struct PlanetDetection {
    bool found{false};
    cv::Point2d centroidPx{};
    cv::Rect bounds{};
    double confidence{0.0};
    double peak{0.0};
    double background{0.0};
};

class PlanetDetector {
public:
    PlanetDetection detect(const cv::Mat &image) const;
};

} // namespace oas
