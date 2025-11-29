#pragma once
#include <QString>
#include <vector>
#include <optional>

struct CatalogStar {
    QString name;
    double raDeg;
    double decDeg;
    double mag;
};

class StarCatalog {
public:
    bool loadCsv(const QString &path);

    const std::vector<CatalogStar>& stars() const { return stars_; }

    // знайти найближчу до RA/DEC
    std::optional<CatalogStar> findNearest(double raDeg, double decDeg, double maxDistDeg = 2.0) const;

private:
    std::vector<CatalogStar> stars_;
};
