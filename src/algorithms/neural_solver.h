#pragma once
#include "core/interfaces.h"
namespace oas {
class NeuralSolver final : public INeuralSolver {
public:
    QString name() const override{return "Neural solver adapter";}
    bool loadModel(const QString &path,QString *error=nullptr) override;
    SolveResult solve(const CameraFrame&,const TelescopeProfile&,const SolveHint &hint = {}) override;
private:QString modelPath_;
};
}
