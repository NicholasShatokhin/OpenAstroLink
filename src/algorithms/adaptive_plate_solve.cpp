#include "algorithms/adaptive_plate_solve.h"

#include <QDateTime>
#include <QUuid>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace oas {
namespace {

cv::Mat gray32(const cv::Mat &src) {
    if (src.empty()) return {};
    cv::Mat mono;
    if (src.channels() == 3) cv::cvtColor(src, mono, cv::COLOR_BGR2GRAY);
    else if (src.channels() == 4) cv::cvtColor(src, mono, cv::COLOR_BGRA2GRAY);
    else mono = src;

    cv::Mat out;
    double scale = 1.0;
    if (mono.depth() == CV_8U) scale = 1.0 / 255.0;
    else if (mono.depth() == CV_16U) scale = 1.0 / 65535.0;
    mono.convertTo(out, CV_32F, scale);
    return out;
}

double percentile(const cv::Mat &image, double p) {
    if (image.empty()) return 0.0;
    cv::Mat small;
    const double s = std::min(1.0, 256.0 / std::max(image.cols, image.rows));
    if (s < 1.0) cv::resize(image, small, {}, s, s, cv::INTER_AREA);
    else small = image;
    const cv::Mat contiguous = small.isContinuous() ? small : small.clone();
    std::vector<float> v;
    const auto *begin = contiguous.ptr<float>(0);
    v.assign(begin, begin + contiguous.total());
    if (v.empty()) return 0.0;
    const auto idx = std::clamp<size_t>(size_t(std::floor((v.size() - 1) * p)), 0, v.size() - 1);
    std::nth_element(v.begin(), v.begin() + idx, v.end());
    return v[idx];
}

cv::Mat robustStretch16(const cv::Mat &f) {
    if (f.empty()) return {};
    const double lo = percentile(f, 0.005);
    const double hi = percentile(f, 0.998);
    cv::Mat clipped;
    if (hi <= lo + 1e-9) f.convertTo(clipped, CV_16U, 65535.0);
    else {
        cv::Mat t = (f - float(lo)) * float(1.0 / (hi - lo));
        cv::max(t, 0.0, t);
        cv::min(t, 1.0, t);
        t.convertTo(clipped, CV_16U, 65535.0);
    }
    return clipped;
}

cv::Mat affineFromMotion(const FrameMotion &m) {
    const double r = m.rotationDeg * CV_PI / 180.0;
    const double c = std::cos(r) * m.scale;
    const double s = std::sin(r) * m.scale;
    return (cv::Mat_<double>(2,3) << c, -s, m.dxPx, s, c, m.dyPx);
}

} // namespace

SolveFrameQuality AdaptivePlateSolvePreprocessor::assess(const cv::Mat &image) const {
    SolveFrameQuality q;
    const cv::Mat g = gray32(image);
    if (g.empty()) return q;
    q.background = percentile(g, 0.50);
    q.p99 = percentile(g, 0.99);
    const double p16 = percentile(g, 0.16);
    const double p84 = percentile(g, 0.84);
    q.noiseSigma = std::max(0.0, 0.5 * (p84 - p16));
    cv::Mat sat = g >= 0.985f;
    q.saturationFraction = double(cv::countNonZero(sat)) / double(std::max<size_t>(1, g.total()));
    const auto stars = detector_.detect(image);
    q.detectedStars = int(stars.size());
    std::vector<double> hfr;
    hfr.reserve(stars.size());
    for (const auto &s : stars) if (s.hfrPx > 0.0 && std::isfinite(s.hfrPx)) hfr.push_back(s.hfrPx);
    if (!hfr.empty()) {
        const size_t mid = hfr.size() / 2;
        std::nth_element(hfr.begin(), hfr.begin() + mid, hfr.end());
        q.medianHfrPx = hfr[mid];
    }
    return q;
}

cv::Mat AdaptivePlateSolvePreprocessor::equalized16(const cv::Mat &input) const {
    cv::Mat g = gray32(input);
    if (g.empty()) return {};

    // Estimate the slowly varying city-sky gradient on a reduced image. This is
    // much cheaper than a giant full-resolution convolution and avoids baking a
    // strong light-pollution gradient into ASTAP's star detector.
    cv::Mat small;
    const double scale = std::min(1.0, 384.0 / std::max(g.cols, g.rows));
    if (scale < 1.0) cv::resize(g, small, {}, scale, scale, cv::INTER_AREA);
    else small = g;
    cv::GaussianBlur(small, small, cv::Size(), 8.0, 8.0, cv::BORDER_REPLICATE);
    cv::Mat bg;
    cv::resize(small, bg, g.size(), 0.0, 0.0, cv::INTER_LINEAR);
    const double med = percentile(g, 0.50);
    cv::Mat corrected = g - bg + float(med);
    return robustStretch16(corrected);
}

AdaptivePreparedFrame AdaptivePlateSolvePreprocessor::prepare(const std::vector<CameraFrame> &frames,
                                                               bool registerFrames,
                                                               bool equalizeBackground,
                                                               QString *warning) const {
    AdaptivePreparedFrame out;
    out.inputFrames = int(frames.size());
    if (frames.empty()) {
        if (warning) *warning = "No frames supplied";
        return out;
    }

    std::vector<cv::Mat> prepared;
    prepared.reserve(frames.size());
    for (const auto &f : frames) {
        cv::Mat p = equalizeBackground ? equalized16(f.image) : robustStretch16(gray32(f.image));
        if (!p.empty()) prepared.push_back(std::move(p));
    }
    if (prepared.empty()) {
        if (warning) *warning = "All captured frames were empty";
        return out;
    }

    cv::Mat finalImage;
    int registered = 1;
    double usedExposureSec = frames.front().exposureSec;
    if (prepared.size() == 1 || !registerFrames) {
        if (prepared.size() == 1) finalImage = prepared.front();
        else {
            cv::Mat accum = cv::Mat::zeros(prepared.front().size(), CV_32F);
            int used = 0;
            for (const auto &p : prepared) {
                if (p.size() != prepared.front().size()) continue;
                cv::Mat f; p.convertTo(f, CV_32F, 1.0 / 65535.0); accum += f; ++used;
            }
            if (used > 0) accum *= 1.0 / used;
            finalImage = robustStretch16(accum);
            registered = used;
            usedExposureSec = 0.0;
            for (int i = 0; i < used && i < int(frames.size()); ++i) usedExposureSec += frames[i].exposureSec;
        }
    } else {
        cv::Mat refFloat; prepared.front().convertTo(refFloat, CV_32F, 1.0 / 65535.0);
        cv::Mat accum = refFloat.clone();
        cv::Mat weight = cv::Mat::ones(refFloat.size(), CV_32F);
        const auto refStars = detector_.detect(prepared.front());
        for (size_t i = 1; i < prepared.size(); ++i) {
            if (prepared[i].size() != prepared.front().size()) continue;
            const auto stars = detector_.detect(prepared[i]);
            const FrameMotion m = motion_.estimate(refStars, stars, 140.0);
            if (!m.valid || m.inliers < 3 || std::abs(m.scale - 1.0) > 0.05 || std::abs(m.rotationDeg) > 5.0) continue;
            cv::Mat transform = affineFromMotion(m), inverse;
            cv::invertAffineTransform(transform, inverse);
            cv::Mat curFloat, aligned, mask, alignedMask;
            prepared[i].convertTo(curFloat, CV_32F, 1.0 / 65535.0);
            cv::warpAffine(curFloat, aligned, inverse, refFloat.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, 0.0);
            mask = cv::Mat::ones(refFloat.size(), CV_32F);
            cv::warpAffine(mask, alignedMask, inverse, refFloat.size(), cv::INTER_NEAREST, cv::BORDER_CONSTANT, 0.0);
            accum += aligned;
            weight += alignedMask;
            ++registered;
            if (i < frames.size()) usedExposureSec += frames[i].exposureSec;
        }
        cv::max(weight, cv::Scalar(1e-6), weight);
        cv::Mat averaged;
        cv::divide(accum, weight, averaged);
        finalImage = robustStretch16(averaged);
        if (registered == 1 && prepared.size() > 1 && warning)
            *warning = "Frame registration rejected all additional frames; solving the first short exposure";
    }

    out.registeredFrames = registered;
    out.frame = frames.front();
    out.frame.id = "adaptive-solve-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    out.frame.image = finalImage;
    out.frame.capturedUtc = QDateTime::currentDateTimeUtc();
    out.frame.exposureSec = usedExposureSec;
    out.frame.source = "adaptive-plate-solve";
    out.quality = assess(out.frame.image);
    return out;
}

} // namespace oas
