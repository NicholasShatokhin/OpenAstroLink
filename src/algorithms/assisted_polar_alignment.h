#pragma once
#include "core/astro_types.h"
#include <vector>

namespace oas {
class AssistedPolarAlignmentEstimator {
public:
    AssistedPolarResult estimate(const std::vector<AssistedPolarSample> &samples,
                                 const ObserverLocation &observer) const;
};
}
