#pragma once
#include <QImage>
#include <optional>

struct NeuralSolveResult {
    double raDeg;
    double decDeg;
    double paDeg;
    double confidence;
};

// Заглушка під реальну NN: зараз просто повертає empty.
// Потім ти сюди впихаєш свій onnxruntime / libtorch.
class NeuralSolver {
public:
    void setModelPath(const QString &p) { modelPath_ = p; }

    std::optional<NeuralSolveResult> solve(const QImage &img) {
        Q_UNUSED(img);
        // TODO: викликати нейромодель
        return std::nullopt;
    }

private:
    QString modelPath_;
};
