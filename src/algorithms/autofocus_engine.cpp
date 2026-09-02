#include "algorithms/autofocus_engine.h"
#include <QThread>
#include <QElapsedTimer>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>

namespace oas {

static cv::Mat grayFixed8(const cv::Mat &src) {
    cv::Mat gray;
    if (src.channels() == 3) cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else if (src.channels() == 4) cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    else gray = src;
    cv::Mat out;
    if (gray.depth() == CV_16U) gray.convertTo(out, CV_8U, 255.0 / 65535.0);
    else if (gray.depth() == CV_8U) out = gray;
    else {
        double mn = 0.0, mx = 0.0;
        cv::minMaxLoc(gray, &mn, &mx);
        gray.convertTo(out, CV_8U, 255.0 / std::max(1.0, mx));
    }
    return out;
}

static cv::Mat sceneCrop(const cv::Mat &src) {
    if (src.empty()) return {};
    const int w = std::max(32, int(std::lround(src.cols * 0.78)));
    const int h = std::max(32, int(std::lround(src.rows * 0.78)));
    const int x = std::max(0, (src.cols - w) / 2);
    const int y = std::max(0, (src.rows - h) / 2);
    return src(cv::Rect(x, y, std::min(w, src.cols - x), std::min(h, src.rows - y)));
}

struct FrameLevels {
    double p50{0.0};
    double p95{0.0};
    double p99{0.0};
    double highClip{0.0};
};

static FrameLevels frameLevels(const cv::Mat &src) {
    FrameLevels out;
    cv::Mat gray = grayFixed8(src);
    if (gray.empty()) return out;
    if (std::max(gray.cols, gray.rows) > 1200) {
        const double k = 1200.0 / double(std::max(gray.cols, gray.rows));
        cv::resize(gray, gray, {}, k, k, cv::INTER_AREA);
    }
    std::array<std::uint64_t, 256> hist{};
    std::uint64_t total = 0;
    for (int y = 0; y < gray.rows; ++y) {
        const auto *row = gray.ptr<std::uint8_t>(y);
        for (int x = 0; x < gray.cols; ++x) { ++hist[row[x]]; ++total; }
    }
    if (!total) return out;
    auto pct = [&](double p) {
        const std::uint64_t target = std::uint64_t(std::clamp(p, 0.0, 1.0) * double(total - 1));
        std::uint64_t acc = 0;
        for (int i = 0; i < 256; ++i) { acc += hist[std::size_t(i)]; if (acc > target) return double(i) / 255.0; }
        return 1.0;
    };
    out.p50 = pct(0.50); out.p95 = pct(0.95); out.p99 = pct(0.99);
    out.highClip = double(hist[254] + hist[255]) / double(total);
    return out;
}

static cv::Mat toGray8(const cv::Mat &s) {
    cv::Mat g;
    if (s.channels() == 3) cv::cvtColor(s, g, cv::COLOR_BGR2GRAY);
    else g = s;
    cv::Mat o;
    double minv = 0.0, maxv = 0.0;
    cv::minMaxLoc(g, &minv, &maxv);
    g.convertTo(o, CV_8U, 255.0 / std::max(1.0, maxv - minv), -minv * 255.0 / std::max(1.0, maxv - minv));
    return o;
}

double AutofocusEngine::median(std::vector<double> v) {
    if (v.empty()) return 0;
    auto m = v.begin() + v.size() / 2;
    std::nth_element(v.begin(), m, v.end());
    double x = *m;
    if (v.size() % 2 == 0) { auto m2 = std::max_element(v.begin(), m); x = (x + *m2) / 2; }
    return x;
}

cv::Rect AutofocusEngine::planetRoi(const cv::Mat &im) {
    cv::Mat g = toGray8(im), b;
    cv::GaussianBlur(g, b, {5, 5}, 1.2);
    cv::Scalar m, s; cv::meanStdDev(b, m, s);
    cv::Mat mask; cv::threshold(b, mask, std::min(250.0, m[0] + 2.0 * s[0]), 255, cv::THRESH_BINARY);
    std::vector<std::vector<cv::Point>> cs; cv::findContours(mask, cs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    double ba = 0; cv::Rect br;
    for (auto &c : cs) { double a = cv::contourArea(c); if (a > ba) { ba = a; br = cv::boundingRect(c); } }
    if (ba < 10) return {im.cols / 4, im.rows / 4, im.cols / 2, im.rows / 2};
    int pad = std::max(br.width, br.height);
    br.x = std::max(0, br.x - pad); br.y = std::max(0, br.y - pad);
    br.width = std::min(im.cols - br.x, br.width + 2 * pad); br.height = std::min(im.rows - br.y, br.height + 2 * pad);
    return br;
}

// Contrast-detect scene AF metric.  The exposure is held fixed while a focus
// search is active, so the score intentionally avoids per-frame min/max
// normalization.  It measures high-frequency structure in a central AF area,
// similar in spirit to classic contrast-detect AF, while suppressing sensor
// noise with a small pre-blur.
static double sceneFocusScore(const cv::Mat &im) {
    if (im.empty()) return 0.0;
    cv::Mat crop = sceneCrop(im);
    cv::Mat gray;
    if (crop.channels() == 3) cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);
    else if (crop.channels() == 4) cv::cvtColor(crop, gray, cv::COLOR_BGRA2GRAY);
    else gray = crop;
    cv::Mat f;
    if (gray.depth() == CV_16U) gray.convertTo(f, CV_32F, 1.0 / 65535.0);
    else if (gray.depth() == CV_8U) gray.convertTo(f, CV_32F, 1.0 / 255.0);
    else { double mn = 0, mx = 0; cv::minMaxLoc(gray, &mn, &mx); gray.convertTo(f, CV_32F, 1.0 / std::max(1.0, mx)); }
    if (std::max(f.cols, f.rows) > 1200) { const double k = 1200.0 / double(std::max(f.cols, f.rows)); cv::resize(f, f, {}, k, k, cv::INTER_AREA); }

    const FrameLevels lv = frameLevels(gray);
    if (lv.p99 < 0.015 || lv.highClip > 0.20) return 0.0;

    cv::Mat smooth; cv::GaussianBlur(f, smooth, {0, 0}, 0.85);
    cv::Mat gx, gy, mag; cv::Sobel(smooth, gx, CV_32F, 1, 0, 3); cv::Sobel(smooth, gy, CV_32F, 0, 1, 3); cv::magnitude(gx, gy, mag);
    cv::Scalar meanMag, stdMag; cv::meanStdDev(mag, meanMag, stdMag);
    cv::Mat lap; cv::Laplacian(smooth, lap, CV_32F, 3); cv::Scalar meanLap, stdLap; cv::meanStdDev(lap, meanLap, stdLap);

    // Tenengrad-like edge energy + Laplacian spread.  Since illumination is
    // fixed across the focus sweep, a local maximum is meaningful; absolute
    // score magnitude is not used as a universal threshold.
    return 1.0e4 * (meanMag[0] + 0.55 * stdMag[0] + 0.35 * stdLap[0]);
}

double AutofocusEngine::score(const cv::Mat &im, AutofocusMode mode, bool autoRoi, int *detectedStars) {
    if (detectedStars) *detectedStars = 0;
    cv::Mat g = toGray8(im); cv::Rect roi(0, 0, g.cols, g.rows); if (mode == AutofocusMode::Planet && autoRoi) roi = planetRoi(g); cv::Mat c = g(roi);
    if (mode == AutofocusMode::Stars) { StarDetector det; auto stars = det.detect(c); if (detectedStars) *detectedStars = int(stars.size()); std::vector<double> q; for (const auto &s : stars) if (s.hfrPx > 0.1) q.push_back(s.flux / (s.hfrPx * s.hfrPx)); return median(q); }
    if (mode == AutofocusMode::Scene) return sceneFocusScore(im);
    cv::Mat gx, gy; cv::Sobel(c, gx, CV_32F, 1, 0, 3); cv::Sobel(c, gy, CV_32F, 0, 1, 3); cv::Mat mag; cv::magnitude(gx, gy, mag); cv::Scalar mean, stddev; cv::meanStdDev(mag, mean, stddev); double v = mean[0] + 0.5 * stddev[0];
    if (mode == AutofocusMode::Bahtinov) { cv::Mat lap; cv::Laplacian(c, lap, CV_32F); cv::meanStdDev(lap, mean, stddev); v += stddev[0]; }
    return v;
}

bool AutofocusEngine::interruptibleSleep(int milliseconds, const Cancellation &cancel) {
    int remaining = std::max(0, milliseconds);
    while (remaining > 0) { if (cancel && cancel()) return false; const int slice = std::min(remaining, 25); QThread::msleep(slice); remaining -= slice; }
    return !(cancel && cancel());
}

static bool waitForFocuserIdle(IFocuser &foc, const AutofocusEngine::Cancellation &cancel, QString &error, int timeoutMs = 120000) {
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (cancel && cancel()) { error = "Autofocus cancelled"; return false; }
        FocuserStatus status; if (!foc.status(status, &error)) return false; if (!status.moving) return true;
        int remaining = 200; while (remaining > 0) { if (cancel && cancel()) { error = "Autofocus cancelled"; return false; } const int slice = std::min(remaining, 25); QThread::msleep(slice); remaining -= slice; }
    }
    error = QString("Focuser motion did not complete within %1 ms").arg(timeoutMs); return false;
}

std::vector<FocusSample> AutofocusEngine::scan(ICamera &cam, IFocuser &foc, const AutofocusRequest &r, int start, int end, int step, const Progress &cb, const Cancellation &cancel, const FrameProgress &frameCb) {
    std::vector<FocusSample> out; if (step <= 0) return out;
    for (int p = start; p <= end; p += step) {
        if (cancel && cancel()) break; QString err; if (!foc.moveAbsolute(std::max(0, p), &err)) continue; if (!waitForFocuserIdle(foc, cancel, err)) break; if (!interruptibleSleep(std::max(0, r.settleMs), cancel)) break;
        std::vector<double> vals; std::vector<int> starCounts; bool previewSent = false;
        for (int i = 0; i < std::max(1, r.framesPerPosition); ++i) {
            if (cancel && cancel()) break; CameraFrame f; ExposureRequest e; e.exposureSec = std::max(0.000001, r.exposureSec); e.gain = std::max(0, r.gain); int stars = 0;
            if (cam.capture(e, f, &err)) { if (frameCb && !previewSent) { frameCb(f, std::max(0, p)); previewSent = true; } const double v = score(f.image, r.mode, r.autoPlanetRoi, &stars); if (r.mode != AutofocusMode::Stars || stars >= std::max(1, r.minStars)) vals.push_back(v); starCounts.push_back(stars); }
        }
        if (cancel && cancel()) break;
        if (vals.empty()) { FocusSample sm{std::max(0, p), 0.0, 0.0, starCounts.empty() ? 0 : *std::max_element(starCounts.begin(), starCounts.end())}; out.push_back(sm); if (cb) cb(sm); continue; }
        double med = median(vals), spread = 0; for (double x : vals) spread += std::abs(x - med); spread /= vals.size(); const int stars = starCounts.empty() ? 0 : *std::max_element(starCounts.begin(), starCounts.end()); FocusSample sm{std::max(0, p), med, spread, stars}; out.push_back(sm); if (cb) cb(sm);
    }
    return out;
}

static double meterSceneExposure(ICamera &cam, AutofocusRequest &effective, const AutofocusEngine::Cancellation &cancel, const AutofocusEngine::FrameProgress &frameCb) {
    double exposure = std::clamp(effective.exposureSec, 0.00005, 10.0);
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (cancel && cancel()) break;
        ExposureRequest req; req.exposureSec = exposure; req.gain = std::max(0, effective.gain); req.saveRaw = false;
        CameraFrame f; QString err; if (!cam.capture(req, f, &err) || f.image.empty()) break;
        if (frameCb) frameCb(f, -1);
        const FrameLevels lv = frameLevels(sceneCrop(f.image));
        // AF wants texture with headroom, not a photographic "pretty" exposure.
        // A p95 around 55% leaves plenty of linear range for edge measurement.
        if (lv.highClip <= 0.01 && lv.p95 >= 0.18 && lv.p95 <= 0.82 && lv.p50 >= 0.02) break;
        double factor = std::pow(0.55 / std::max(0.01, lv.p95), 0.78);
        if (lv.highClip > 0.01 || lv.p99 > 0.96) factor = std::min(factor, 0.32);
        if (lv.p95 < 0.06) factor = std::max(factor, 2.0);
        factor = std::clamp(factor, 0.20, 4.0);
        const double next = std::clamp(exposure * factor, 0.00005, 10.0);
        if (std::abs(next / exposure - 1.0) < 0.08) break;
        exposure = next;
    }
    effective.exposureSec = exposure;
    return exposure;
}

AutofocusResult AutofocusEngine::run(ICamera &cam, IFocuser &foc, const AutofocusRequest &r, const Progress &cb, const Cancellation &cancel, const FrameProgress &frameCb) {
    AutofocusResult out; FocuserStatus fs; QString err;
    if (cancel && cancel()) { out.message = "Autofocus cancelled"; return out; }
    if (!foc.status(fs, &err)) { out.message = err; return out; }
    const int originalPosition = fs.position;

    auto failAndRestore = [&](QString message, const std::vector<FocusSample> &samples) {
        AutofocusResult failed; failed.samples = samples; failed.bestPosition = originalPosition; failed.bestScore = 0.0;
        QString restoreError;
        if (!foc.moveAbsolute(originalPosition, &restoreError) || !waitForFocuserIdle(foc, {}, restoreError)) {
            failed.message = message + "; WARNING: could not restore starting focus position: " + restoreError;
        } else {
            failed.message = message + QString("; starting focus position %1 restored").arg(originalPosition);
        }
        return failed;
    };
    auto cancelAndRestore = [&](const std::vector<FocusSample> &samples) {
        foc.halt(nullptr);
        return failAndRestore("Autofocus cancelled", samples);
    };

    AutofocusRequest effective = r;
    if (effective.mode == AutofocusMode::Scene) meterSceneExposure(cam, effective, cancel, frameCb);
    if (cancel && cancel()) return cancelAndRestore({});

    const int coarseStep = std::max(1, effective.coarseStep), fineStep = std::max(1, effective.fineStep);
    const int half = std::max(coarseStep, effective.rangeSteps / 2);

    // Scene autofocus uses a camera-like local contrast search.  It probes both
    // directions around the current focus, follows only a demonstrably improving
    // direction, brackets the peak, then refines it.  A flat or monotonic curve
    // is a safe failure and returns to the original focus instead of leaving the
    // focuser at the end of a blind sweep.
    if (effective.mode == AutofocusMode::Scene) {
        std::vector<FocusSample> samples;
        auto sampleAt = [&](int p) -> FocusSample {
            auto v = scan(cam, foc, effective, std::max(0, p), std::max(0, p), coarseStep, cb, cancel, frameCb);
            if (v.empty()) return FocusSample{std::max(0, p), 0.0, 0.0, 0};
            samples.push_back(v.front()); return v.front();
        };
        const int minPos = std::max(0, originalPosition - half), maxPos = originalPosition + half;
        const FocusSample center = sampleAt(originalPosition);
        if (cancel && cancel()) return cancelAndRestore(samples);
        if (center.score <= 0.0) return failAndRestore(QString("No usable scene contrast at the starting position (AF metered exposure %1 s)").arg(effective.exposureSec, 0, 'g', 5), samples);

        const FocusSample left = originalPosition - coarseStep >= minPos ? sampleAt(originalPosition - coarseStep) : center;
        const FocusSample right = originalPosition + coarseStep <= maxPos ? sampleAt(originalPosition + coarseStep) : center;
        if (cancel && cancel()) return cancelAndRestore(samples);

        constexpr double meaningfulGain = 1.012; // ignore ~1% metric/noise wander
        FocusSample best = center;
        if (left.score > best.score * meaningfulGain) best = left;
        if (right.score > best.score * meaningfulGain) best = right;

        // If the current position is already a local maximum, keep it (or only
        // fine-tune nearby).  This is the crucial fail-safe missing in v0.47.
        if (best.position == center.position) {
            const int fineHalf = std::max(fineStep * 4, coarseStep);
            auto fine = scan(cam, foc, effective, std::max(minPos, originalPosition - fineHalf), std::min(maxPos, originalPosition + fineHalf), fineStep, cb, cancel, frameCb);
            samples.insert(samples.end(), fine.begin(), fine.end());
            if (cancel && cancel()) return cancelAndRestore(samples);
            for (const auto &s : fine) if (s.score > best.score * 1.003) best = s;
            if (!foc.moveAbsolute(best.position, &err) || !waitForFocuserIdle(foc, cancel, err)) return failAndRestore(err, samples);
            out.success = true; out.bestPosition = best.position; out.bestScore = best.score; out.samples = samples;
            out.message = QString("Scene autofocus: starting focus was already near the local contrast maximum; settled at %1 (AF exposure %2 s)").arg(best.position).arg(effective.exposureSec, 0, 'g', 5);
            return out;
        }

        const int direction = best.position > originalPosition ? +1 : -1;
        int p = best.position + direction * coarseStep;
        int worse = 0;
        while (p >= minPos && p <= maxPos && !(cancel && cancel())) {
            FocusSample s = sampleAt(p);
            if (s.score > best.score * 1.004) { best = s; worse = 0; }
            else if (s.score < best.score * 0.996) { ++worse; }
            else { ++worse; }
            if (worse >= 2) break;
            p += direction * coarseStep;
        }
        if (cancel && cancel()) return cancelAndRestore(samples);

        // A best score at the permitted boundary means the peak was never
        // bracketed.  Do not "trust" the last position and defocus the scope.
        if (best.position <= minPos + coarseStep / 2 || best.position >= maxPos - coarseStep / 2) {
            return failAndRestore(QString("Scene focus peak was not bracketed inside ±%1 steps (best metric kept improving toward the boundary); increase range only if needed").arg(half), samples);
        }

        const int fineHalf = std::max(coarseStep, fineStep * 5);
        auto fine = scan(cam, foc, effective, std::max(minPos, best.position - fineHalf), std::min(maxPos, best.position + fineHalf), fineStep, cb, cancel, frameCb);
        samples.insert(samples.end(), fine.begin(), fine.end());
        if (cancel && cancel()) return cancelAndRestore(samples);
        for (const auto &s : fine) if (s.score > best.score) best = s;
        if (best.score < center.score * 1.008) return failAndRestore("Scene focus metric did not improve enough to justify moving away from the original focus", samples);
        if (!foc.moveAbsolute(best.position, &err) || !waitForFocuserIdle(foc, cancel, err)) return failAndRestore(err, samples);
        out.success = true; out.bestPosition = best.position; out.bestScore = best.score; out.samples = samples;
        out.message = QString("Scene autofocus completed with a bracketed local contrast peak at %1 (AF exposure %2 s)").arg(best.position).arg(effective.exposureSec, 0, 'g', 5);
        return out;
    }

    auto coarse = scan(cam, foc, effective, std::max(0, originalPosition - half), originalPosition + half, coarseStep, cb, cancel, frameCb);
    if (cancel && cancel()) return cancelAndRestore(coarse);
    if (coarse.size() < 3) return failAndRestore("Not enough coarse focus samples", coarse);
    if (effective.mode == AutofocusMode::Stars) {
        const int maxStars = std::max_element(coarse.begin(), coarse.end(), [](const auto &a, const auto &b){ return a.detectedStars < b.detectedStars; })->detectedStars;
        if (maxStars < std::max(1, effective.minStars)) return failAndRestore(QString("No suitable stars detected (need at least %1 per focus frame). Use Scene autofocus for daytime/structured targets or increase exposure/gain.").arg(std::max(1, effective.minStars)), coarse);
    }
    auto bestIt = std::max_element(coarse.begin(), coarse.end(), [](const auto &a, const auto &b){ return a.score < b.score; });
    if (bestIt == coarse.end() || bestIt->score <= 0.0) return failAndRestore("No usable focus contrast detected", coarse);
    FocusSample coarseBest = *bestIt;
    const int fineHalf = std::max(coarseStep, fineStep * 4);
    auto fine = scan(cam, foc, effective, std::max(0, coarseBest.position - fineHalf), coarseBest.position + fineHalf, fineStep, cb, cancel, frameCb);
    out.samples = coarse; out.samples.insert(out.samples.end(), fine.begin(), fine.end());
    if (cancel && cancel()) return cancelAndRestore(out.samples);
    FocusSample chosen = coarseBest;
    if (!fine.empty()) { auto candidate = std::max_element(fine.begin(), fine.end(), [](const auto &a, const auto &b){ return a.score < b.score; }); if (candidate != fine.end() && candidate->score > chosen.score * 1.005) chosen = *candidate; }
    if (chosen.score <= 0.0) return failAndRestore("No usable focus metric peak detected", out.samples);
    if (!foc.moveAbsolute(chosen.position, &err) || !waitForFocuserIdle(foc, cancel, err)) return failAndRestore(err, out.samples);
    out.success = true; out.bestPosition = chosen.position; out.bestScore = chosen.score; out.message = "Autofocus completed"; return out;
}

} // namespace oas
