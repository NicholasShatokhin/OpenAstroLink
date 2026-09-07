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
    const int w = std::max(32, int(std::lround(src.cols * 0.92)));
    const int h = std::max(32, int(std::lround(src.rows * 0.92)));
    const int x = std::max(0, (src.cols - w) / 2);
    const int y = std::max(0, (src.rows - h) / 2);
    return src(cv::Rect(x, y, std::min(w, src.cols - x), std::min(h, src.rows - y)));
}

struct FrameLevels {
    double p50{0.0};
    double p95{0.0};
    double p99{0.0};
    double p995{0.0};
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
    out.p50 = pct(0.50); out.p95 = pct(0.95); out.p99 = pct(0.99); out.p995 = pct(0.995);
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
    if (lv.p995 < 0.012 || lv.highClip > 0.30) return 0.0;

    // A global whole-frame Sobel average was too easily diluted by the dark
    // background of a sparse daylight/Moon target (and could be dominated by a
    // fixed window/edge).  Measure physical-scale high-frequency structure per
    // tile, reject flat/dark tiles, and aggregate the strongest useful quarter.
    // Exposure is fixed for the entire sweep, so this remains a true
    // contrast-detect focus metric rather than an auto-stretched image score.
    cv::Mat smooth; cv::GaussianBlur(f, smooth, {0, 0}, 0.90);
    cv::Mat gx, gy, mag, lap;
    cv::Sobel(smooth, gx, CV_32F, 1, 0, 3);
    cv::Sobel(smooth, gy, CV_32F, 0, 1, 3);
    cv::magnitude(gx, gy, mag);
    cv::Laplacian(smooth, lap, CV_32F, 3);

    constexpr int tilesX = 6, tilesY = 4;
    std::vector<double> tileScores;
    tileScores.reserve(tilesX * tilesY);
    for (int ty = 0; ty < tilesY; ++ty) {
        const int y0 = ty * smooth.rows / tilesY;
        const int y1 = (ty + 1) * smooth.rows / tilesY;
        for (int tx = 0; tx < tilesX; ++tx) {
            const int x0 = tx * smooth.cols / tilesX;
            const int x1 = (tx + 1) * smooth.cols / tilesX;
            if (x1 - x0 < 8 || y1 - y0 < 8) continue;
            const cv::Rect r(x0, y0, x1 - x0, y1 - y0);
            cv::Scalar meanI, stdI, meanMag, stdMag, meanLap, stdLap;
            cv::meanStdDev(smooth(r), meanI, stdI);
            // Reject tiles that contain essentially no scene information. A
            // moderately dark but structured silhouette remains eligible.
            if (meanI[0] < 0.0035 && stdI[0] < 0.0060) continue;
            if (meanI[0] > 0.985 && stdI[0] < 0.0100) continue;
            cv::meanStdDev(mag(r), meanMag, stdMag);
            cv::meanStdDev(lap(r), meanLap, stdLap);
            const double textureWeight = std::clamp(stdI[0] / 0.020, 0.25, 1.0);
            const double raw = meanMag[0] + 0.60 * stdMag[0] + 0.38 * stdLap[0];
            if (raw > 0.0 && std::isfinite(raw)) tileScores.push_back(raw * textureWeight);
        }
    }
    if (tileScores.empty()) return 0.0;
    std::sort(tileScores.begin(), tileScores.end(), std::greater<double>());
    const std::size_t keep = std::clamp<std::size_t>((tileScores.size() + 3) / 4, 2, 6);
    double sum = 0.0;
    for (std::size_t i = 0; i < std::min(keep, tileScores.size()); ++i) sum += tileScores[i];
    return 1.0e4 * sum / double(std::min(keep, tileScores.size()));
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
    // Scene AF does not need a photographically pretty frame. It needs enough
    // unsaturated structure for a repeatable contrast metric.  The old meter
    // watched global P50/P95, so a small bright target on a black field drove
    // 0.05 -> 0.2 -> 0.8 -> 3.2 -> 10 s even when the target was already very
    // usable.  Meter the bright tail instead and cap acquisition latency.
    double exposure = std::clamp(effective.exposureSec, 0.00005, 2.0);
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (cancel && cancel()) break;
        ExposureRequest req; req.exposureSec = exposure; req.gain = std::max(0, effective.gain); req.saveRaw = false;
        CameraFrame f; QString err; if (!cam.capture(req, f, &err) || f.image.empty()) break;
        if (frameCb) frameCb(f, -1);
        const FrameLevels lv = frameLevels(sceneCrop(f.image));
        const double controlled = (lv.highClip > 0.006 || lv.p995 > 0.97) ? std::max(0.004, lv.p99) : std::max(0.004, lv.p995);
        if (lv.highClip <= 0.006 && controlled >= 0.16 && controlled <= 0.88) break;
        double factor = std::pow(0.58 / controlled, 0.62);
        if (lv.highClip > 0.010 || lv.p995 > 0.98) factor = std::min(factor, 0.55);
        if (controlled < 0.07) factor = std::max(factor, 1.70);
        factor = std::clamp(factor, 0.45, 2.50);
        const double next = std::clamp(exposure * factor, 0.00005, 2.0);
        if (std::abs(next / exposure - 1.0) < 0.10) break;
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
        // Two frames are enough to estimate repeatability for scene contrast;
        // using the UI default of three at every point made a near-focused run
        // need ~40 captures.  Keep the user setting for star/planet modes.
        AutofocusRequest sceneReq = effective;
        sceneReq.framesPerPosition = std::clamp(effective.framesPerPosition, 1, 2);
        std::vector<FocusSample> samples;
        auto sampleAt = [&](int p) -> FocusSample {
            p = std::clamp(p, 0, std::numeric_limits<int>::max());
            auto v = scan(cam, foc, sceneReq, p, p, 1, cb, cancel, frameCb);
            if (v.empty()) return FocusSample{p, 0.0, 0.0, 0};
            samples.push_back(v.front()); return v.front();
        };
        auto uncertainty = [](const FocusSample &a, const FocusSample &b) {
            const double scale = std::max({1.0, std::abs(a.score), std::abs(b.score)});
            return std::max(0.020 * scale, 1.6 * (a.spread + b.spread));
        };
        auto clearlyBetter = [&](const FocusSample &a, const FocusSample &b) {
            return a.score > b.score + uncertainty(a, b);
        };
        auto clearlyWorse = [&](const FocusSample &a, const FocusSample &b) {
            return a.score + uncertainty(a, b) < b.score;
        };
        auto chooseBest = [&](const FocusSample &seed) {
            FocusSample best = seed;
            for (const auto &s : samples) if (clearlyBetter(s, best)) best = s;
            return best;
        };
        auto settleAt = [&](const FocusSample &candidate, const FocusSample &center, const QString &reason) -> AutofocusResult {
            FocusSample chosen = candidate;
            if (!clearlyBetter(candidate, center)) chosen = center;
            if (!foc.moveAbsolute(chosen.position, &err) || !waitForFocuserIdle(foc, cancel, err)) return failAndRestore(err, samples);
            if (cancel && cancel()) return cancelAndRestore(samples);

            // Verify a proposed move at the final position.  If the apparent
            // improvement does not repeat, retain the already-good starting
            // focus rather than chasing metric noise.
            if (chosen.position != center.position) {
                FocusSample verify = sampleAt(chosen.position);
                if (cancel && cancel()) return cancelAndRestore(samples);
                if (!clearlyBetter(verify, center)) {
                    if (!foc.moveAbsolute(center.position, &err) || !waitForFocuserIdle(foc, cancel, err)) return failAndRestore(err, samples);
                    chosen = center;
                    out.message = QString("Scene autofocus: candidate peak was not repeatable; retained starting focus %1 (AF exposure %2 s)").arg(center.position).arg(effective.exposureSec, 0, 'g', 5);
                } else {
                    chosen = verify;
                    out.message = reason.arg(chosen.position).arg(effective.exposureSec, 0, 'g', 5);
                }
            } else {
                out.message = QString("Scene autofocus: starting focus %1 is already within the repeatable local maximum; no unnecessary move (AF exposure %2 s)").arg(center.position).arg(effective.exposureSec, 0, 'g', 5);
            }
            out.success = true; out.bestPosition = chosen.position; out.bestScore = chosen.score; out.samples = samples;
            return out;
        };

        const int minPos = std::max(0, originalPosition - half), maxPos = originalPosition + half;
        const FocusSample center = sampleAt(originalPosition);
        if (cancel && cancel()) return cancelAndRestore(samples);
        if (center.score <= 0.0) return failAndRestore(QString("No usable scene structure at the starting position (AF metered exposure %1 s)").arg(effective.exposureSec, 0, 'g', 5), samples);

        const FocusSample left = originalPosition - coarseStep >= minPos ? sampleAt(originalPosition - coarseStep) : center;
        const FocusSample right = originalPosition + coarseStep <= maxPos ? sampleAt(originalPosition + coarseStep) : center;
        if (cancel && cancel()) return cancelAndRestore(samples);

        const bool leftBetter = left.position != center.position && clearlyBetter(left, center);
        const bool rightBetter = right.position != center.position && clearlyBetter(right, center);

        if (!leftBetter && !rightBetter) {
            // Center is already competitive.  A compact ±2*fine refinement is
            // enough; v0.2.10.53 scanned ±coarse at every fine step here.
            for (int d : {-2, -1, 1, 2}) {
                const int p = originalPosition + d * fineStep;
                if (p >= minPos && p <= maxPos) sampleAt(p);
                if (cancel && cancel()) return cancelAndRestore(samples);
            }
            const FocusSample best = chooseBest(center);
            return settleAt(best, center, QString("Scene autofocus completed with a repeatable compact local peak at %1 (AF exposure %2 s)"));
        }

        FocusSample best = leftBetter && (!rightBetter || clearlyBetter(left, right)) ? left : right;
        const int direction = best.position > originalPosition ? +1 : -1;
        bool bracketed = false;
        // At most four additional coarse probes: quick enough for daylight
        // alignment while still able to find a peak within a useful range.
        for (int probe = 0; probe < 4; ++probe) {
            const int p = best.position + direction * coarseStep;
            if (p < minPos || p > maxPos) break;
            const FocusSample s = sampleAt(p);
            if (cancel && cancel()) return cancelAndRestore(samples);
            if (clearlyBetter(s, best)) { best = s; continue; }
            if (clearlyWorse(s, best) || !clearlyBetter(s, best)) { bracketed = true; break; }
        }
        if (!bracketed) return failAndRestore(QString("Scene focus peak was not bracketed inside ±%1 steps; starting focus retained").arg(half), samples);

        // Five-point refinement around the bracketed coarse best, rather than a
        // broad eleven-point fine sweep.
        for (int d : {-2, -1, 0, 1, 2}) {
            const int p = best.position + d * fineStep;
            if (p < minPos || p > maxPos) continue;
            if (d == 0) continue; // already sampled at the coarse best
            sampleAt(p);
            if (cancel && cancel()) return cancelAndRestore(samples);
        }
        best = chooseBest(best);
        return settleAt(best, center, QString("Scene autofocus completed with a bracketed repeatable local peak at %1 (AF exposure %2 s)"));
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
