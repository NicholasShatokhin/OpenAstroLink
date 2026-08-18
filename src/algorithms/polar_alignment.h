#pragma once
#include "core/astro_types.h"

namespace oas {
class PolarAlignmentEstimator {
public: PolarAlignmentResult estimate(const std::vector<PolarSample>&samples,const ObserverLocation&observer,const QDateTime&utc) const;
};
}
