#pragma once
#include <vector>
#include <optional>
#include <cmath>

// Вхідні зірки з кадру
struct ImageStar {
    double x;   // px
    double y;   // px
    double flux;
};

// Зірка з каталогу (небесні координати)
struct CatalogEntry {
    double raDeg;
    double decDeg;
    double mag;
};

struct AstroSolution {
    double raDeg = 0.0;
    double decDeg = 0.0;
    double paDeg = 0.0;
    int matchedStars = 0;
    bool valid = false;
};

// Дуже спрощений солвер:
// 1) бере N яскравих зірок з кадру
// 2) шукає таку ж трикутну конфігурацію в каталозі
// 3) обчислює афінне перетворення "image -> sky"
// 4) перевіряє, скільки зірок лягло
class AstrometrySolver {
public:
    void setCatalog(const std::vector<CatalogEntry> &cat) {
        catalog_ = cat;
    }

    // focalLenMm, pixelUm потрібні щоб з px зробити кут
    std::optional<AstroSolution> solve(const std::vector<ImageStar> &imgStars,
                                       double focalLenMm,
                                       double pixelUm);

private:
    std::vector<CatalogEntry> catalog_;
};
