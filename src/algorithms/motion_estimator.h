#pragma once
#include "core/astro_types.h"

namespace oas {
struct FrameMotion { bool valid{false}; double dxPx{0}; double dyPx{0}; double rotationDeg{0}; double scale{1}; int inliers{0}; };
class MotionEstimator {
public: FrameMotion estimate(const std::vector<DetectedStar>&a,const std::vector<DetectedStar>&b,double maxMatchDistancePx=80.0) const;
};
}
