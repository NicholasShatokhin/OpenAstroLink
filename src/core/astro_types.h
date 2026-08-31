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
enum class AutofocusMode { Stars, Scene, Planet, Bahtinov };
enum class GuideDirection { North, South, East, West };
enum class TrackingRate { Sidereal, Lunar, Solar };
enum class EquatorialFrame { J2000, JNow };
enum class MountGeometryType { GermanEquatorial, ForkEquatorial, AltAzimuth, AltAzimuthDerotator, EquatorialPlatform, CustomTwoAxis };


inline QString trackingRateName(TrackingRate rate) {
    switch (rate) {
    case TrackingRate::Lunar: return "lunar";
    case TrackingRate::Solar: return "solar";
    case TrackingRate::Sidereal: default: return "sidereal";
    }
}
inline TrackingRate trackingRateFromString(const QString &text, TrackingRate fallback = TrackingRate::Sidereal) {
    const QString s = text.trimmed().toLower();
    if (s == "lunar" || s == "moon") return TrackingRate::Lunar;
    if (s == "solar" || s == "sun") return TrackingRate::Solar;
    if (s == "sidereal" || s == "stars") return TrackingRate::Sidereal;
    return fallback;
}

struct MountGeometryConfig {
    MountGeometryType type{MountGeometryType::GermanEquatorial};
    // Mechanical axis orientation. +1 means increasing canonical axis angle
    // increases the controller-reported axis coordinate. These defaults match
    // the HIL-qualified EQDrive installation used during v0.2.10.25 tests:
    // RA+ => decreasing hour-axis coordinate; DEC+ => increasing DEC axis.
    int axis1Sign{1};
    int axis2Sign{1};
    QString preferredPierSide{"east"};
    // Raw controller coordinates of the repeatable mechanical Home pose.
    // For a GEM this is the conventional counterweight-down pose with the
    // telescope/DEC axis aligned with the celestial pole.  When customHome
    // and autoHomeSync are enabled, Core can restore a usable sky model on
    // connect without asking for a manual near-pole Sync every session.
    double homeAxis1Deg{0.0};
    double homeAxis2Deg{0.0};
    bool customHome{false};
    bool autoHomeSync{false};
    double homeToleranceDeg{2.0};
    double parkAxis1Deg{0.0};
    double parkAxis2Deg{0.0};
    bool customPark{false};
    bool allowAutomaticPierFlip{false};
    // Coordinate-model ABI for direct Sky-Watcher/EQDrive Motor Controller mounts.
    // v6 (OpenAstroLink 0.2.10.44) keeps the controller counts present at
    // direct-MC connect as the session Home/Park reference, exposed as
    // Axis1=0°, Axis2=0°. Sky coordinates are mapped through a telescope
    // direction vector rotated into the polar-aligned mount frame (the same
    // geometry used by mature SkyWatcher/INDI implementations), instead of
    // guessing an hour-angle phase. Axis1 is mount-frame azimuth; Axis2 is
    // signed polar distance. Native serial EQDrive and direct UDP/11880 use
    // this model; high-level SynScan hand-controller/App backends remain
    // RA/DEC backends.
    int nativeCoordinateModelVersion{1};
    // When an ASCOM backend exposes a valid observatory site, prefer that site
    // as the authoritative source for OAL coordinate conversions. This avoids
    // maintaining two independently editable locations for EQMOD.
    bool preferBackendSite{true};
    // User-controlled sky-separation safety envelope for coordinate GOTO.
    // The legacy field name is kept for wire/settings compatibility, but from
    // v0.2.10.38 its value is interpreted as ANGULAR SKY SEPARATION, not raw
    // motor-axis rotation. Near the celestial pole a few degrees on the sky can
    // legitimately require a large RA-axis rotation, so the raw transport keeps
    // a separate hard mechanical ceiling of 180 deg.
    double maxGotoAxisDeltaDeg{15.0};
};

struct MechanicalAxes {
    double axis1Deg{0.0};
    double axis2Deg{0.0};
    bool valid{false};
};

struct EquatorialCoord {
    double raDeg{0.0};
    double decDeg{0.0};
    EquatorialFrame frame{EquatorialFrame::J2000};
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
    MountGeometryConfig mount{};

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
    // Durable camera artifact stored by the node/driver. For DSLR RAW capture
    // this points at the original CR2/CR3; it is distinct from image, which may
    // be an operational preview used by UI/autofocus/plate solving.
    QString scienceFilePath;
    // Native raw-sensor metadata used only for preview processing.
    bool bayerEncoded{false};
    QString bayerPattern;
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
    double maxCapturePhaseSec{120.0};
    bool registerFrames{true};
    bool equalizeBackground{true};
    bool useMountHint{true};
    SolveHint hint{};
};

struct SolveResult {
    bool success{false};
    double raDeg{0.0};
    double decDeg{0.0};
    EquatorialFrame frame{EquatorialFrame::J2000};
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
    MechanicalAxes axes{};
    QString geometryType{"unknown"};
    QJsonObject diagnostics{};
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
    int detectedStars{0};
};

struct AutofocusRequest {
    AutofocusMode mode{AutofocusMode::Stars};
    int rangeSteps{1600};
    int coarseStep{200};
    int fineStep{40};
    int framesPerPosition{3};
    int settleMs{400};
    bool autoPlanetRoi{true};
    // Camera settings used for every focus sample. Scene autofocus is intended
    // for daytime terrestrial targets, Moon/planet structure and other
    // non-stellar images; star autofocus additionally enforces minStars.
    double exposureSec{0.05};
    int gain{0};
    int minStars{3};
};

enum class BayerPattern { Auto, RGGB, BGGR, GRBG, GBRG };

inline QString bayerPatternName(BayerPattern p) {
    switch (p) {
    case BayerPattern::RGGB: return "RGGB";
    case BayerPattern::BGGR: return "BGGR";
    case BayerPattern::GRBG: return "GRBG";
    case BayerPattern::GBRG: return "GBRG";
    default: return "AUTO";
    }
}

inline BayerPattern bayerPatternFromString(const QString &text, BayerPattern fallback = BayerPattern::Auto) {
    const QString s = text.trimmed().toUpper();
    if (s == "RGGB") return BayerPattern::RGGB;
    if (s == "BGGR") return BayerPattern::BGGR;
    if (s == "GRBG") return BayerPattern::GRBG;
    if (s == "GBRG") return BayerPattern::GBRG;
    if (s == "AUTO" || s.isEmpty()) return BayerPattern::Auto;
    return fallback;
}

struct LiveViewRequest {
    double exposureSec{0.05};
    int gain{0};
    int binX{1};
    int binY{1};
    double targetFps{5.0};
    // Preview-only color reconstruction. Science frames remain untouched.
    // AUTO uses driver-published CFA metadata; explicit patterns make the
    // feature camera-vendor-neutral even when a driver cannot identify CFA.
    bool debayer{false};
    BayerPattern bayerPattern{BayerPattern::Auto};
    // Optional raw live-stream recording. SER is written on the node before
    // preview-only debayer/stretch so science pixels are preserved.
    bool recordSer{false};
    QString serPath{};
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
    return {{"raDeg", c.raDeg}, {"decDeg", c.decDeg}, {"coordinateFrame", c.frame == EquatorialFrame::JNow ? "JNOW" : "J2000"}};
}

inline QJsonObject solveToJson(const SolveResult &s) {
    return {{"success", s.success}, {"raDeg", s.raDeg}, {"decDeg", s.decDeg},
            {"coordinateFrame", s.frame == EquatorialFrame::JNow ? "JNOW" : "J2000"}, {"rotationDeg", s.rotationDeg}, {"scaleArcsecPerPx", s.scaleArcsecPerPx},
            {"matchedStars", s.matchedStars}, {"rmsArcsec", s.rmsArcsec},
            {"catalog", s.catalog}, {"message", s.message}};
}

inline QJsonObject autofocusToJson(const AutofocusResult &r) {
    QJsonArray samples;
    for (const auto &s : r.samples)
        samples.append(QJsonObject{{"position", s.position}, {"score", s.score}, {"spread", s.spread}, {"detectedStars", s.detectedStars}});
    return {{"success", r.success}, {"bestPosition", r.bestPosition}, {"bestScore", r.bestScore},
            {"message", r.message}, {"samples", samples}};
}

} // namespace oas
