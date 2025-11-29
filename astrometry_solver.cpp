#include "astrometry_solver.h"
#include <algorithm>

// допоміжна: px -> кут (рад)
static double px_to_rad(double px, double focal_mm, double pixel_um) {
    // фізичний розмір пікселя
    double px_mm = pixel_um / 1000.0;
    double angle_rad = std::atan2(px * px_mm, focal_mm); // small angle
    return angle_rad;
}

// небесні координати в 2D (локальна проєкція, наприклад TAN)
struct Sky2D {
    double x;
    double y;
};

// спрощено: переводимо RA/DEC в локальні XY навколо першої зірки каталогу
static Sky2D project_tan(double ra0, double dec0, double ra, double dec) {
    // все в рад
    double ra0r = ra0 * M_PI/180.0;
    double dec0r= dec0* M_PI/180.0;
    double rar  = ra  * M_PI/180.0;
    double decr = dec * M_PI/180.0;

    double cosc = std::sin(dec0r)*std::sin(decr) + std::cos(dec0r)*std::cos(decr)*std::cos(rar - ra0r);
    double x = (std::cos(decr) * std::sin(rar - ra0r)) / cosc;
    double y = (std::cos(dec0r)*std::sin(decr) - std::sin(dec0r)*std::cos(decr)*std::cos(rar - ra0r)) / cosc;
    return {x, y};
}

std::optional<AstroSolution> AstrometrySolver::solve(const std::vector<ImageStar> &imgStars,
                                                     double focalLenMm,
                                                     double pixelUm)
{
    if (imgStars.size() < 3 || catalog_.size() < 3)
        return std::nullopt;

    // 1. візьмемо перші 3 найяскравіші зображення
    std::vector<ImageStar> img = imgStars;
    std::sort(img.begin(), img.end(), [](auto &a, auto &b){ return a.flux > b.flux; });
    size_t useN = std::min<size_t>(img.size(), 20);
    img.resize(useN);

    // 2. в каталозі теж візьмемо перші K яскравих (тут припустимо вже відфільтрований)
    size_t catN = std::min<size_t>(catalog_.size(), 2000);

    // 3. беремо базову каталожну зірку як центр проєкції
    const auto &c0 = catalog_[0];

    // 4. переберемо кілька триплетів зображення і каталогу (дуже грубо, але працює на малих наборах)
    AstroSolution best;
    for (size_t i=0; i<img.size(); ++i) {
        for (size_t j=i+1; j<img.size(); ++j) {
            for (size_t k=j+1; k<img.size(); ++k) {
                // три зірки зображення
                auto s1 = img[i];
                auto s2 = img[j];
                auto s3 = img[k];

                // Вектори в радіанах (кут між пікселями)
                // (x,y) -> (?x, ?y)
                // точка 1 буде в (0,0)
                double x2 = px_to_rad(s2.x - s1.x, focalLenMm, pixelUm);
                double y2 = px_to_rad(s2.y - s1.y, focalLenMm, pixelUm);
                double x3 = px_to_rad(s3.x - s1.x, focalLenMm, pixelUm);
                double y3 = px_to_rad(s3.y - s1.y, focalLenMm, pixelUm);

                // тепер пробуємо знайти в каталозі три зірки з подібними відносними відстанями
                for (size_t ci=0; ci<catN; ++ci) {
                    for (size_t cj=ci+1; cj<catN; ++cj) {
                        for (size_t ck=cj+1; ck<catN; ++ck) {
                            auto C1 = catalog_[ci];
                            auto C2 = catalog_[cj];
                            auto C3 = catalog_[ck];

                            // проектуємо C2,C3 у площину навколо C1
                            auto p2 = project_tan(C1.raDeg, C1.decDeg, C2.raDeg, C2.decDeg);
                            auto p3 = project_tan(C1.raDeg, C1.decDeg, C3.raDeg, C3.decDeg);

                            // тепер у нас є два трикутники: (0,0), (x2,y2), (x3,y3) і (0,0), (p2.x,p2.y), (p3.x,p3.y)
                            // шукаємо масштаб+обертання, що переводить перший у другий
                            // вирішимо по двох точках (s2 -> p2, s3 -> p3)
                            double denom = (x2*y3 - y2*x3);
                            if (std::abs(denom) < 1e-9) continue;

                            // матриця перетворення
                            // [a b] [x2] = [p2.x]
                            // [c d] [y2]   [p2.y]
                            // і те саме для (x3,y3)->(p3.x,p3.y)
                            // розв?яжемо через формули Крамера
                            double a = ( p2.x*y3 - p3.x*y2) / denom;
                            double b = (-p2.x*x3 + p3.x*x2) / denom;
                            double c = ( p2.y*y3 - p3.y*y2) / denom;
                            double d = (-p2.y*x3 + p3.y*x2) / denom;

                            // тепер перевіримо, скільки інших зірок зображення ляжуть у каталог
                            int matched = 0;
                            for (auto &si : img) {
                                // координата відносно s1
                                double rx = px_to_rad(si.x - s1.x, focalLenMm, pixelUm);
                                double ry = px_to_rad(si.y - s1.y, focalLenMm, pixelUm);
                                double sx = a*rx + b*ry;
                                double sy = c*rx + d*ry;

                                // шукаємо в каталозі найближчу до (sx,sy) у площині навколо C1
                                double bestd = 1e9;
                                for (size_t cc=0; cc<catN; ++cc) {
                                    auto pp = project_tan(C1.raDeg, C1.decDeg, catalog_[cc].raDeg, catalog_[cc].decDeg);
                                    double dx = pp.x - sx;
                                    double dy = pp.y - sy;
                                    double dd = dx*dx + dy*dy;
                                    if (dd < bestd) bestd = dd;
                                }
                                if (bestd < 1e-6) {
                                    matched++;
                                }
                            }

                            if (matched > best.matchedStars) {
                                best.matchedStars = matched;
                                best.raDeg = C1.raDeg;
                                best.decDeg = C1.decDeg;
                                // PA приблизно з матриці
                                double pa = std::atan2(b, a); // рад
                                best.paDeg = pa * 180.0/M_PI;
                                best.valid = true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (best.valid)
        return best;
    return std::nullopt;
}
