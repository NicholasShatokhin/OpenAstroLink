#pragma once
#include "algorithms/star_catalog.h"
#include "algorithms/star_detector.h"
#include "core/interfaces.h"
#include <memory>

namespace oas {
class PatternPlateSolver final : public IPlateSolver {
public:
    explicit PatternPlateSolver(std::shared_ptr<StarCatalog> catalog) : catalog_(std::move(catalog)) {}
    QString name() const override { return "OAL triangle catalog solver"; }
    SolveResult solve(const CameraFrame&,const TelescopeProfile&,const SolveHint&hint={}) override;
private: std::shared_ptr<StarCatalog> catalog_; StarDetector detector_;
};
}
