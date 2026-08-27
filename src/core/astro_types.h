#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <opencv2/core.hpp>
#include <optional>
#include <vector>

namespace oas {

enum class ConnectionState { Disconnected, Connecting, Connected, Error };
enum class DeviceKind { Camera, Mount, Focuser, Guider, Solver, Unknown };
enum class AutofocusMode { Stars, Planet, Bahtinov };
enum class GuideDirection { North, South, East, West };

struct EquatorialCoord {
    double raDeg{0.0};
    double decDeg{0.0};
};

struct ObserverLocation {
    double latitudeDeg{0.0};
    double longitudeDeg{0.0};
    double elevationM{0.0};
};

struct TelescopeProfile {
    QString name{"Default"};

    // Main imaging optical train. focalLengthMm is the effective focal length
    // after any reducer/Barlow; apertureMm is the clear entrance aperture.
    QString opticalDesign{"reflector"};
    double apertureMm{100.0};
    double centralObstructionMm{0.0};
    double focalLengthMm{400.0};
    double pixelSizeUm{4.8};
    int sensorWidthPx{1920};
    int sensorHeightPx{1080};

    // Independent guide optical train. A separate guide camera can be connected
    // at the same time as the main camera.
    QString guideScopeName{"Guide scope"};
    double guideApertureMm{30.0};
    double guideFocalLengthMm{120.0};
    double guidePixelSizeUm{3.75};
    int guideSensorWidthPx{1280};
    int guideSensorHeightPx{960};

    ObserverLocation observer{};

    double focalRatio() const {
        return apertureMm > 0.0 ? focalLengthMm / apertureMm : 0.0;
    }
    double arcsecPerPixel() const {
        return focalLengthMm > 0.0 ? 206.265 * pixelSizeUm / focalLengthMm : 0.0;
    }
    double guideFocalRatio() const {
        return guideApertureMm > 0.0 ? guideFocalLengthMm / guideApertureMm : 0.0;
    }
    double guideArcsecPerPixel() const {
        return guideFocalLengthMm > 0.0 ? 206.265 * guidePixelSizeUm / guideFocalLengthMm : 0.0;
    }
};

struct ExposureRequest {
    double exposureSec{1.0};
    int binX{1};
    int binY{1};
    int gain{0};
    int offset{0};
    cv::Rect roi{};
    bool saveRaw{false};
    QString savePath{};
};

struct CameraFrame {
    QString id;
    cv::Mat image;
    QDateTime capturedUtc;
    double exposureSec{0.0};
    int gain{0};
    int binX{1};
    int binY{1};
    QString source;
};

struct DetectedStar {
    cv::Point2d positionPx;
    double flux{0.0};
    double peak{0.0};
    double hfrPx{0.0};
};

struct SolveHint {
    std::optional<double> raDeg;
    std::optional<double> decDeg;
    std::optional<double> fovDeg;
    double searchRadiusDeg{20.0};
};

struct AdaptiveSolveRequest {
    ExposureRequest exposure{};
    int maxAttempts{3};
    int stackFrames{3};
    int finalStackFrames{5};
    int minStarsForSolve{20};
    double exposureGrowth{1.35};
    double maxSingleExposureSec{3.0};
    bool registerFrames{true};
    bool equalizeBackground{true};
    bool useMountHint{true};
    SolveHint hint{};
};

struct SolveResult {
    bool success{false};
    double raDeg{0.0};
    double decDeg{0.0};
    double rotationDeg{0.0};
    double scaleArcsecPerPx{0.0};
    int matchedStars{0};
    double rmsArcsec{0.0};
    QString catalog;
    QString message;
    std::vector<DetectedStar> imageStars;
};

struct MountStatus {
    ConnectionState connection{ConnectionState::Disconnected};
    EquatorialCoord coordinate{};
    // False when a backend is connected but has not yet established a valid
    // sky-coordinate model (for example native EQDrive before the first Sync).
    bool coordinateValid{true};
    bool tracking{false};
    bool slewing{false};
    bool parked{false};
    QString pierSide{"unknown"};
};

struct FocuserStatus {
    ConnectionState connection{ConnectionState::Disconnected};
    int position{0};
    bool moving{false};
    std::optional<double> temperatureC;
};

struct FocusSample {
    int position{0};
    double score{0.0};
    double spread{0.0};
};

struct AutofocusRequest {
    AutofocusMode mode{AutofocusMode::Stars};
    int rangeSteps{1600};
    int coarseStep{200};
    int fineStep{40};
    int framesPerPosition{3};
    int settleMs{400};
    bool autoPlanetRoi{true};
};

struct AutofocusResult {
    bool success{false};
    int bestPosition{0};
    double bestScore{0.0};
    QString message;
    std::vector<FocusSample> samples;
};

struct GuidingStatus {
    bool active{false};
    EquatorialCoord target{};
    double raErrorArcsec{0.0};
    double decErrorArcsec{0.0};
    double rmsArcsec{0.0};
};

struct PolarSample {
    SolveResult solve;
    double mountRaDeg{0.0};
};

struct PolarAlignmentResult {
    bool success{false};
    double axisRaDeg{0.0};
    double axisDecDeg{90.0};
    double totalErrorArcmin{0.0};
    double altitudeAdjustmentArcmin{0.0};
    double azimuthAdjustmentArcmin{0.0};
    QString message;
};

struct SessionTarget {
    QString name;
    EquatorialCoord coordinate;
    double exposureSec{30.0};
    int repeats{1};
    QString filter;
};

struct SessionStatus {
    QString id;
    QString name;
    bool active{false};
    int targetIndex{0};
    int targetCount{0};
    int completedFrames{0};
    QString state{"idle"};
};

inline QJsonObject coordToJson(const EquatorialCoord &c) {
    return {{"raDeg", c.raDeg}, {"decDeg", c.decDeg}};
}

inline QJsonObject solveToJson(const SolveResult &s) {
    return {{"success", s.success}, {"raDeg", s.raDeg}, {"decDeg", s.decDeg},
            {"rotationDeg", s.rotationDeg}, {"scaleArcsecPerPx", s.scaleArcsecPerPx},
            {"matchedStars", s.matchedStars}, {"rmsArcsec", s.rmsArcsec},
            {"catalog", s.catalog}, {"message", s.message}};
}

inline QJsonObject autofocusToJson(const AutofocusResult &r) {
    QJsonArray samples;
    for (const auto &s : r.samples)
        samples.append(QJsonObject{{"position", s.position}, {"score", s.score}, {"spread", s.spread}});
    return {{"success", r.success}, {"bestPosition", r.bestPosition}, {"bestScore", r.bestScore},
            {"message", r.message}, {"samples", samples}};
}

} // namespace oas
