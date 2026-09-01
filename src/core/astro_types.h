#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <opencv2/core.hpp>
#include <optional>
#include <vector>
#include <algorithm>

namespace oas {

enum class ConnectionState { Disconnected, Connecting, Connected, Error };
enum class DeviceKind {
    Camera, Mount, Focuser, Guider, Solver,
    FilterWheel, Rotator, Dome, Weather, Gps, Power,
    CoverCalibrator, SafetyMonitor, Switch, Unknown
};
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
    // v7 (OpenAstroLink 0.2.10.45) uses EQMOD-style mechanical GEM coordinates.
    // The repeatable counterweight-down polar Home is Axis1=0°, Axis2=0° and,
    // in the northern hemisphere, corresponds to HA=-6h, Dec=+90° on the west
    // pointing branch.  Target branch/side-of-pier is determined by hour angle;
    // native serial EQDrive and direct UDP/11880 share this exact Core model.
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
    int offset{0};
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
    int offset{0};
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
    // Optional hardware ROI in binned sensor coordinates. The origin is kept
    // as acquisition provenance because dark/flat calibration must use the
    // same physical sensor region when a planetary tracker moves the ROI.
    cv::Rect roi{};
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

// Legacy session target retained for wire/API compatibility. New scheduler
// code converts this into an ObservationPlan containing DSO FITS blocks.
struct SessionTarget {
    QString name;
    EquatorialCoord coordinate;
    double exposureSec{30.0};
    int repeats{1};
    QString filter;
};

enum class ObservationMode { DsoFits, PlanetarySer };

inline QString observationModeName(ObservationMode mode) {
    return mode == ObservationMode::PlanetarySer ? "planetary-ser" : "dso-fits";
}
inline ObservationMode observationModeFromString(const QString &text, ObservationMode fallback = ObservationMode::DsoFits) {
    const QString s = text.trimmed().toLower();
    if (s == "dso-fits" || s == "dso" || s == "fits") return ObservationMode::DsoFits;
    if (s == "planetary-ser" || s == "planetary" || s == "ser") return ObservationMode::PlanetarySer;
    return fallback;
}

struct RecenterPolicy {
    bool beforeFirstFrame{true};
    int everyNFrames{0};
    double toleranceArcmin{2.0};
    int maxAttempts{2};
    double solveExposureSec{1.0};
};

struct SchedulerAutofocusPolicy {
    bool beforeFirstFrame{true};
    int everyNFrames{0};
    AutofocusRequest request{};
};

struct DsoFitsBlock {
    ExposureRequest exposure{};
    int frameCount{1};
    QString filter;
    RecenterPolicy recenter{};
    SchedulerAutofocusPolicy autofocus{};
};

struct PlanetaryAutofocusPolicy {
    bool beforeFirstRun{true};
    int everyNRuns{0};
    AutofocusRequest request{AutofocusMode::Planet,1200,160,32,3,300,true,0.01,100,1};
};

struct PlanetaryTrackingPolicy {
    // Fast loop: keep the target near the ROI centre by moving the hardware ROI
    // without moving the mount. The SER dimensions stay constant.
    bool allowRoiShift{true};
    int roiShiftThresholdPx{48};
    // Slow loop: when drift is too large for ROI-only tracking, optionally
    // nudge the mount using a two-axis image-response calibration. Disabled by
    // default until the active mount backend is HIL-qualified for micro-slews.
    bool mountCorrections{false};
    int mountCorrectionThresholdPx{140};
    bool autoCalibrateMount{true};
    double calibrationArcsec{20.0};
    double maxMountCorrectionArcsec{30.0};
    int mountSettleMs{700};
    int lostTargetFrames{8};
};

struct PlanetarySerBlock {
    LiveViewRequest stream{};
    int serRuns{1};
    double durationSec{120.0};
    double pauseSec{2.0};
    TrackingRate trackingRate{TrackingRate::Sidereal};
    // Initial hardware ROI in binned sensor coordinates. width/height <= 0
    // selects a conservative 640x480 window centred on the acquired planet.
    int roiWidth{0};
    int roiHeight{0};
    int roiX{0};
    int roiY{0};
    PlanetaryAutofocusPolicy autofocus{};
    PlanetaryTrackingPolicy tracking{};
};

struct ObservationBlock {
    QString id;
    QString name;
    EquatorialCoord coordinate{};
    ObservationMode mode{ObservationMode::DsoFits};
    DsoFitsBlock dso{};
    PlanetarySerBlock planetary{};
};

struct ObservationPlan {
    QString id;
    QString name;
    std::vector<ObservationBlock> blocks;
};

struct SessionStatus {
    QString id;
    QString name;
    bool active{false};
    int blockIndex{0};
    int blockCount{0};
    int completedFrames{0};
    int currentBlockCompletedFrames{0};
    QString currentBlockId;
    QString currentBlockName;
    QString currentStep{"idle"};
    QString currentOperationId;
    QString lastError;
    QString state{"idle"};

    // v0.2.10.45 and earlier clients used targetIndex/targetCount. Keep mirror
    // fields until the public session API is versioned independently.
    int targetIndex{0};
    int targetCount{0};
};

inline QJsonObject exposureRequestToJson(const ExposureRequest &r) {
    QJsonObject o{{"exposureSec",r.exposureSec},{"gain",r.gain},{"offset",r.offset},
                  {"binX",r.binX},{"binY",r.binY},{"saveRaw",r.saveRaw}};
    if (!r.savePath.isEmpty()) o["savePath"] = r.savePath;
    if (r.roi.width > 0 && r.roi.height > 0)
        o["roi"] = QJsonObject{{"x",r.roi.x},{"y",r.roi.y},{"width",r.roi.width},{"height",r.roi.height}};
    return o;
}

inline ExposureRequest exposureRequestFromJson(const QJsonObject &o, const ExposureRequest &fallback = {}) {
    ExposureRequest r=fallback;
    r.exposureSec=o.value("exposureSec").toDouble(r.exposureSec);
    r.gain=o.value("gain").toInt(r.gain);r.offset=o.value("offset").toInt(r.offset);
    r.binX=o.value("binX").toInt(r.binX);r.binY=o.value("binY").toInt(r.binY);
    r.saveRaw=o.value("saveRaw").toBool(r.saveRaw);r.savePath=o.value("savePath").toString(r.savePath);
    const auto roi=o.value("roi").toObject();
    if(!roi.isEmpty())r.roi=cv::Rect(roi.value("x").toInt(),roi.value("y").toInt(),roi.value("width").toInt(),roi.value("height").toInt());
    return r;
}

inline QJsonObject observationBlockToJson(const ObservationBlock &b) {
    QJsonObject root{{"id",b.id},{"name",b.name},{"raDeg",b.coordinate.raDeg},{"decDeg",b.coordinate.decDeg},
                     {"coordinateFrame",b.coordinate.frame==EquatorialFrame::JNow?"JNOW":"J2000"},{"mode",observationModeName(b.mode)}};
    if(b.mode==ObservationMode::DsoFits){
        const auto &d=b.dso;
        root["dso"]=QJsonObject{{"exposure",exposureRequestToJson(d.exposure)},{"frameCount",d.frameCount},{"filter",d.filter},
            {"recenter",QJsonObject{{"beforeFirstFrame",d.recenter.beforeFirstFrame},{"everyNFrames",d.recenter.everyNFrames},
                                     {"toleranceArcmin",d.recenter.toleranceArcmin},{"maxAttempts",d.recenter.maxAttempts},{"solveExposureSec",d.recenter.solveExposureSec}}},
            {"autofocus",QJsonObject{{"beforeFirstFrame",d.autofocus.beforeFirstFrame},{"everyNFrames",d.autofocus.everyNFrames},
                                      {"mode",d.autofocus.request.mode==AutofocusMode::Planet?"planet":d.autofocus.request.mode==AutofocusMode::Scene?"scene":d.autofocus.request.mode==AutofocusMode::Bahtinov?"bahtinov":"stars"},
                                      {"rangeSteps",d.autofocus.request.rangeSteps},{"coarseStep",d.autofocus.request.coarseStep},{"fineStep",d.autofocus.request.fineStep},
                                      {"framesPerPosition",d.autofocus.request.framesPerPosition},{"settleMs",d.autofocus.request.settleMs},
                                      {"exposureSec",d.autofocus.request.exposureSec},{"gain",d.autofocus.request.gain},{"minStars",d.autofocus.request.minStars}}}};
    }else{
        const auto &p=b.planetary;
        root["planetary"]=QJsonObject{{"serRuns",p.serRuns},{"durationSec",p.durationSec},{"pauseSec",p.pauseSec},{"trackingRate",trackingRateName(p.trackingRate)},
            {"stream",QJsonObject{{"exposureSec",p.stream.exposureSec},{"gain",p.stream.gain},{"offset",p.stream.offset},{"binX",p.stream.binX},{"binY",p.stream.binY},{"targetFps",p.stream.targetFps},{"debayer",p.stream.debayer},{"bayerPattern",bayerPatternName(p.stream.bayerPattern)}}},
            {"roi",QJsonObject{{"x",p.roiX},{"y",p.roiY},{"width",p.roiWidth},{"height",p.roiHeight}}},
            {"autofocus",QJsonObject{{"beforeFirstRun",p.autofocus.beforeFirstRun},{"everyNRuns",p.autofocus.everyNRuns},
                                      {"rangeSteps",p.autofocus.request.rangeSteps},{"coarseStep",p.autofocus.request.coarseStep},{"fineStep",p.autofocus.request.fineStep},
                                      {"framesPerPosition",p.autofocus.request.framesPerPosition},{"settleMs",p.autofocus.request.settleMs},
                                      {"exposureSec",p.autofocus.request.exposureSec},{"gain",p.autofocus.request.gain}}},
            {"tracking",QJsonObject{{"allowRoiShift",p.tracking.allowRoiShift},{"roiShiftThresholdPx",p.tracking.roiShiftThresholdPx},
                                     {"mountCorrections",p.tracking.mountCorrections},{"mountCorrectionThresholdPx",p.tracking.mountCorrectionThresholdPx},
                                     {"autoCalibrateMount",p.tracking.autoCalibrateMount},{"calibrationArcsec",p.tracking.calibrationArcsec},
                                     {"maxMountCorrectionArcsec",p.tracking.maxMountCorrectionArcsec},{"mountSettleMs",p.tracking.mountSettleMs},
                                     {"lostTargetFrames",p.tracking.lostTargetFrames}}}};
    }
    return root;
}

inline ObservationBlock observationBlockFromJson(const QJsonObject &o) {
    ObservationBlock b;b.id=o.value("id").toString();b.name=o.value("name").toString();
    b.coordinate.raDeg=o.value("raDeg").toDouble();b.coordinate.decDeg=o.value("decDeg").toDouble();
    b.coordinate.frame=o.value("coordinateFrame").toString("J2000").trimmed().toUpper()=="JNOW"?EquatorialFrame::JNow:EquatorialFrame::J2000;
    b.mode=observationModeFromString(o.value("mode").toString("dso-fits"));
    if(b.mode==ObservationMode::DsoFits){
        const auto d=o.value("dso").toObject();b.dso.exposure=exposureRequestFromJson(d.value("exposure").toObject(),b.dso.exposure);b.dso.exposure.saveRaw=true;
        b.dso.frameCount=std::max(1,d.value("frameCount").toInt(b.dso.frameCount));b.dso.filter=d.value("filter").toString();
        const auto r=d.value("recenter").toObject();if(!r.isEmpty()){b.dso.recenter.beforeFirstFrame=r.value("beforeFirstFrame").toBool(b.dso.recenter.beforeFirstFrame);b.dso.recenter.everyNFrames=std::max(0,r.value("everyNFrames").toInt(b.dso.recenter.everyNFrames));b.dso.recenter.toleranceArcmin=std::max(0.1,r.value("toleranceArcmin").toDouble(b.dso.recenter.toleranceArcmin));b.dso.recenter.maxAttempts=std::clamp(r.value("maxAttempts").toInt(b.dso.recenter.maxAttempts),0,10);b.dso.recenter.solveExposureSec=std::clamp(r.value("solveExposureSec").toDouble(b.dso.recenter.solveExposureSec),0.001,30.0);}
        const auto a=d.value("autofocus").toObject();if(!a.isEmpty()){b.dso.autofocus.beforeFirstFrame=a.value("beforeFirstFrame").toBool(b.dso.autofocus.beforeFirstFrame);b.dso.autofocus.everyNFrames=std::max(0,a.value("everyNFrames").toInt(b.dso.autofocus.everyNFrames));const QString m=a.value("mode").toString("stars").toLower();b.dso.autofocus.request.mode=m=="planet"?AutofocusMode::Planet:m=="scene"?AutofocusMode::Scene:m=="bahtinov"?AutofocusMode::Bahtinov:AutofocusMode::Stars;b.dso.autofocus.request.rangeSteps=a.value("rangeSteps").toInt(b.dso.autofocus.request.rangeSteps);b.dso.autofocus.request.coarseStep=a.value("coarseStep").toInt(b.dso.autofocus.request.coarseStep);b.dso.autofocus.request.fineStep=a.value("fineStep").toInt(b.dso.autofocus.request.fineStep);b.dso.autofocus.request.framesPerPosition=a.value("framesPerPosition").toInt(b.dso.autofocus.request.framesPerPosition);b.dso.autofocus.request.settleMs=a.value("settleMs").toInt(b.dso.autofocus.request.settleMs);b.dso.autofocus.request.exposureSec=a.value("exposureSec").toDouble(b.dso.autofocus.request.exposureSec);b.dso.autofocus.request.gain=a.value("gain").toInt(b.dso.autofocus.request.gain);b.dso.autofocus.request.minStars=a.value("minStars").toInt(b.dso.autofocus.request.minStars);}
    }else{
        const auto p=o.value("planetary").toObject();b.planetary.serRuns=std::max(1,p.value("serRuns").toInt(b.planetary.serRuns));b.planetary.durationSec=std::max(0.1,p.value("durationSec").toDouble(b.planetary.durationSec));b.planetary.pauseSec=std::max(0.0,p.value("pauseSec").toDouble(b.planetary.pauseSec));b.planetary.trackingRate=trackingRateFromString(p.value("trackingRate").toString("sidereal"));
        const auto st=p.value("stream").toObject();if(!st.isEmpty()){b.planetary.stream.exposureSec=st.value("exposureSec").toDouble(b.planetary.stream.exposureSec);b.planetary.stream.gain=st.value("gain").toInt(b.planetary.stream.gain);b.planetary.stream.offset=st.value("offset").toInt(b.planetary.stream.offset);b.planetary.stream.binX=st.value("binX").toInt(b.planetary.stream.binX);b.planetary.stream.binY=st.value("binY").toInt(b.planetary.stream.binY);b.planetary.stream.targetFps=st.value("targetFps").toDouble(b.planetary.stream.targetFps);b.planetary.stream.debayer=st.value("debayer").toBool(b.planetary.stream.debayer);b.planetary.stream.bayerPattern=bayerPatternFromString(st.value("bayerPattern").toString("AUTO"));}
        const auto roi=p.value("roi").toObject();b.planetary.roiX=roi.value("x").toInt();b.planetary.roiY=roi.value("y").toInt();b.planetary.roiWidth=roi.value("width").toInt();b.planetary.roiHeight=roi.value("height").toInt();
        const auto a=p.value("autofocus").toObject();if(!a.isEmpty()){b.planetary.autofocus.beforeFirstRun=a.value("beforeFirstRun").toBool(b.planetary.autofocus.beforeFirstRun);b.planetary.autofocus.everyNRuns=std::max(0,a.value("everyNRuns").toInt(b.planetary.autofocus.everyNRuns));b.planetary.autofocus.request.mode=AutofocusMode::Planet;b.planetary.autofocus.request.rangeSteps=a.value("rangeSteps").toInt(b.planetary.autofocus.request.rangeSteps);b.planetary.autofocus.request.coarseStep=a.value("coarseStep").toInt(b.planetary.autofocus.request.coarseStep);b.planetary.autofocus.request.fineStep=a.value("fineStep").toInt(b.planetary.autofocus.request.fineStep);b.planetary.autofocus.request.framesPerPosition=a.value("framesPerPosition").toInt(b.planetary.autofocus.request.framesPerPosition);b.planetary.autofocus.request.settleMs=a.value("settleMs").toInt(b.planetary.autofocus.request.settleMs);b.planetary.autofocus.request.exposureSec=a.value("exposureSec").toDouble(b.planetary.autofocus.request.exposureSec);b.planetary.autofocus.request.gain=a.value("gain").toInt(b.planetary.autofocus.request.gain);}
        const auto tr=p.value("tracking").toObject();if(!tr.isEmpty()){b.planetary.tracking.allowRoiShift=tr.value("allowRoiShift").toBool(b.planetary.tracking.allowRoiShift);b.planetary.tracking.roiShiftThresholdPx=std::max(4,tr.value("roiShiftThresholdPx").toInt(b.planetary.tracking.roiShiftThresholdPx));b.planetary.tracking.mountCorrections=tr.value("mountCorrections").toBool(b.planetary.tracking.mountCorrections);b.planetary.tracking.mountCorrectionThresholdPx=std::max(b.planetary.tracking.roiShiftThresholdPx+4,tr.value("mountCorrectionThresholdPx").toInt(b.planetary.tracking.mountCorrectionThresholdPx));b.planetary.tracking.autoCalibrateMount=tr.value("autoCalibrateMount").toBool(b.planetary.tracking.autoCalibrateMount);b.planetary.tracking.calibrationArcsec=std::max(1.0,tr.value("calibrationArcsec").toDouble(b.planetary.tracking.calibrationArcsec));b.planetary.tracking.maxMountCorrectionArcsec=std::max(1.0,tr.value("maxMountCorrectionArcsec").toDouble(b.planetary.tracking.maxMountCorrectionArcsec));b.planetary.tracking.mountSettleMs=std::max(0,tr.value("mountSettleMs").toInt(b.planetary.tracking.mountSettleMs));b.planetary.tracking.lostTargetFrames=std::max(1,tr.value("lostTargetFrames").toInt(b.planetary.tracking.lostTargetFrames));}
    }
    return b;
}

inline QJsonObject observationPlanToJson(const ObservationPlan &p) {
    QJsonArray blocks;for(const auto &b:p.blocks)blocks.append(observationBlockToJson(b));
    return QJsonObject{{"id",p.id},{"name",p.name},{"blocks",blocks}};
}

inline ObservationPlan observationPlanFromJson(const QJsonObject &o) {
    ObservationPlan p;p.id=o.value("id").toString();p.name=o.value("name").toString("OAL observing plan");
    for(const auto &v:o.value("blocks").toArray())if(v.isObject())p.blocks.push_back(observationBlockFromJson(v.toObject()));
    return p;
}

inline ObservationBlock observationBlockFromLegacy(const SessionTarget &t, int index = 0) {
    ObservationBlock b;b.id=QString("legacy-%1").arg(index+1);b.name=t.name;b.coordinate=t.coordinate;b.mode=ObservationMode::DsoFits;
    b.dso.exposure.exposureSec=t.exposureSec;b.dso.exposure.saveRaw=true;b.dso.frameCount=std::max(1,t.repeats);b.dso.filter=t.filter;
    // The legacy wire model had no solve/autofocus intent. Preserve that
    // behavior instead of unexpectedly requiring a solver/focuser for old clients.
    b.dso.recenter.beforeFirstFrame=false;b.dso.autofocus.beforeFirstFrame=false;return b;
}

inline QJsonObject sessionStatusToJson(const SessionStatus &s) {
    return QJsonObject{{"id",s.id},{"name",s.name},{"active",s.active},{"blockIndex",s.blockIndex},{"blockCount",s.blockCount},
        {"targetIndex",s.targetIndex},{"targetCount",s.targetCount},{"completedFrames",s.completedFrames},{"currentBlockCompletedFrames",s.currentBlockCompletedFrames},
        {"currentBlockId",s.currentBlockId},{"currentBlockName",s.currentBlockName},{"currentStep",s.currentStep},{"currentOperationId",s.currentOperationId},
        {"lastError",s.lastError},{"state",s.state}};
}

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
