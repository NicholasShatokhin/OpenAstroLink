#pragma once
#include <optional>

struct GuidingCorrection {
    // дугов≥ секунди або умовн≥ одиниц≥ Ч €к тоб≥ зручн≥ше
    double dRaArcsec  = 0.0;
    double dDecArcsec = 0.0;
};

class Guiding {
public:
    // задати ц≥ль, куди маЇмо дивитис€
    void setTarget(double raDeg, double decDeg) {
        targetRaDeg_  = raDeg;
        targetDecDeg_ = decDeg;
    }

    // ќбчислити корекц≥ю за поточним положенн€м монтировки
    std::optional<GuidingCorrection> compute(double currentRaDeg, double currentDecDeg) {
        // якщо ц≥ль ще не задано Ц н≥чого не робимо
        if (!targetRaDeg_.has_value() || !targetDecDeg_.has_value())
            return std::nullopt;

        // ƒ≥стаЇмо значенн€ з optional
        const double targetRa  = *targetRaDeg_;
        const double targetDec = *targetDecDeg_;

        double dRaDeg  = targetRa  - currentRaDeg;
        double dDecDeg = targetDec - currentDecDeg;
        // грубо: 1 deg = 3600 arcsec
        GuidingCorrection c;
        c.dRaArcsec  = dRaDeg  * 3600.0;
        c.dDecArcsec = dDecDeg * 3600.0;
        return c;
    }

    bool hasTarget() const { return targetRaDeg_.has_value(); }

private:
    std::optional<double> targetRaDeg_;
    std::optional<double> targetDecDeg_;
};
